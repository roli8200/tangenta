#include "DocumentIO.h"

#include "ImageObject.h"
#include "Page.h"
#include "ImagePool.h"
#include "Serialization.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

#include <miniz.h>

namespace vek::io {

namespace {

const char* const kMimetypeEntry = "mimetype";
const char* const kManifestEntry = "manifest.json";
const char* const kDocumentEntry = "document.json";
const QString kImagePrefix = QStringLiteral("images/");

// Schlüssel aller Bilder im Baum, jeder nur einmal
void collectImages(const Node& node, QStringList& keys)
{
    if (const auto* image = node_cast<ImageObject>(&node); image && !keys.contains(image->key()))
        keys << image->key();
    // Bitmap-Muster (Füllung, Mustertransparenz)
    if (const auto* o = node_cast<DrawObject>(&node)) {
        for (const PatternFill* p : {&o->style().fill.pattern, &o->transparency().pattern})
            if (p->kind == PatternFill::Kind::Bitmap && !p->imageKey.isEmpty() && !keys.contains(p->imageKey))
                keys << p->imageKey;
    }
    // Eingebettetes Hintergrundbild (Optionen > Dokument > Hintergrund)
    if (const auto* page = node_cast<Page>(&node)) {
        const QString bg = page->options().value(QStringLiteral("doc/background/bitmap")).toString();
        if (bg.startsWith(QLatin1String("pool:")) && !keys.contains(bg.mid(5)))
            keys << bg.mid(5);
    }
    for (int i = 0; i < node.childCount(); ++i)
        collectImages(*node.child(i), keys);
}

QString tr(const char* text)
{
    return QCoreApplication::translate("vek::io", text);
}

template <class T>
T fail(QString* error, const QString& msg)
{
    if (error)
        *error = msg;
    return T();
}

class ZipReader {
public:
    explicit ZipReader(const QByteArray& data)
    {
        mz_zip_zero_struct(&m_zip);
        m_ok = mz_zip_reader_init_mem(&m_zip, data.constData(), static_cast<size_t>(data.size()), 0);
    }
    ~ZipReader()
    {
        if (m_ok)
            mz_zip_reader_end(&m_zip);
    }
    bool ok() const { return m_ok; }
    QStringList names()
    {
        QStringList list;
        const mz_uint count = mz_zip_reader_get_num_files(&m_zip);
        for (mz_uint i = 0; i < count; ++i) {
            char name[512];
            if (mz_zip_reader_get_filename(&m_zip, i, name, sizeof(name)) > 0)
                list << QString::fromUtf8(name);
        }
        return list;
    }
    bool read(const char* name, QByteArray& out)
    {
        size_t size = 0;
        void* p = mz_zip_reader_extract_file_to_heap(&m_zip, name, &size, 0);
        if (!p)
            return false;
        out = QByteArray(static_cast<const char*>(p), static_cast<qsizetype>(size));
        mz_free(p);
        return true;
    }

private:
    mz_zip_archive m_zip;
    bool m_ok = false;
};

} // namespace

QByteArray saveToBytes(const Document& doc, QString* error)
{
    QJsonObject manifest;
    manifest[QStringLiteral("format")] = QStringLiteral("vektoreditor");
    manifest[QStringLiteral("version")] = FormatVersion;
    manifest[QStringLiteral("generator")] =
        QStringLiteral("TANGENTA %1").arg(QCoreApplication::applicationVersion());

    QJsonObject body;
    body[QStringLiteral("unit")] = QStringLiteral("mm");
    body[QStringLiteral("root")] = serial::toJson(doc.root());

    const QByteArray manifestBytes = QJsonDocument(manifest).toJson(QJsonDocument::Indented);
    const QByteArray bodyBytes = QJsonDocument(body).toJson(QJsonDocument::Indented);

    mz_zip_archive zip;
    mz_zip_zero_struct(&zip);
    if (!mz_zip_writer_init_heap(&zip, 0, 64 * 1024))
        return fail<QByteArray>(error, tr("ZIP-Container konnte nicht angelegt werden."));
    // mimetype unkomprimiert und zuerst: Dateityp ist ohne Entpacken erkennbar (wie ODF)
    bool ok = mz_zip_writer_add_mem(&zip, kMimetypeEntry, MimeType, sizeof(MimeType) - 1, MZ_NO_COMPRESSION)
              && mz_zip_writer_add_mem(&zip, kManifestEntry, manifestBytes.constData(),
                                       static_cast<size_t>(manifestBytes.size()), MZ_DEFAULT_COMPRESSION)
              && mz_zip_writer_add_mem(&zip, kDocumentEntry, bodyBytes.constData(),
                                       static_cast<size_t>(bodyBytes.size()), MZ_DEFAULT_COMPRESSION);
    // Bilder unverändert (JPEG bleibt JPEG) und ohne erneute Kompression
    QStringList images;
    collectImages(doc.root(), images);
    for (const QString& key : std::as_const(images)) {
        const QByteArray bytes = imagepool::encoded(key);
        if (bytes.isEmpty())
            continue;
        const QByteArray name = (kImagePrefix + key + QLatin1Char('.') + imagepool::suffix(key)).toUtf8();
        ok = ok && mz_zip_writer_add_mem(&zip, name.constData(), bytes.constData(), static_cast<size_t>(bytes.size()),
                                         MZ_NO_COMPRESSION);
    }
    void* buffer = nullptr;
    size_t size = 0;
    ok = ok && mz_zip_writer_finalize_heap_archive(&zip, &buffer, &size);
    QByteArray result;
    if (ok)
        result = QByteArray(static_cast<const char*>(buffer), static_cast<qsizetype>(size));
    mz_zip_writer_end(&zip);
    if (buffer)
        mz_free(buffer);
    if (!ok)
        return fail<QByteArray>(error, tr("ZIP-Container konnte nicht geschrieben werden."));
    return result;
}

std::unique_ptr<Document> loadFromBytes(const QByteArray& data, QString* error)
{
    using Result = std::unique_ptr<Document>;
    ZipReader zip(data);
    if (!zip.ok())
        return fail<Result>(error, tr("Die Datei ist kein gültiger Container."));

    QByteArray mime;
    if (!zip.read(kMimetypeEntry, mime) || mime != MimeType)
        return fail<Result>(error, tr("Die Datei ist kein TANGENTA-Dokument."));

    QByteArray manifestBytes;
    if (!zip.read(kManifestEntry, manifestBytes))
        return fail<Result>(error, tr("manifest.json fehlt."));
    const QJsonObject manifest = QJsonDocument::fromJson(manifestBytes).object();
    const int version = manifest.value(QStringLiteral("version")).toInt(0);
    if (version < 1)
        return fail<Result>(error, tr("Unbekannte Formatversion."));
    if (version > FormatVersion)
        return fail<Result>(error, tr("Die Datei stammt aus einer neueren Programmversion (Format %1).").arg(version));

    QByteArray bodyBytes;
    if (!zip.read(kDocumentEntry, bodyBytes))
        return fail<Result>(error, tr("document.json fehlt."));
    QJsonParseError parseError;
    const QJsonObject body = QJsonDocument::fromJson(bodyBytes, &parseError).object();
    if (parseError.error != QJsonParseError::NoError)
        return fail<Result>(error, tr("document.json ist beschädigt: %1").arg(parseError.errorString()));
    if (body.value(QStringLiteral("unit")).toString() != QLatin1String("mm"))
        return fail<Result>(error, tr("Nicht unterstützte Masseinheit."));

    // Bilder zuerst in den Bildpool; der Schlüssel ergibt sich wieder aus dem Inhalt
    for (const QString& name : zip.names()) {
        QByteArray bytes;
        if (name.startsWith(kImagePrefix) && zip.read(name.toUtf8().constData(), bytes))
            imagepool::add(bytes);
    }

    QString treeError;
    std::unique_ptr<Node> root = serial::fromJson(body.value(QStringLiteral("root")).toObject(), serial::Ids::Keep, &treeError);
    if (!root)
        return fail<Result>(error, treeError);
    return Document::fromTree(std::move(root), error);
}

bool save(const Document& doc, const QString& path, QString* error)
{
    const QByteArray bytes = saveToBytes(doc, error);
    if (bytes.isEmpty())
        return false;
    // QSaveFile schreibt in eine Hilfsdatei und ersetzt erst am Ende: kein halbes Dokument bei Absturz
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        return fail<bool>(error, file.errorString());
    return true;
}

std::unique_ptr<Document> load(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail<std::unique_ptr<Document>>(error, file.errorString());
    return loadFromBytes(file.readAll(), error);
}

} // namespace vek::io

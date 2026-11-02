#include "PdfSettings.h"

#include <QJsonDocument>
#include <QObject>
#include <QRegularExpression>
#include <QSettings>

#include <algorithm>

namespace vek {

QString PdfSettings::compatibilityLevel() const
{
    switch (compatibility) {
    case Compatibility::Acrobat4: return QStringLiteral("1.3");
    case Compatibility::Acrobat5: return QStringLiteral("1.4");
    case Compatibility::Acrobat6: return QStringLiteral("1.5");
    case Compatibility::Acrobat7: return QStringLiteral("1.6");
    case Compatibility::Acrobat8:
    case Compatibility::Acrobat9: return QStringLiteral("1.7");
    case Compatibility::PdfX1a: return QStringLiteral("1.3");
    case Compatibility::PdfX3: return QStringLiteral("1.3");
    case Compatibility::PdfA1b: return QStringLiteral("1.4");
    case Compatibility::PdfA2b: return QStringLiteral("1.7");
    }
    return QStringLiteral("1.5");
}

QJsonObject PdfSettings::toJson() const
{
    QJsonObject o;
    o[QStringLiteral("range")] = int(range);
    o[QStringLiteral("pages")] = pages;
    o[QStringLiteral("sizeFromSelection")] = sizeFromSelection;
    o[QStringLiteral("compatibility")] = int(compatibility);
    o[QStringLiteral("colors")] = int(colors);
    o[QStringLiteral("useProofSettings")] = useProofSettings;
    o[QStringLiteral("embedProfile")] = embedProfile;
    o[QStringLiteral("keepOverprint")] = keepOverprint;
    o[QStringLiteral("author")] = author;
    o[QStringLiteral("keywords")] = keywords;
    o[QStringLiteral("subject")] = subject;
    o[QStringLiteral("fastWebView")] = fastWebView;
    o[QStringLiteral("bookmarks")] = bookmarks;
    o[QStringLiteral("thumbnails")] = thumbnails;
    o[QStringLiteral("startView")] = int(startView);
    o[QStringLiteral("ascii85")] = ascii85;
    o[QStringLiteral("compression")] = int(compression);
    o[QStringLiteral("jpegQuality")] = jpegQuality;
    o[QStringLiteral("downsampleColor")] = downsampleColor;
    o[QStringLiteral("colorDpi")] = colorDpi;
    o[QStringLiteral("downsampleGray")] = downsampleGray;
    o[QStringLiteral("grayDpi")] = grayDpi;
    o[QStringLiteral("downsampleMono")] = downsampleMono;
    o[QStringLiteral("monoDpi")] = monoDpi;
    o[QStringLiteral("compressText")] = compressText;
    o[QStringLiteral("textAsCurves")] = textAsCurves;
    o[QStringLiteral("embedFonts")] = embedFonts;
    o[QStringLiteral("embedBase14")] = embedBase14;
    o[QStringLiteral("subsetFonts")] = subsetFonts;
    o[QStringLiteral("subsetBelow")] = subsetBelow;
    o[QStringLiteral("complexFillsAsBitmaps")] = complexFillsAsBitmaps;
    o[QStringLiteral("bleedLimit")] = bleedLimit;
    o[QStringLiteral("bleed")] = bleed;
    o[QStringLiteral("cropMarks")] = cropMarks;
    o[QStringLiteral("registrationMarks")] = registrationMarks;
    o[QStringLiteral("fileInfo")] = fileInfo;
    o[QStringLiteral("densitometer")] = densitometer;
    // Kennwörter werden nie gespeichert, nur ob sie verlangt sind
    o[QStringLiteral("openPassword")] = openPassword;
    o[QStringLiteral("permissionPassword")] = permissionPassword;
    o[QStringLiteral("printing")] = int(printing);
    o[QStringLiteral("changes")] = int(changes);
    o[QStringLiteral("allowCopy")] = allowCopy;
    o[QStringLiteral("preflightIgnore")] = preflightIgnore;
    return o;
}

PdfSettings PdfSettings::fromJson(const QJsonObject& o)
{
    PdfSettings s;
    auto i = [&](const char* key, int def) { return o.value(QLatin1String(key)).toInt(def); };
    auto b = [&](const char* key, bool def) { return o.value(QLatin1String(key)).toBool(def); };
    s.range = Range(std::clamp(i("range", int(s.range)), 0, 3));
    s.pages = o.value(QStringLiteral("pages")).toString(s.pages);
    s.sizeFromSelection = b("sizeFromSelection", s.sizeFromSelection);
    s.compatibility = Compatibility(std::clamp(i("compatibility", int(s.compatibility)), 0, 9));
    s.colors = Colors(std::clamp(i("colors", int(s.colors)), 0, 3));
    s.useProofSettings = b("useProofSettings", s.useProofSettings);
    s.embedProfile = b("embedProfile", s.embedProfile);
    s.keepOverprint = b("keepOverprint", s.keepOverprint);
    s.author = o.value(QStringLiteral("author")).toString();
    s.keywords = o.value(QStringLiteral("keywords")).toString();
    s.subject = o.value(QStringLiteral("subject")).toString();
    s.fastWebView = b("fastWebView", s.fastWebView);
    s.bookmarks = b("bookmarks", s.bookmarks);
    s.thumbnails = b("thumbnails", s.thumbnails);
    s.startView = StartView(std::clamp(i("startView", int(s.startView)), 0, 3));
    s.ascii85 = b("ascii85", s.ascii85);
    s.compression = Compression(std::clamp(i("compression", int(s.compression)), 0, 2));
    s.jpegQuality = std::clamp(i("jpegQuality", s.jpegQuality), 0, 100);
    s.downsampleColor = b("downsampleColor", s.downsampleColor);
    s.colorDpi = i("colorDpi", s.colorDpi);
    s.downsampleGray = b("downsampleGray", s.downsampleGray);
    s.grayDpi = i("grayDpi", s.grayDpi);
    s.downsampleMono = b("downsampleMono", s.downsampleMono);
    s.monoDpi = i("monoDpi", s.monoDpi);
    s.compressText = b("compressText", s.compressText);
    s.textAsCurves = b("textAsCurves", s.textAsCurves);
    s.embedFonts = b("embedFonts", s.embedFonts);
    s.embedBase14 = b("embedBase14", s.embedBase14);
    s.subsetFonts = b("subsetFonts", s.subsetFonts);
    s.subsetBelow = std::clamp(i("subsetBelow", s.subsetBelow), 1, 100);
    s.complexFillsAsBitmaps = b("complexFillsAsBitmaps", s.complexFillsAsBitmaps);
    s.bleedLimit = b("bleedLimit", s.bleedLimit);
    s.bleed = o.value(QStringLiteral("bleed")).toDouble(s.bleed);
    s.cropMarks = b("cropMarks", s.cropMarks);
    s.registrationMarks = b("registrationMarks", s.registrationMarks);
    s.fileInfo = b("fileInfo", s.fileInfo);
    s.densitometer = b("densitometer", s.densitometer);
    s.openPassword = b("openPassword", s.openPassword);
    s.permissionPassword = b("permissionPassword", s.permissionPassword);
    s.printing = Printing(std::clamp(i("printing", int(s.printing)), 0, 2));
    s.changes = Changes(std::clamp(i("changes", int(s.changes)), 0, 4));
    s.allowCopy = b("allowCopy", s.allowCopy);
    s.preflightIgnore = b("preflightIgnore", s.preflightIgnore);
    return s;
}

QStringList PdfSettings::builtinPresets()
{
    return {QObject::tr("Druckvorstufe"), QObject::tr("Web"),     QObject::tr("Dokumentverteilung"), QObject::tr("Bearbeitung"),
            QObject::tr("PDF/X-1a"),      QObject::tr("PDF/X-3"), QObject::tr("Archivieren (CMYK)"), QObject::tr("Archivieren (RGB)")};
}

QStringList PdfSettings::presets()
{
    QStringList list = builtinPresets();
    QSettings st;
    st.beginGroup(QStringLiteral("pdf/presets"));
    for (const QString& key : st.childKeys())
        if (!list.contains(key))
            list << key;
    return list;
}

bool PdfSettings::isBuiltin(const QString& name)
{
    return builtinPresets().contains(name);
}

PdfSettings PdfSettings::preset(const QString& name)
{
    PdfSettings s; // Dokumentverteilung
    const QStringList b = builtinPresets();
    const int index = int(b.indexOf(name));
    switch (index) {
    case 0: // Druckvorstufe: CMYK, volle Auflösung, Anschnitt und Schnittmarken
        s.colors = Colors::Cmyk;
        s.compression = Compression::Jpeg;
        s.jpegQuality = 100;
        s.downsampleColor = s.downsampleGray = true;
        s.colorDpi = s.grayDpi = 300;
        s.monoDpi = 1200;
        s.bleedLimit = true;
        s.cropMarks = true;
        s.subsetFonts = true;
        s.bookmarks = false;
        break;
    case 1: // Web: klein, RGB, schnelle Webanzeige
        s.colors = Colors::Rgb;
        s.jpegQuality = 70;
        s.colorDpi = s.grayDpi = 96;
        s.monoDpi = 300;
        s.fastWebView = true;
        break;
    case 2: // Dokumentverteilung
        break;
    case 3: // Bearbeitung: verlustfrei, ganze Schriften
        s.compression = Compression::Zip;
        s.downsampleColor = s.downsampleGray = s.downsampleMono = false;
        s.subsetFonts = false;
        break;
    case 4: // PDF/X-1a: CMYK, keine Transparenz
        s.compatibility = Compatibility::PdfX1a;
        s.colors = Colors::Cmyk;
        s.jpegQuality = 100;
        s.colorDpi = s.grayDpi = 300;
        s.monoDpi = 1200;
        s.bleedLimit = true;
        s.cropMarks = true;
        s.bookmarks = false;
        break;
    case 5: // PDF/X-3: Farbe mit Ausgabebedingung
        s.compatibility = Compatibility::PdfX3;
        s.colors = Colors::Cmyk;
        s.jpegQuality = 100;
        s.colorDpi = s.grayDpi = 300;
        s.monoDpi = 1200;
        s.bleedLimit = true;
        s.cropMarks = true;
        s.bookmarks = false;
        break;
    case 6: // Archivieren (CMYK)
        s.compatibility = Compatibility::PdfA1b;
        s.colors = Colors::Cmyk;
        s.compression = Compression::Zip;
        break;
    case 7: // Archivieren (RGB)
        s.compatibility = Compatibility::PdfA1b;
        s.colors = Colors::Rgb;
        s.compression = Compression::Zip;
        break;
    default: {
        QSettings st;
        const QByteArray json = st.value(QStringLiteral("pdf/presets/") + name).toByteArray();
        s = fromJson(QJsonDocument::fromJson(json).object());
        break;
    }
    }
    return s;
}

void PdfSettings::savePreset(const QString& name, const PdfSettings& settings)
{
    if (name.trimmed().isEmpty() || isBuiltin(name))
        return;
    QSettings().setValue(QStringLiteral("pdf/presets/") + name, QJsonDocument(settings.toJson()).toJson(QJsonDocument::Compact));
}

void PdfSettings::removePreset(const QString& name)
{
    if (!isBuiltin(name))
        QSettings().remove(QStringLiteral("pdf/presets/") + name);
}

PdfSettings PdfSettings::last(QString* presetName)
{
    QSettings st;
    const QString name = st.value(QStringLiteral("pdf/lastPreset"), QObject::tr("Dokumentverteilung")).toString();
    if (presetName)
        *presetName = name;
    const QByteArray json = st.value(QStringLiteral("pdf/last")).toByteArray();
    if (json.isEmpty())
        return preset(name);
    return fromJson(QJsonDocument::fromJson(json).object());
}

void PdfSettings::remember(const PdfSettings& settings, const QString& presetName)
{
    QSettings st;
    st.setValue(QStringLiteral("pdf/last"), QJsonDocument(settings.toJson()).toJson(QJsonDocument::Compact));
    st.setValue(QStringLiteral("pdf/lastPreset"), presetName);
}

QList<int> parsePageList(const QString& text, int pageCount)
{
    QList<int> out;
    static const QRegularExpression part(QStringLiteral(R"(^\s*(\d*)\s*(-?)\s*(\d*)\s*$)"));
    for (const QString& item : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QRegularExpressionMatch m = part.match(item);
        if (!m.hasMatch() || (m.captured(1).isEmpty() && m.captured(3).isEmpty()))
            continue;
        int from = m.captured(1).isEmpty() ? 1 : m.captured(1).toInt();
        int to = m.captured(2).isEmpty() ? from : m.captured(3).isEmpty() ? pageCount : m.captured(3).toInt();
        from = std::clamp(from, 1, pageCount);
        to = std::clamp(to, 1, pageCount);
        for (int p = std::min(from, to); p <= std::max(from, to); ++p)
            if (!out.contains(p - 1))
                out << p - 1;
    }
    return out;
}

} // namespace vek

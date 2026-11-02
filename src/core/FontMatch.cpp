#include "FontMatch.h"

#include "TextObject.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QRawFont>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

namespace vek::fontmatch {

namespace {

// Zeitpunkt des Programmstarts: danach installierte Schriften lädt reloadInstalledFonts() nach
const QDateTime kStarted = QDateTime::currentDateTimeUtc();
int g_generation = 0;

// Normalisierter Name -> installierte Familie
const QHash<QString, QString>& installedIndex()
{
    static QHash<QString, QString> index;
    static int built = -1;
    if (built != g_generation) {
        index.clear();
        for (const QString& f : QFontDatabase::families()) {
            if (QFontDatabase::isPrivateFamily(f))
                continue;
            index.insert(normalized(f), f);
        }
        built = g_generation;
    }
    return index;
}

struct GoogleEntry {
    QString family;
    QString category;
};

const QHash<QString, GoogleEntry>& googleIndex()
{
    static const QHash<QString, GoogleEntry> index = [] {
        QHash<QString, GoogleEntry> h;
        QFile f(QStringLiteral(":/fonts/googlefonts.tsv"));
        if (f.open(QIODevice::ReadOnly))
            for (const QByteArray& line : f.readAll().split('\n')) {
                const QList<QByteArray> cols = line.split('\t');
                if (cols.size() >= 2)
                    h.insert(normalized(QString::fromUtf8(cols[0])), GoogleEntry{QString::fromUtf8(cols[0]), QString::fromUtf8(cols[1])});
            }
        return h;
    }();
    return index;
}

QUrl googleUrl(const QString& family)
{
    return QUrl(QStringLiteral("https://fonts.google.com/specimen/") + QString(family).replace(QLatin1Char(' '), QLatin1Char('+')));
}

// Umriss in ein kleines Raster (Geviert-Koordinaten, y nach unten, linke Kante bei 0)
QImage raster(const QPainterPath& yDown)
{
    static constexpr int W = 40, H = 52;
    QImage img(W, H, QImage::Format_Grayscale8);
    img.fill(0);
    if (yDown.isEmpty())
        return img;
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, false);
    // x: 0..1000, y: -1000..300 (Oberlänge bis Unterlänge)
    p.scale(W / 1000.0, H / 1300.0);
    p.translate(-yDown.boundingRect().left(), 1000);
    p.fillPath(yDown, Qt::white);
    return img;
}

double overlap(const QImage& a, const QImage& b)
{
    int both = 0, any = 0;
    for (int y = 0; y < a.height(); ++y) {
        const uchar* la = a.constScanLine(y);
        const uchar* lb = b.constScanLine(y);
        for (int x = 0; x < a.width(); ++x) {
            const bool pa = la[x] > 127, pb = lb[x] > 127;
            both += pa && pb;
            any += pa || pb;
        }
    }
    return any ? double(both) / any : 1.0;
}

} // namespace

QString normalized(const QString& family)
{
    QString s = family.toLower();
    s.remove(QRegularExpression(QStringLiteral("[\\s\\-_,]")));
    for (const char* suffix : {"psmt", "mt", "ps"})
        if (s.endsWith(QLatin1String(suffix)) && s.size() > int(qstrlen(suffix)) + 2) {
            s.chop(int(qstrlen(suffix)));
            break;
        }
    return s;
}

QString familyFromBaseName(const QString& baseName, bool* bold, bool* italic)
{
    QString name = baseName;
    if (name.size() > 7 && name[6] == QLatin1Char('+'))
        name = name.mid(7);
    QString family = name, style;
    const qsizetype cut = std::max(name.indexOf(QLatin1Char('-')), name.indexOf(QLatin1Char(',')));
    if (cut > 0) {
        family = name.left(cut);
        style = name.mid(cut + 1);
    }
    // Schnitt am Ende des Familiennamens ("ArialBold", "TimesNewRomanPSMT")
    for (const char* suffix : {"PSMT", "MT", "PS"})
        if (family.endsWith(QLatin1String(suffix)) && family.size() > 4)
            family.chop(int(qstrlen(suffix)));
    // Am Namensende nur eindeutige Schnittwörter ("Roman" gehört zu "Times New Roman")
    static const QStringList suffixes{QStringLiteral("BoldItalic"), QStringLiteral("BoldOblique"), QStringLiteral("SemiBold"),
                                      QStringLiteral("Semibold"),   QStringLiteral("ExtraBold"),   QStringLiteral("Bold"),
                                      QStringLiteral("Italic"),     QStringLiteral("Oblique"),     QStringLiteral("Black"),
                                      QStringLiteral("Light"),      QStringLiteral("Medium")};
    for (const QString& w : suffixes)
        if (family.endsWith(w) && family.size() > w.size() + 2) {
            style.prepend(w);
            family.chop(w.size());
            break;
        }
    const QString st = style.toLower();
    if (bold)
        *bold = st.contains(QLatin1String("bold")) || st.contains(QLatin1String("black")) || st.contains(QLatin1String("heavy"))
                || st.contains(QLatin1String("demi"));
    if (italic)
        *italic = st.contains(QLatin1String("italic")) || st.contains(QLatin1String("oblique")) || st.endsWith(QLatin1String("it"));
    // Binnenmajuskeln trennen: "TimesNewRoman" -> "Times New Roman" (Abkürzungen bleiben)
    if (!family.contains(QLatin1Char(' '))) {
        static const QRegularExpression camel(QStringLiteral("(?<=[a-z])(?=[A-Z])"));
        family.replace(camel, QStringLiteral(" "));
    }
    return family.trimmed();
}

QString installedFamily(const QString& family)
{
    const QString key = normalized(family);
    const auto& index = installedIndex();
    if (const auto it = index.constFind(key); it != index.constEnd())
        return *it;
    // Die 14 PDF-Grundschriften auf ihre üblichen Gegenstücke
    static const QList<std::pair<QString, QStringList>> base14{
        {QStringLiteral("helvetica"), {QStringLiteral("arial"), QStringLiteral("liberationsans"), QStringLiteral("arimo")}},
        {QStringLiteral("arial"), {QStringLiteral("liberationsans"), QStringLiteral("arimo")}},
        {QStringLiteral("times"), {QStringLiteral("timesnewroman"), QStringLiteral("liberationserif"), QStringLiteral("tinos")}},
        {QStringLiteral("timesroman"), {QStringLiteral("timesnewroman"), QStringLiteral("liberationserif"), QStringLiteral("tinos")}},
        {QStringLiteral("timesnewroman"), {QStringLiteral("liberationserif"), QStringLiteral("tinos")}},
        {QStringLiteral("courier"), {QStringLiteral("couriernew"), QStringLiteral("liberationmono"), QStringLiteral("cousine")}},
        {QStringLiteral("couriernew"), {QStringLiteral("liberationmono"), QStringLiteral("cousine")}},
    };
    for (const auto& [name, alternatives] : base14)
        if (key == name)
            for (const QString& alt : alternatives)
                if (const auto it = index.constFind(alt); it != index.constEnd())
                    return *it;
    return {};
}

QList<Candidate> suggest(const ImportedFont& font, int count)
{
    // Vergleichszeichen: Breiten aller Zeichen, Formen bevorzugt von Buchstaben
    QList<char32_t> widthChars, shapeChars;
    for (auto it = font.widths.constBegin(); it != font.widths.constEnd(); ++it)
        if (it.key() > U' ' && it.value() > 0)
            widthChars << it.key();
    std::sort(widthChars.begin(), widthChars.end());
    for (auto it = font.glyphs.constBegin(); it != font.glyphs.constEnd(); ++it)
        if (!it.value().isEmpty() && QChar::isLetterOrNumber(it.key()))
            shapeChars << it.key();
    std::sort(shapeChars.begin(), shapeChars.end());
    if (shapeChars.size() > 16) {
        QList<char32_t> spread;
        for (int i = 0; i < 16; ++i)
            spread << shapeChars[i * shapeChars.size() / 16];
        shapeChars = spread;
    }
    QList<QImage> pdfShapes;
    for (char32_t c : std::as_const(shapeChars))
        pdfShapes << raster(QTransform(1, 0, 0, -1, 0, 0).map(font.glyphs.value(c)));
    const QString key = normalized(font.family);

    QList<Candidate> out;
    for (const QString& family : QFontDatabase::families(QFontDatabase::Latin)) {
        if (QFontDatabase::isPrivateFamily(family) || family.startsWith(QLatin1Char('@')))
            continue;
        QFont qf(family);
        qf.setPixelSize(1000);
        qf.setBold(font.bold);
        qf.setItalic(font.italic);
        qf.setStyleStrategy(QFont::NoFontMerging);
        const QRawFont raw = QRawFont::fromFont(qf, QFontDatabase::Latin);
        if (!raw.isValid())
            continue;
        Candidate c;
        c.family = family;
        if (!widthChars.isEmpty()) {
            double err = 0;
            int n = 0;
            for (char32_t ch : std::as_const(widthChars)) {
                const QString s = QString::fromUcs4(&ch, 1);
                const QList<quint32> idx = raw.glyphIndexesForString(s);
                if (idx.size() != 1 || idx[0] == 0) {
                    err += 1.0; // Zeichen fehlt
                    ++n;
                    continue;
                }
                const double adv = raw.advancesForGlyphIndexes(idx).value(0).x();
                const double w = font.widths.value(ch);
                err += std::min(1.0, std::abs(adv - w) / std::max(w, 250.0));
                ++n;
            }
            c.widths = std::clamp(1.0 - 2.5 * err / std::max(1, n), 0.0, 1.0);
        }
        if (!pdfShapes.isEmpty()) {
            double sum = 0;
            for (int i = 0; i < shapeChars.size(); ++i) {
                const QString s = QString::fromUcs4(&shapeChars[i], 1);
                const QList<quint32> idx = raw.glyphIndexesForString(s);
                if (idx.size() != 1 || idx[0] == 0)
                    continue;
                sum += overlap(pdfShapes[i], raster(raw.pathForGlyph(idx[0])));
            }
            c.shape = sum / shapeChars.size();
        }
        if (c.shape >= 0 && c.widths >= 0)
            c.score = 0.7 * c.shape + 0.3 * c.widths;
        else if (c.shape >= 0)
            c.score = c.shape;
        else if (c.widths >= 0)
            c.score = c.widths;
        else
            c.score = 0;
        // Gleicher oder enthaltener Name: kleiner Vorzug
        const QString ck = normalized(family);
        if (!key.isEmpty() && (ck.contains(key) || key.contains(ck)))
            c.score = std::min(1.0, c.score + 0.05);
        out << c;
    }
    std::sort(out.begin(), out.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
    if (out.size() > count)
        out.resize(count);
    return out;
}

std::optional<WebFont> googleFont(const QString& family)
{
    const auto& index = googleIndex();
    const auto it = index.constFind(normalized(family));
    if (it == index.constEnd())
        return std::nullopt;
    return WebFont{it->family, googleUrl(it->family), QObject::tr("bei Google Fonts (%1)").arg(it->category)};
}

QList<WebFont> freeAlternatives(const QString& family)
{
    // Metrisch gleiche freie Schriften: Zeilenfall bleibt beim Ersetzen erhalten
    static const QList<std::pair<QStringList, QStringList>> table{
        {{QStringLiteral("arial"), QStringLiteral("helvetica")}, {QStringLiteral("Arimo"), QStringLiteral("Liberation Sans")}},
        {{QStringLiteral("timesnewroman"), QStringLiteral("times"), QStringLiteral("timesroman")}, {QStringLiteral("Tinos"), QStringLiteral("Liberation Serif")}},
        {{QStringLiteral("couriernew"), QStringLiteral("courier")}, {QStringLiteral("Cousine"), QStringLiteral("Liberation Mono")}},
        {{QStringLiteral("calibri")}, {QStringLiteral("Carlito")}},
        {{QStringLiteral("cambria")}, {QStringLiteral("Caladea")}},
        {{QStringLiteral("georgia")}, {QStringLiteral("Gelasio")}},
        {{QStringLiteral("arialnarrow")}, {QStringLiteral("Liberation Sans Narrow")}},
        {{QStringLiteral("symbol")}, {QStringLiteral("Noto Sans Symbols")}},
    };
    QList<WebFont> out;
    const QString key = normalized(family);
    for (const auto& [names, alternatives] : table) {
        if (!names.contains(key))
            continue;
        for (const QString& alt : alternatives) {
            QUrl url = googleIndex().contains(normalized(alt)) ? googleUrl(alt)
                                                               : QUrl(QStringLiteral("https://github.com/liberationfonts/liberation-fonts/releases"));
            out << WebFont{alt, url, QObject::tr("metrisch gleich zu %1").arg(family)};
        }
    }
    return out;
}

QUrl searchUrl(const QString& family)
{
    QUrl url(QStringLiteral("https://www.google.com/search"));
    url.setQuery(QStringLiteral("q=%1").arg(QString::fromLatin1(QUrl::toPercentEncoding(QStringLiteral("\"%1\" font").arg(family)))));
    return url;
}

Choice defaultChoice(const ImportedFont& font, const QList<Candidate>& candidates)
{
    if (!font.installed.isEmpty())
        return {Choice::Kind::Family, font.installed};
    if (!candidates.isEmpty() && (candidates.first().score >= 0.85 || !font.embedded))
        return {Choice::Kind::Family, candidates.first().family};
    if (font.embedded)
        return {Choice::Kind::Curves, {}};
    return {Choice::Kind::Family, QStringLiteral("Arial")};
}

void placeOnBaseline(TextObject& text, const QTransform& baseline, double runWidthMm)
{
    if (text.paragraphs().isEmpty() || text.paragraphs().first().runs.isEmpty())
        return;
    const CharFormat& f = text.paragraphs().first().runs.first().format;
    QFont qf(f.family);
    qf.setPixelSize(std::max(1, int(std::lround(f.sizePt * 25.4 / 72.0 * TextObject::kUnitsPerMm))));
    qf.setBold(f.bold);
    qf.setItalic(f.italic);
    const double ascent = QFontMetricsF(qf).ascent() / TextObject::kUnitsPerMm;
    double sx = 1.0;
    if (runWidthMm > 0) {
        const double natural = text.frameRect().width();
        if (natural > 0)
            sx = std::clamp(runWidthMm / natural, 0.6, 1.6);
    }
    text.setTransform(QTransform::fromTranslate(0, -ascent) * QTransform::fromScale(sx, 1.0) * baseline);
}

void apply(ImportedFont& font, const Choice& choice)
{
    for (qsizetype i = 0; i < font.texts.size(); ++i) {
        TextObject* text = font.texts[i];
        if (!text)
            continue;
        if (choice.kind == Choice::Kind::Curves) {
            std::unique_ptr<DrawObject>& curve = font.curves[std::size_t(i)];
            if (!curve || !text->parent())
                continue;
            replaceDetachedChild(*text->parent(), text->indexInParent(), std::move(curve));
            font.texts[i] = nullptr;
            continue;
        }
        QList<TextParagraph> paragraphs = text->paragraphs();
        for (TextParagraph& p : paragraphs)
            for (TextRun& r : p.runs)
                r.format.family = choice.family;
        text->setParagraphs(paragraphs);
        if (i < font.baselines.size())
            placeOnBaseline(*text, font.baselines[i], font.runWidths.value(i));
    }
}

int reloadInstalledFonts()
{
    QStringList dirs;
#ifdef Q_OS_WIN
    dirs << qEnvironmentVariable("WINDIR", QStringLiteral("C:/Windows")) + QStringLiteral("/Fonts")
         << qEnvironmentVariable("LOCALAPPDATA") + QStringLiteral("/Microsoft/Windows/Fonts");
#else
    dirs << QDir::homePath() + QStringLiteral("/.local/share/fonts") << QDir::homePath() + QStringLiteral("/.fonts") << QStringLiteral("/usr/share/fonts")
         << QStringLiteral("/usr/local/share/fonts");
#endif
    static QSet<QString> loaded;
    const QStringList before = QFontDatabase::families();
    for (const QString& dir : std::as_const(dirs)) {
        QDirIterator it(dir, {QStringLiteral("*.ttf"), QStringLiteral("*.otf"), QStringLiteral("*.ttc"), QStringLiteral("*.TTF"), QStringLiteral("*.OTF")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QFileInfo info = it.fileInfo();
            if (loaded.contains(path) || (info.lastModified().toUTC() < kStarted && info.birthTime().toUTC() < kStarted))
                continue;
            loaded.insert(path);
            QFontDatabase::addApplicationFont(path);
        }
    }
    ++g_generation;
    return int(QFontDatabase::families().size() - before.size());
}

} // namespace vek::fontmatch

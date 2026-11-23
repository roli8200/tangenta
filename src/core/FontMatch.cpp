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

enum class Kind { Unknown, Sans, Serif, Mono, Script, Display, Symbol };

// Gewicht 100..900 und Breite 1..9 wie in der OS/2-Tabelle, 0 = unbekannt
struct Traits {
    Kind kind = Kind::Unknown;
    int weight = 0;
    int width = 0;
};

Traits traitsFromName(const QString& name)
{
    const QString n = normalized(name);
    Traits t;
    // Längere Wörter zuerst: "extrabold" vor "bold", "extralight" vor "light"
    static const std::pair<const char*, int> weights[] = {
        {"hairline", 100}, {"extralight", 200}, {"ultralight", 200}, {"semibold", 600}, {"demibold", 600}, {"extrabold", 800},
        {"ultrabold", 800}, {"thin", 100}, {"light", 300}, {"medium", 500}, {"heavy", 900}, {"black", 900},
        {"bold", 700}, {"demi", 600}, {"book", 400}, {"regular", 400}};
    for (const auto& [word, weight] : weights)
        if (n.contains(QLatin1String(word))) {
            t.weight = weight;
            break;
        }
    if (n.contains(QLatin1String("semicondensed")))
        t.width = 4;
    else if (n.contains(QLatin1String("condensed")) || n.contains(QLatin1String("narrow")) || n.contains(QLatin1String("compressed")))
        t.width = 3;
    else if (n.contains(QLatin1String("extended")) || n.contains(QLatin1String("expanded")) || n.contains(QLatin1String("wide")))
        t.width = 7;
    // Bekannte Familien, dann Wörter im Namen ("sans" vor "serif": "Sans Serif")
    static const std::pair<const char*, Kind> known[] = {
        {"centurygothic", Kind::Sans}, {"franklingothic", Kind::Sans}, {"helvetica", Kind::Sans}, {"arial", Kind::Sans},
        {"futura", Kind::Sans}, {"frutiger", Kind::Sans}, {"univers", Kind::Sans}, {"gillsans", Kind::Sans},
        {"optima", Kind::Sans}, {"avantgarde", Kind::Sans}, {"myriad", Kind::Sans}, {"verdana", Kind::Sans},
        {"tahoma", Kind::Sans}, {"segoe", Kind::Sans}, {"calibri", Kind::Sans}, {"roboto", Kind::Sans},
        {"courier", Kind::Mono}, {"consolas", Kind::Mono}, {"lucidaconsole", Kind::Mono}, {"mono", Kind::Mono},
        {"typewriter", Kind::Mono}, {"comicsans", Kind::Script}, {"brushscript", Kind::Script}, {"zapfchancery", Kind::Script},
        {"script", Kind::Script}, {"hand", Kind::Script}, {"brush", Kind::Script}, {"callig", Kind::Script},
        {"parkavenue", Kind::Script}, {"wingdings", Kind::Symbol}, {"webdings", Kind::Symbol}, {"symbol", Kind::Symbol}, {"dingbat", Kind::Symbol},
        {"times", Kind::Serif}, {"garamond", Kind::Serif}, {"bodoni", Kind::Serif}, {"baskerville", Kind::Serif},
        {"palatino", Kind::Serif}, {"georgia", Kind::Serif}, {"minion", Kind::Serif}, {"caslon", Kind::Serif},
        {"cambria", Kind::Serif}, {"bookman", Kind::Serif}, {"century", Kind::Serif}, {"rockwell", Kind::Serif},
        {"clarendon", Kind::Serif}, {"didot", Kind::Serif}, {"goudy", Kind::Serif}, {"cooper", Kind::Serif},
        {"sans", Kind::Sans}, {"grotesk", Kind::Sans}, {"gothic", Kind::Sans}, {"serif", Kind::Serif},
        {"antiqua", Kind::Serif}, {"roman", Kind::Serif}};
    for (const auto& [word, kind] : known)
        if (n.contains(QLatin1String(word))) {
            t.kind = kind;
            break;
        }
    return t;
}

Kind googleKind(const QString& category)
{
    if (category == QLatin1String("Sans Serif"))
        return Kind::Sans;
    if (category == QLatin1String("Serif"))
        return Kind::Serif;
    if (category == QLatin1String("Monospace"))
        return Kind::Mono;
    if (category == QLatin1String("Handwriting"))
        return Kind::Script;
    if (category == QLatin1String("Display"))
        return Kind::Display;
    return Kind::Unknown;
}

// Installierte Schrift: Gewicht und Breite aus der OS/2-Tabelle; die Art aus dem Namen, wenn er sie
// nennt (manche Schriften tragen falsche PANOSE-Werte), sonst aus PANOSE und Schriftklasse
Traits traitsFromFont(const QRawFont& raw, const QString& family)
{
    Traits t = traitsFromName(family);
    const QByteArray os2 = raw.fontTable("OS/2");
    if (os2.size() < 42)
        return t;
    auto u16 = [&os2](int at) { return int(quint8(os2[at])) << 8 | int(quint8(os2[at + 1])); };
    t.weight = u16(4);
    t.width = u16(6);
    const int familyClass = quint8(os2[30]);
    const int panoseFamily = quint8(os2[32]), serifStyle = quint8(os2[33]), proportion = quint8(os2[35]);
    Kind kind = Kind::Unknown;
    if (panoseFamily == 2 && proportion == 9)
        kind = Kind::Mono;
    else if (panoseFamily == 2 && serifStyle >= 11 && serifStyle <= 13)
        kind = Kind::Sans;
    else if (panoseFamily == 2 && serifStyle >= 2 && serifStyle <= 10)
        kind = Kind::Serif;
    else if (panoseFamily == 3)
        kind = Kind::Script;
    else if (panoseFamily == 4)
        kind = Kind::Display;
    else if (panoseFamily == 5)
        kind = Kind::Symbol;
    else if ((familyClass >= 1 && familyClass <= 5) || familyClass == 7)
        kind = Kind::Serif;
    else if (familyClass == 8)
        kind = Kind::Sans;
    else if (familyClass == 9)
        kind = Kind::Display;
    else if (familyClass == 10)
        kind = Kind::Script;
    else if (familyClass == 12)
        kind = Kind::Symbol;
    if (t.kind == Kind::Unknown)
        t.kind = kind;
    return t;
}

// Gesuchte Schrift: Art aus Google Fonts oder dem Namen, Gewicht aus dem Namen oder fett/normal
Traits wantedTraits(const ImportedFont& font)
{
    Traits t = traitsFromName(font.baseName.isEmpty() ? font.family : font.baseName);
    const Traits fromFamily = traitsFromName(font.family);
    if (t.kind == Kind::Unknown)
        t.kind = fromFamily.kind;
    if (const auto g = googleIndex().constFind(normalized(font.family)); g != googleIndex().constEnd() && googleKind(g->category) != Kind::Unknown)
        t.kind = googleKind(g->category);
    if (t.kind == Kind::Unknown && font.fixedPitch)
        t.kind = Kind::Mono;
    if (t.kind == Kind::Unknown && font.serif)
        t.kind = Kind::Serif;
    if (!t.weight)
        t.weight = font.bold ? 700 : 400;
    if (!t.width)
        t.width = 5;
    return t;
}

// Bewährte Ersatzschriften (wie die Ersetzungsregeln von fontconfig): Namensteil der fehlenden
// Schrift -> installierte Gegenstücke, das beste zuerst. Der erste passende Eintrag gilt.
QStringList knownSubstitutes(const QString& key)
{
    static const QList<std::pair<QStringList, QStringList>> table{
        {{QStringLiteral("arialnarrow"), QStringLiteral("helveticanarrow"), QStringLiteral("helveticacondensed")},
         {QStringLiteral("Arial Narrow"), QStringLiteral("Liberation Sans Narrow")}},
        {{QStringLiteral("helvetica"), QStringLiteral("nimbussans"), QStringLiteral("arial"), QStringLiteral("swiss721")},
         {QStringLiteral("Arial"), QStringLiteral("Liberation Sans"), QStringLiteral("Arimo"), QStringLiteral("Swis721 BT")}},
        {{QStringLiteral("myriad"), QStringLiteral("frutiger"), QStringLiteral("univers")},
         {QStringLiteral("Segoe UI"), QStringLiteral("Source Sans Pro"), QStringLiteral("Calibri"), QStringLiteral("Arial")}},
        {{QStringLiteral("avantgarde"), QStringLiteral("futura"), QStringLiteral("centurygothic")},
         {QStringLiteral("Century Gothic"), QStringLiteral("Futura"), QStringLiteral("Tw Cen MT")}},
        {{QStringLiteral("gillsans")}, {QStringLiteral("Gill Sans MT"), QStringLiteral("Gill Sans")}},
        {{QStringLiteral("optima")}, {QStringLiteral("Optima"), QStringLiteral("Candara")}},
        {{QStringLiteral("franklingothic")}, {QStringLiteral("Franklin Gothic Book"), QStringLiteral("Franklin Gothic Medium")}},
        {{QStringLiteral("din")}, {QStringLiteral("Bahnschrift"), QStringLiteral("DIN")}},
        {{QStringLiteral("calibri")}, {QStringLiteral("Calibri"), QStringLiteral("Carlito")}},
        {{QStringLiteral("cambria")}, {QStringLiteral("Cambria"), QStringLiteral("Caladea")}},
        {{QStringLiteral("times"), QStringLiteral("nimbusroman"), QStringLiteral("tinos")},
         {QStringLiteral("Times New Roman"), QStringLiteral("Liberation Serif"), QStringLiteral("Tinos")}},
        {{QStringLiteral("garamond")}, {QStringLiteral("Garamond"), QStringLiteral("EB Garamond")}},
        {{QStringLiteral("caslon")}, {QStringLiteral("Big Caslon"), QStringLiteral("Libre Caslon Text"), QStringLiteral("Garamond")}},
        {{QStringLiteral("palatino")}, {QStringLiteral("Palatino Linotype"), QStringLiteral("Book Antiqua")}},
        {{QStringLiteral("bookman")}, {QStringLiteral("Bookman Old Style")}},
        {{QStringLiteral("cooper")}, {QStringLiteral("Cooper Black")}},
        {{QStringLiteral("rockwell")}, {QStringLiteral("Rockwell")}},
        {{QStringLiteral("courier"), QStringLiteral("nimbusmono")},
         {QStringLiteral("Courier New"), QStringLiteral("Liberation Mono"), QStringLiteral("Cousine")}},
        {{QStringLiteral("brushscript")}, {QStringLiteral("Brush Script MT")}},
        {{QStringLiteral("zapfchancery")}, {QStringLiteral("Monotype Corsiva")}},
        {{QStringLiteral("comicsans")}, {QStringLiteral("Comic Sans MS")}},
        {{QStringLiteral("stencil")}, {QStringLiteral("Stencil")}},
    };
    for (const auto& [names, substitutes] : table)
        for (const QString& n : names)
            if (key.contains(n))
                return substitutes;
    return {};
}

// Schriften für andere Schriftsysteme: ihre lateinischen Zeichen sind nur Beigabe
bool otherScript(const QString& family)
{
    static const QStringList words{
        QStringLiteral("arabic"), QStringLiteral("naskh"), QStringLiteral("hebrew"), QStringLiteral("lao"), QStringLiteral("thai"),
        QStringLiteral("khmer"), QStringLiteral("myanmar"), QStringLiteral("georgian"), QStringLiteral("armenian"), QStringLiteral("devanagari"),
        QStringLiteral("bengali"), QStringLiteral("tamil"), QStringLiteral("telugu"), QStringLiteral("ethiopic"), QStringLiteral("phagspa"),
        QStringLiteral("taile"), QStringLiteral("yibaiti"), QStringLiteral("mongolian"), QStringLiteral("himalaya"), QStringLiteral("ebrima"),
        QStringLiteral("nirmala"), QStringLiteral("leelawadee"), QStringLiteral("gadugi"), QStringLiteral("javanese"), QStringLiteral("math"),
        QStringLiteral("symbol"), QStringLiteral("emoji"), QStringLiteral("cjk"), QStringLiteral("yahei"), QStringLiteral("jhenghei"),
        QStringLiteral("malgun"), QStringLiteral("gothicjp"), QStringLiteral("mincho"), QStringLiteral("simsun"), QStringLiteral("mingliu"),
        QStringLiteral("meiryo"), QStringLiteral("yugothic"), QStringLiteral("batang"), QStringLiteral("gulim"), QStringLiteral("dotum")};
    const QString n = normalized(family);
    return std::any_of(words.cbegin(), words.cend(), [&n](const QString& w) { return n.contains(w); });
}

double traitScore(const Traits& want, const Traits& have)
{
    double sum = 0, n = 0;
    if (want.kind != Kind::Unknown && have.kind == Kind::Unknown) {
        sum += 1.0; // Art der installierten Schrift unbekannt: halber Treffer
        n += 2.0;
    } else if (want.kind != Kind::Unknown) {
        // Zierschriften sind oft Grotesk oder Antiqua mit Ausschmückung: halber Treffer
        const bool near = (want.kind == Kind::Display) != (have.kind == Kind::Display)
            && (want.kind == Kind::Sans || want.kind == Kind::Serif || have.kind == Kind::Sans || have.kind == Kind::Serif);
        sum += 2.0 * (want.kind == have.kind ? 1.0 : near ? 0.4 : 0.0);
        n += 2.0;
    }
    if (want.weight && have.weight) {
        sum += 1.0 - std::min(1.0, std::abs(want.weight - have.weight) / 400.0);
        n += 1.0;
    }
    if (want.width && have.width) {
        sum += 1.0 - std::min(1.0, std::abs(want.width - have.width) / 4.0);
        n += 1.0;
    }
    return n > 0 ? sum / n : -1.0;
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
    const Traits wanted = wantedTraits(font);
    const QStringList substitutes = knownSubstitutes(key);

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
        if (c.shape < 0)
            c.traits = traitScore(wanted, traitsFromFont(raw, family));
        if (c.shape >= 0 && c.widths >= 0)
            c.score = 0.7 * c.shape + 0.3 * c.widths;
        else if (c.shape >= 0)
            c.score = c.shape;
        else if (c.widths >= 0 && c.traits >= 0)
            c.score = 0.5 * c.widths + 0.5 * c.traits;
        else if (c.widths >= 0)
            c.score = c.widths;
        else if (c.traits >= 0)
            c.score = 0.7 * c.traits; // nur nach Name und Klasse: höchstens 70 %, eine Vermutung
        else
            c.score = 0;
        // Gleicher oder enthaltener Name: kleiner Vorzug; bewährte Ersatzschrift: deutlicher Vorzug
        const QString ck = normalized(family);
        if (!key.isEmpty() && (ck.contains(key) || key.contains(ck)))
            c.score = std::min(1.0, c.score + 0.05);
        if (const qsizetype at = substitutes.indexOf(family); at >= 0)
            c.score = std::min(1.0, c.score + 0.2 - 0.03 * double(at));
        if (otherScript(family) && !otherScript(font.family))
            c.score = std::max(0.0, c.score - 0.15);
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
        // Nur die Läufe in dieser Schrift: ein Text kann mehrere Schriften enthalten
        QList<TextParagraph> paragraphs = text->paragraphs();
        QSet<QString> families;
        for (const TextParagraph& p : std::as_const(paragraphs))
            for (const TextRun& r : p.runs)
                families.insert(r.format.family);
        for (TextParagraph& p : paragraphs)
            for (TextRun& r : p.runs)
                if (families.size() == 1 || r.format.family == font.family)
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
#elif defined(Q_OS_MACOS)
    dirs << QDir::homePath() + QStringLiteral("/Library/Fonts") << QStringLiteral("/Library/Fonts");
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

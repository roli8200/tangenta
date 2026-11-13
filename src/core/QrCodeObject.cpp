#include "QrCodeObject.h"

#include "qrcodegen.hpp"

#include <QJsonObject>
#include <QObject>
#include <QUrl>

#include <functional>
#include <map>

namespace vek {

namespace qr {

QString kindLabel(Kind kind)
{
    switch (kind) {
    case Kind::Url: return QObject::tr("Webseite");
    case Kind::Text: return QObject::tr("Text");
    case Kind::Email: return QObject::tr("E-Mail");
    case Kind::Phone: return QObject::tr("Telefon");
    case Kind::Sms: return QObject::tr("SMS");
    case Kind::Wifi: return QObject::tr("WLAN");
    case Kind::Contact: return QObject::tr("Visitenkarte");
    }
    return {};
}

QList<Field> fields(Kind kind)
{
    switch (kind) {
    case Kind::Url: return {{QStringLiteral("url"), QObject::tr("Adresse:")}};
    case Kind::Text: return {{QStringLiteral("text"), QObject::tr("Text:"), true}};
    case Kind::Email:
        return {{QStringLiteral("to"), QObject::tr("An:")},
                {QStringLiteral("subject"), QObject::tr("Betreff:")},
                {QStringLiteral("body"), QObject::tr("Nachricht:"), true}};
    case Kind::Phone: return {{QStringLiteral("number"), QObject::tr("Nummer:")}};
    case Kind::Sms:
        return {{QStringLiteral("number"), QObject::tr("Nummer:")},
                {QStringLiteral("text"), QObject::tr("Nachricht:"), true}};
    case Kind::Wifi:
        return {{QStringLiteral("ssid"), QObject::tr("Netzwerkname:")},
                {QStringLiteral("password"), QObject::tr("Passwort:")},
                {QStringLiteral("security"), QObject::tr("Verschlüsselung:"), false,
                 {QStringLiteral("WPA/WPA2/WPA3"), QStringLiteral("WEP"), QObject::tr("keine")},
                 {QStringLiteral("WPA"), QStringLiteral("WEP"), QStringLiteral("nopass")}},
                {QStringLiteral("hidden"), QObject::tr("Verborgenes Netz:"), false, {QObject::tr("nein"), QObject::tr("ja")},
                 {QStringLiteral("0"), QStringLiteral("1")}}};
    case Kind::Contact:
        return {{QStringLiteral("first"), QObject::tr("Vorname:")},
                {QStringLiteral("last"), QObject::tr("Nachname:")},
                {QStringLiteral("org"), QObject::tr("Firma:")},
                {QStringLiteral("title"), QObject::tr("Funktion:")},
                {QStringLiteral("phone"), QObject::tr("Telefon:")},
                {QStringLiteral("mobile"), QObject::tr("Mobil:")},
                {QStringLiteral("email"), QObject::tr("E-Mail:")},
                {QStringLiteral("url"), QObject::tr("Webseite:")},
                {QStringLiteral("street"), QObject::tr("Strasse:")},
                {QStringLiteral("zip"), QObject::tr("PLZ:")},
                {QStringLiteral("city"), QObject::tr("Ort:")},
                {QStringLiteral("country"), QObject::tr("Land:")}};
    }
    return {};
}

namespace {

// Sonderzeichen in WIFI: und MECARD mit Rückstrich schützen
QString escapeWifi(const QString& s)
{
    QString out;
    for (const QChar c : s) {
        if (c == u'\\' || c == u';' || c == u',' || c == u':' || c == u'"')
            out += u'\\';
        out += c;
    }
    return out;
}

// vCard 3.0: Rückstrich, Komma, Strichpunkt und Zeilenumbruch schützen
QString escapeVCard(const QString& s)
{
    QString out;
    for (const QChar c : s) {
        if (c == u'\\' || c == u',' || c == u';')
            out += u'\\';
        if (c == u'\n')
            out += QStringLiteral("\\n");
        else if (c != u'\r')
            out += c;
    }
    return out;
}

QString digits(const QString& number)
{
    QString out;
    for (const QChar c : number.trimmed()) {
        if (c.isDigit() || c == u'+' || c == u'*' || c == u'#')
            out += c;
    }
    return out;
}

} // namespace

QString payload(Kind kind, const QMap<QString, QString>& v)
{
    auto get = [&](const char* id) { return v.value(QLatin1String(id)).trimmed(); };
    switch (kind) {
    case Kind::Url: {
        const QString url = get("url");
        // Ohne Schema ergänzen, sonst öffnen manche Leser eine Websuche
        if (!url.isEmpty() && !url.contains(QLatin1String("://")) && !url.startsWith(QLatin1String("mailto:")))
            return QStringLiteral("https://") + url;
        return url;
    }
    case Kind::Text: return v.value(QStringLiteral("text"));
    case Kind::Email: {
        QString out = QStringLiteral("mailto:") + get("to");
        QStringList query;
        if (!get("subject").isEmpty())
            query << QStringLiteral("subject=") + QString::fromLatin1(QUrl::toPercentEncoding(get("subject")));
        if (!v.value(QStringLiteral("body")).isEmpty())
            query << QStringLiteral("body=") + QString::fromLatin1(QUrl::toPercentEncoding(v.value(QStringLiteral("body"))));
        if (!query.isEmpty())
            out += u'?' + query.join(u'&');
        return out;
    }
    case Kind::Phone: return QStringLiteral("tel:") + digits(get("number"));
    case Kind::Sms: return QStringLiteral("SMSTO:") + digits(get("number")) + u':' + v.value(QStringLiteral("text"));
    case Kind::Wifi: {
        const QString security = get("security");
        const QString type = security == QLatin1String("WEP") || security == QLatin1String("nopass") ? security
                                                                                                    : QStringLiteral("WPA");
        QString out = QStringLiteral("WIFI:T:%1;S:%2;").arg(type, escapeWifi(get("ssid")));
        if (type != QLatin1String("nopass"))
            out += QStringLiteral("P:%1;").arg(escapeWifi(v.value(QStringLiteral("password"))));
        if (get("hidden") == QLatin1String("1"))
            out += QStringLiteral("H:true;");
        return out + u';';
    }
    case Kind::Contact: {
        QStringList lines{QStringLiteral("BEGIN:VCARD"), QStringLiteral("VERSION:3.0")};
        lines << QStringLiteral("N:%1;%2;;;").arg(escapeVCard(get("last")), escapeVCard(get("first")));
        const QString full = QStringList{get("first"), get("last")}.join(u' ').trimmed();
        lines << QStringLiteral("FN:") + escapeVCard(full.isEmpty() ? get("org") : full);
        auto line = [&](const QString& key, const char* id) {
            if (!get(id).isEmpty())
                lines << key + escapeVCard(get(id));
        };
        line(QStringLiteral("ORG:"), "org");
        line(QStringLiteral("TITLE:"), "title");
        line(QStringLiteral("TEL;TYPE=WORK,VOICE:"), "phone");
        line(QStringLiteral("TEL;TYPE=CELL:"), "mobile");
        line(QStringLiteral("EMAIL:"), "email");
        line(QStringLiteral("URL:"), "url");
        if (!get("street").isEmpty() || !get("city").isEmpty() || !get("zip").isEmpty() || !get("country").isEmpty())
            lines << QStringLiteral("ADR;TYPE=WORK:;;%1;%2;;%3;%4")
                         .arg(escapeVCard(get("street")), escapeVCard(get("city")), escapeVCard(get("zip")),
                              escapeVCard(get("country")));
        lines << QStringLiteral("END:VCARD");
        return lines.join(QStringLiteral("\r\n"));
    }
    }
    return {};
}

QStringList errorCorrectionLabels()
{
    return {QObject::tr("L – 7 %"), QObject::tr("M – 15 %"), QObject::tr("Q – 25 %"), QObject::tr("H – 30 %")};
}

Matrix encode(const QString& text, int errorCorrection, QString* error)
{
    static const qrcodegen::QrCode::Ecc levels[] = {qrcodegen::QrCode::Ecc::LOW, qrcodegen::QrCode::Ecc::MEDIUM,
                                                    qrcodegen::QrCode::Ecc::QUARTILE, qrcodegen::QrCode::Ecc::HIGH};
    Matrix m;
    if (text.isEmpty()) {
        if (error)
            *error = QObject::tr("Kein Inhalt");
        return m;
    }
    try {
        const QByteArray utf8 = text.toUtf8();
        const qrcodegen::QrCode code = qrcodegen::QrCode::encodeText(utf8.constData(), levels[std::clamp(errorCorrection, 0, 3)]);
        m.size = code.getSize();
        m.version = code.getVersion();
        m.dark.resize(std::size_t(m.size * m.size));
        for (int y = 0; y < m.size; ++y)
            for (int x = 0; x < m.size; ++x)
                m.dark[std::size_t(y * m.size + x)] = code.getModule(x, y);
    } catch (const qrcodegen::data_too_long&) {
        if (error)
            *error = QObject::tr("Der Inhalt ist zu lang für einen QR-Code");
        return Matrix();
    }
    return m;
}

} // namespace qr

namespace {

const QString kData = QStringLiteral("qrData");
const QString kEcc = QStringLiteral("qrEcc");
const QString kShape = QStringLiteral("qrShape");
const QString kQuiet = QStringLiteral("qrQuiet");
const QString kBackground = QStringLiteral("qrBackground");
const QString kEdit = QStringLiteral("qrEdit");

const char* const kKindNames[] = {"url", "text", "email", "phone", "sms", "wifi", "contact"};

bool inFinder(int x, int y, int n)
{
    return (x < 7 && y < 7) || (x >= n - 7 && y < 7) || (x < 7 && y >= n - 7);
}

// Umriss aller dunklen Module als geschlossene Polygone (Kanten zwischen dunkel und hell,
// im Uhrzeigersinn um jedes dunkle Feld). Löcher laufen dadurch gegenläufig: mit WindingFill
// entstehen saubere Flächen ohne Haarlinien zwischen Nachbarmodulen.
QPainterPath traceModules(const qr::Matrix& m, const std::function<bool(int, int)>& include,
                          const std::function<QPointF(int, int)>& corner)
{
    using Pt = std::pair<int, int>;
    std::multimap<Pt, Pt> edges;
    auto dark = [&](int x, int y) { return m.at(x, y) && include(x, y); };
    for (int y = 0; y < m.size; ++y) {
        for (int x = 0; x < m.size; ++x) {
            if (!dark(x, y))
                continue;
            if (!dark(x, y - 1))
                edges.emplace(Pt{x, y}, Pt{x + 1, y});
            if (!dark(x + 1, y))
                edges.emplace(Pt{x + 1, y}, Pt{x + 1, y + 1});
            if (!dark(x, y + 1))
                edges.emplace(Pt{x + 1, y + 1}, Pt{x, y + 1});
            if (!dark(x - 1, y))
                edges.emplace(Pt{x, y + 1}, Pt{x, y});
        }
    }
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    while (!edges.empty()) {
        auto it = edges.begin();
        const Pt start = it->first;
        Pt from = start;
        Pt to = it->second;
        edges.erase(it);
        QList<Pt> loop{from};
        while (to != start) {
            // Richtungswechsel nur an Ecken; gerade Strecken zusammenfassen
            const Pt dir{to.first - from.first, to.second - from.second};
            auto range = edges.equal_range(to);
            auto next = range.first;
            for (auto r = range.first; r != range.second; ++r) {
                // Bei zwei Fortsetzungen (Module berühren sich nur an der Ecke) rechts abbiegen
                const Pt d{r->second.first - to.first, r->second.second - to.second};
                if (d.first == -dir.second && d.second == dir.first)
                    next = r;
            }
            if (next == range.second)
                break;
            const Pt d{next->second.first - to.first, next->second.second - to.second};
            if (d != dir)
                loop << to;
            from = to;
            to = next->second;
            edges.erase(next);
        }
        path.moveTo(corner(loop.first().first, loop.first().second));
        for (int i = 1; i < loop.size(); ++i)
            path.lineTo(corner(loop[i].first, loop[i].second));
        path.closeSubpath();
    }
    return path;
}

} // namespace

QrCodeObject::QrCodeObject()
{
    Style s;
    s.fill = Fill::solid(Color(0, 0, 0));
    s.stroke.reset();
    setStyle(s);
    m_values.insert(QStringLiteral("url"), QStringLiteral("tangenta.ch"));
}

void QrCodeObject::setContent(qr::Kind kind, const QMap<QString, QString>& values)
{
    m_kind = kind;
    m_values = values;
}

const qr::Matrix& QrCodeObject::matrix() const
{
    const QString text = payload();
    if (!m_cache || m_cache->text != text || m_cache->ecc != m_ecc) {
        // Neuer Zwischenspeicher statt Ändern: Kopien des Objekts teilen den alten
        auto cache = std::make_shared<Cache>();
        cache->text = text;
        cache->ecc = m_ecc;
        cache->matrix = qr::encode(text, m_ecc);
        m_cache = cache;
    }
    return m_cache->matrix;
}

QPainterPath QrCodeObject::backgroundPath() const
{
    QPainterPath p;
    p.addRect(QRectF(QPointF(0, 0), m_size));
    return p;
}

QPainterPath QrCodeObject::localPath() const
{
    const qr::Matrix& m = matrix();
    if (m.size == 0)
        return {};
    const int n = m.size;
    const double mw = m_size.width() / (n + 2 * m_quiet);
    const double mh = m_size.height() / (n + 2 * m_quiet);
    auto corner = [&](int x, int y) { return QPointF((x + m_quiet) * mw, (y + m_quiet) * mh); };
    if (m_shape == Shape::Squares)
        return traceModules(m, [](int, int) { return true; }, corner);

    // Punkte: Datenmodule als Kreise, die drei Suchmuster als abgerundete Quadrate
    // Gerade-ungerade: Ring = äusseres minus inneres Quadrat, der Kern darin wieder gefüllt
    QPainterPath p;
    p.setFillRule(Qt::OddEvenFill);
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            if (m.at(x, y) && !inFinder(x, y, n))
                p.addEllipse(QRectF(corner(x, y), QSizeF(mw, mh)).adjusted(mw * 0.06, mh * 0.06, -mw * 0.06, -mh * 0.06));
        }
    }
    for (const QPoint origin : {QPoint(0, 0), QPoint(n - 7, 0), QPoint(0, n - 7)}) {
        const QPointF o = corner(origin.x(), origin.y());
        p.addRoundedRect(QRectF(o, QSizeF(7 * mw, 7 * mh)), 2.0 * mw, 2.0 * mh);
        p.addRoundedRect(QRectF(o + QPointF(mw, mh), QSizeF(5 * mw, 5 * mh)), 1.4 * mw, 1.4 * mh);
        p.addRoundedRect(QRectF(o + QPointF(2 * mw, 2 * mh), QSizeF(3 * mw, 3 * mh)), 0.9 * mw, 0.9 * mh);
    }
    return p;
}

QPainterPath QrCodeObject::snapPath() const
{
    return transform().map(backgroundPath());
}

QRectF QrCodeObject::boundingRect() const
{
    return transform().map(backgroundPath()).boundingRect();
}

bool QrCodeObject::hitTest(const QPointF& point, double tolerance, bool /*treatAsFilled*/) const
{
    // Ganze Fläche trifft, auch zwischen den Modulen
    const QPainterPath area = transform().map(backgroundPath());
    return area.intersects(QRectF(point - QPointF(tolerance, tolerance), QSizeF(2 * tolerance, 2 * tolerance)));
}

void QrCodeObject::applyTransform(const QTransform& m)
{
    DrawObject::applyTransform(m);
    absorbScale(m_size);
}

QList<PropertyDescriptor> QrCodeObject::propertyDescriptors() const
{
    using K = PropertyDescriptor::Kind;
    QList<PropertyDescriptor> list;

    PropertyDescriptor edit;
    edit.id = kEdit;
    edit.label = QObject::tr("QR-Code bearbeiten …");
    edit.icon = QStringLiteral("qr-code");
    edit.kind = K::Action;
    edit.separatorBefore = true;
    list << edit;

    // Schnell ändern: Webadresse oder Text direkt in der Leiste
    if (m_kind == qr::Kind::Url || m_kind == qr::Kind::Text) {
        PropertyDescriptor data;
        data.id = kData;
        data.label = m_kind == qr::Kind::Url ? QObject::tr("Webadresse im QR-Code") : QObject::tr("Text im QR-Code");
        data.kind = K::Text;
        data.textWidth = 220;
        list << data;
    }

    PropertyDescriptor ecc;
    ecc.id = kEcc;
    ecc.label = QObject::tr("Fehlerkorrektur");
    ecc.kind = K::List;
    ecc.choiceLabels = qr::errorCorrectionLabels();
    list << ecc;

    PropertyDescriptor shape;
    shape.id = kShape;
    shape.label = QObject::tr("Form der Module");
    shape.kind = K::List;
    shape.choiceLabels = {QObject::tr("Quadrate"), QObject::tr("Punkte")};
    list << shape;

    PropertyDescriptor quiet;
    quiet.id = kQuiet;
    quiet.label = QObject::tr("Ruhezone in Modulen");
    quiet.kind = K::Number;
    quiet.minimum = 0;
    quiet.maximum = 10;
    list << quiet;

    PropertyDescriptor bg;
    bg.id = kBackground;
    bg.label = QObject::tr("Weisser Hintergrund");
    bg.icon = QStringLiteral("qr-background");
    bg.kind = K::Toggle;
    list << bg;
    return list;
}

QVariant QrCodeObject::propertyValue(const QString& id) const
{
    if (id == kData)
        return m_values.value(m_kind == qr::Kind::Url ? QStringLiteral("url") : QStringLiteral("text"));
    if (id == kEcc)
        return m_ecc;
    if (id == kShape)
        return static_cast<int>(m_shape);
    if (id == kQuiet)
        return m_quiet;
    if (id == kBackground)
        return m_background.has_value();
    return {};
}

bool QrCodeObject::setPropertyValue(const QString& id, const QVariant& value)
{
    if (id == kData) {
        m_values.insert(m_kind == qr::Kind::Url ? QStringLiteral("url") : QStringLiteral("text"), value.toString());
        return true;
    }
    if (id == kEcc) {
        setErrorCorrection(value.toInt());
        return true;
    }
    if (id == kShape) {
        m_shape = value.toInt() == 1 ? Shape::Dots : Shape::Squares;
        return true;
    }
    if (id == kQuiet) {
        setQuietZone(qRound(value.toDouble()));
        return true;
    }
    if (id == kBackground) {
        if (value.toBool())
            m_background = m_background.value_or(Color(255, 255, 255));
        else
            m_background.reset();
        return true;
    }
    return false;
}

void QrCodeObject::writeProperties(QJsonObject& out) const
{
    DrawObject::writeProperties(out);
    out[QStringLiteral("width")] = m_size.width();
    out[QStringLiteral("height")] = m_size.height();
    out[QStringLiteral("kind")] = QLatin1String(kKindNames[static_cast<int>(m_kind)]);
    QJsonObject values;
    for (auto it = m_values.cbegin(); it != m_values.cend(); ++it)
        values[it.key()] = it.value();
    out[QStringLiteral("values")] = values;
    out[QStringLiteral("errorCorrection")] = m_ecc;
    out[QStringLiteral("shape")] = m_shape == Shape::Dots ? QStringLiteral("dots") : QStringLiteral("squares");
    out[QStringLiteral("quietZone")] = m_quiet;
    if (m_background)
        out[QStringLiteral("background")] = m_background->toHex();
}

void QrCodeObject::readProperties(const QJsonObject& in)
{
    DrawObject::readProperties(in);
    m_size = QSizeF(in.value(QStringLiteral("width")).toDouble(30.0), in.value(QStringLiteral("height")).toDouble(30.0));
    const QString kind = in.value(QStringLiteral("kind")).toString();
    m_kind = qr::Kind::Text;
    for (int i = 0; i < qr::kKindCount; ++i) {
        if (kind == QLatin1String(kKindNames[i]))
            m_kind = static_cast<qr::Kind>(i);
    }
    m_values.clear();
    const QJsonObject values = in.value(QStringLiteral("values")).toObject();
    for (auto it = values.begin(); it != values.end(); ++it)
        m_values.insert(it.key(), it.value().toString());
    setErrorCorrection(in.value(QStringLiteral("errorCorrection")).toInt(1));
    m_shape = in.value(QStringLiteral("shape")).toString() == QLatin1String("dots") ? Shape::Dots : Shape::Squares;
    setQuietZone(in.value(QStringLiteral("quietZone")).toInt(2));
    m_background.reset();
    if (in.contains(QStringLiteral("background")))
        m_background = Color::fromHex(in.value(QStringLiteral("background")).toString());
}

} // namespace vek

#include "ImageSearch.h"

#include "Editor.h"
#include "ImageObject.h"
#include "Options.h"
#include "SvgImport.h"

#include <QImageReader>
#include <QJsonArray>
#include <QLocale>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QTimer>
#include <QUrlQuery>

#include <memory>

namespace vek {

namespace imagesearch {

namespace {

const QByteArray kUserAgent = "TANGENTA/1.0 (Vektorgrafik; Qt) Bildersuche";
constexpr int kTimeoutMs = 12000;

QUrl withQuery(const QString& base, const QList<QPair<QString, QString>>& items)
{
    QUrl url(base);
    QUrlQuery q;
    for (const auto& [k, v] : items)
        q.addQueryItem(k, QString::fromUtf8(QUrl::toPercentEncoding(v)));
    url.setQuery(q);
    return url;
}

QJsonObject object(const QByteArray& json)
{
    return QJsonDocument::fromJson(json).object();
}

QString str(const QJsonValue& v)
{
    return v.isDouble() ? QString::number(qint64(v.toDouble())) : v.toString();
}

// Seitenlängen, wenn (w, h) in maxW × maxH eingepasst wird (nie vergrössert)
QPair<int, int> fit(int w, int h, int maxW, int maxH)
{
    if (w <= 0 || h <= 0)
        return {maxW, maxH};
    const double f = std::min({1.0, maxW > 0 ? double(maxW) / w : 1.0, maxH > 0 ? double(maxH) / h : 1.0});
    return {int(std::lround(w * f)), int(std::lround(h * f))};
}

Size sized(const QString& label, const QUrl& url, int w, int h, int maxW, int maxH = 0)
{
    if (w <= 0 || h <= 0) // Masse unbekannt: nur die Obergrenze nennen
        return {label.isEmpty() ? QObject::tr("bis %1 Pixel").arg(std::max(maxW, maxH)) : label, url, 0, 0, false};
    const auto [fw, fh] = fit(w, h, maxW, maxH);
    return {label, url, fw, fh, false};
}

QString stripTags(QString s)
{
    s.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    return s.simplified();
}

} // namespace

const QList<Source>& sources()
{
    static const QList<Source> list{
        {QStringLiteral("commons"), QStringLiteral("Wikimedia Commons"), false, Kind::Photo,
         QObject::tr("Braucht keinen Schlüssel. Freie Bilder, meist mit Namensnennungspflicht.")},
        {QStringLiteral("openverse"), QStringLiteral("Openverse"), false, Kind::Photo,
         QObject::tr("Braucht keinen Schlüssel (dann 20 Suchen je Minute, 200 je Tag). Sammelt freie Bilder "
                     "aus vielen Archiven, darunter Flickr.")},
        {QStringLiteral("icons8"), QStringLiteral("Icons8"), false, Kind::Icon,
         QObject::tr("Sucht ohne Schlüssel; ohne Abo nur kleine PNG (100 Pixel) mit Pflicht-Link auf icons8.com. "
                     "Mit API-Schlüssel (developers.icons8.com) als SVG, d.h. bearbeitbare Kurven.")},
        {QStringLiteral("flaticon"), QStringLiteral("Flaticon"), true, Kind::Icon,
         QObject::tr("Flaticon läuft über die Magnific-Schnittstelle (früher Freepik): derselbe Schlüssel wie "
                     "Magnific aus magnific.com/developers/dashboard. Freie Symbole als SVG mit Namensnennung.")},
        {QStringLiteral("magnific"), QStringLiteral("Magnific (früher Freepik)"), true, Kind::Photo,
         QObject::tr("Der grösste Katalog. API-Schlüssel aus magnific.com/developers/dashboard. ACHTUNG: die API "
                     "wird getrennt vom Abonnement abgerechnet.")},
        {QStringLiteral("pixabay"), QStringLiteral("Pixabay"), true, Kind::Photo,
         QObject::tr("Schlüssel aus dem Pixabay-Konto unter „Dokumentation → API“.")},
        {QStringLiteral("unsplash"), QStringLiteral("Unsplash"), true, Kind::Photo,
         QObject::tr("Der „Access Key“ einer Anwendung aus unsplash.com/oauth/applications.")},
        {QStringLiteral("pexels"), QStringLiteral("Pexels"), true, Kind::Photo, QObject::tr("Schlüssel aus pexels.com/api.")},
        {QStringLiteral("adobe"), QStringLiteral("Adobe Stock"), true, Kind::Photo,
         QObject::tr("API-Key aus der Adobe Developer Console. Nur Vorschauen mit Wasserzeichen.")},
        {QStringLiteral("getty"), QStringLiteral("Getty Images"), true, Kind::Photo,
         QObject::tr("API-Key aus developers.gettyimages.com. Nur Vorschauen mit Wasserzeichen.")},
        {QStringLiteral("istock"), QStringLiteral("iStock"), true, Kind::Photo,
         QObject::tr("Dieselbe Schnittstelle wie Getty, eigener Schlüssel. Nur Vorschauen mit Wasserzeichen.")},
    };
    return list;
}

const Source* source(const QString& key)
{
    for (const Source& s : sources())
        if (s.key == key)
            return &s;
    return nullptr;
}

QString optionKey(const QString& source)
{
    // Flaticon nimmt den Magnific-Schlüssel
    return QStringLiteral("global/imageSearch/key/") + (source == QLatin1String("flaticon") ? QStringLiteral("magnific") : source);
}

QStringList available(const QHash<QString, QString>& keys)
{
    QStringList out;
    for (const Source& s : sources()) {
        const QString k = s.key == QLatin1String("flaticon") ? QStringLiteral("magnific") : s.key;
        if (!s.needsKey || !keys.value(k).trimmed().isEmpty())
            out << s.key;
    }
    return out;
}

QHash<QString, QString> keysFromOptions()
{
    QHash<QString, QString> keys;
    for (const Source& s : sources()) {
        const QString key = optionKey(s.key);
        if (Options::instance().known(key)) {
            const QString v = Options::instance().text(key).trimmed();
            if (!v.isEmpty())
                keys.insert(key.section(QLatin1Char('/'), -1), v);
        }
    }
    return keys;
}

int pageLimit(const QString& source)
{
    static const QHash<QString, int> limits{{QStringLiteral("commons"), 50}, {QStringLiteral("openverse"), 20},
                                            {QStringLiteral("unsplash"), 30}, {QStringLiteral("pexels"), 80},
                                            {QStringLiteral("adobe"), 64}};
    return limits.value(source, 100);
}

QUrl viewUrl(const Hit& hit)
{
    // Magnific/Flaticon-Originale gehen nur über den Download-Endpunkt (zählt als Download): Vorschau zeigen
    if (hit.source == QLatin1String("magnific") || hit.source == QLatin1String("flaticon") || hit.source == QLatin1String("adobe") ||
        hit.source == QLatin1String("getty") || hit.source == QLatin1String("istock"))
        return hit.image.isEmpty() ? hit.preview : hit.image;
    if (hit.source == QLatin1String("icons8"))
        return QUrl(QStringLiteral("https://img.icons8.com/?size=512&id=%1&format=png").arg(hit.id));
    // Kleinste Fassung, die einen Bildschirm füllt (die Liste ist absteigend), sonst die grösste
    const QList<Size> all = choices(hit);
    QUrl best;
    for (const Size& s : all) {
        if (s.vector || s.url.isEmpty())
            continue;
        if (best.isEmpty() || s.width >= 1600 || s.height >= 1200 || s.width == 0)
            best = s.url;
    }
    return best.isEmpty() ? hit.image : best;
}

bool insertable(const Hit& hit)
{
    return hit.freeToUse || hit.subscription;
}

QList<Request> requests(const QString& source, const QString& term, int perPage, int page, const QHash<QString, QString>& keys)
{
    perPage = std::min(perPage, pageLimit(source));
    const QString n = QString::number(perPage), p = QString::number(page);
    const QString offset = QString::number((page - 1) * perPage);
    const QString key = keys.value(source == QLatin1String("flaticon") ? QStringLiteral("magnific") : source).trimmed();
    if (source == QLatin1String("commons")) {
        // Zwei Abrufe: grosse Fassung zum Einfügen, kleine für die Kachel (Commons liefert nur
        // die Breiten aus, die man bestellt)
        auto query = [&](const QString& width) {
            return withQuery(QStringLiteral("https://commons.wikimedia.org/w/api.php"),
                             {{QStringLiteral("action"), QStringLiteral("query")},
                              {QStringLiteral("format"), QStringLiteral("json")},
                              {QStringLiteral("generator"), QStringLiteral("search")},
                              {QStringLiteral("gsrsearch"), QStringLiteral("filetype:bitmap ") + term},
                              {QStringLiteral("gsrnamespace"), QStringLiteral("6")},
                              {QStringLiteral("gsrlimit"), n},
                              {QStringLiteral("gsroffset"), offset},
                              {QStringLiteral("prop"), QStringLiteral("imageinfo")},
                              {QStringLiteral("iiprop"), QStringLiteral("url|size|mime|extmetadata")},
                              {QStringLiteral("iiurlwidth"), width}});
        };
        return {{source, query(QStringLiteral("1600")), {}}, {QStringLiteral("commons_preview"), query(QStringLiteral("330")), {}}};
    }
    if (source == QLatin1String("openverse"))
        // Nur gewerblich nutzbare Lizenzen (keine NC)
        return {{source,
                 withQuery(QStringLiteral("https://api.openverse.org/v1/images/"),
                           {{QStringLiteral("q"), term},
                            {QStringLiteral("page_size"), n},
                            {QStringLiteral("page"), p},
                            {QStringLiteral("license_type"), QStringLiteral("commercial")}}),
                 {}}};
    if (source == QLatin1String("icons8")) {
        QList<QPair<QString, QString>> q{{QStringLiteral("term"), term},
                                         {QStringLiteral("amount"), n},
                                         {QStringLiteral("offset"), offset},
                                         {QStringLiteral("language"), QStringLiteral("de")}};
        if (!key.isEmpty())
            q.append({QStringLiteral("token"), key});
        return {{source, withQuery(QStringLiteral("https://search.icons8.com/api/iconsets/v5/search"), q), {}}};
    }
    if (source == QLatin1String("flaticon"))
        return {{source,
                 withQuery(QStringLiteral("https://api.magnific.com/v1/icons"),
                           {{QStringLiteral("term"), term},
                            {QStringLiteral("per_page"), n},
                            {QStringLiteral("page"), p},
                            {QStringLiteral("order"), QStringLiteral("relevance")},
                            {QStringLiteral("thumbnail_size"), QStringLiteral("128")}}),
                 {{"x-magnific-api-key", key.toUtf8()}, {"Accept-Language", "de-DE"}}}};
    if (source == QLatin1String("magnific"))
        return {{source,
                 withQuery(QStringLiteral("https://api.magnific.com/v1/resources"),
                           {{QStringLiteral("term"), term},
                            {QStringLiteral("limit"), n},
                            {QStringLiteral("page"), p},
                            {QStringLiteral("order"), QStringLiteral("relevance")},
                            {QStringLiteral("filters[content_type][photo]"), QStringLiteral("1")}}),
                 {{"x-magnific-api-key", key.toUtf8()}, {"Accept-Language", "de-DE"}}}};
    if (source == QLatin1String("pixabay"))
        return {{source,
                 withQuery(QStringLiteral("https://pixabay.com/api/"),
                           {{QStringLiteral("key"), key},
                            {QStringLiteral("q"), term},
                            {QStringLiteral("per_page"), QString::number(std::max(3, perPage))},
                            {QStringLiteral("page"), p},
                            {QStringLiteral("image_type"), QStringLiteral("photo")},
                            {QStringLiteral("safesearch"), QStringLiteral("true")},
                            {QStringLiteral("lang"), QStringLiteral("de")}}),
                 {}}};
    if (source == QLatin1String("unsplash"))
        return {{source,
                 withQuery(QStringLiteral("https://api.unsplash.com/search/photos"),
                           {{QStringLiteral("query"), term}, {QStringLiteral("per_page"), n}, {QStringLiteral("page"), p}}),
                 {{"Authorization", "Client-ID " + key.toUtf8()}}}};
    if (source == QLatin1String("pexels"))
        return {{source,
                 withQuery(QStringLiteral("https://api.pexels.com/v1/search"),
                           {{QStringLiteral("query"), term},
                            {QStringLiteral("per_page"), n},
                            {QStringLiteral("page"), p},
                            {QStringLiteral("locale"), QStringLiteral("de-DE")}}),
                 {{"Authorization", key.toUtf8()}}}};
    if (source == QLatin1String("adobe")) {
        QUrl url = withQuery(QStringLiteral("https://stock.adobe.io/Rest/Media/1/Search/Files"),
                             {{QStringLiteral("locale"), QStringLiteral("de_DE")},
                              {QStringLiteral("search_parameters[words]"), term},
                              {QStringLiteral("search_parameters[limit]"), n},
                              {QStringLiteral("search_parameters[offset]"), offset}});
        url.setQuery(url.query() + QStringLiteral("&result_columns[]=id&result_columns[]=title&result_columns[]=thumbnail_500_url"
                                                  "&result_columns[]=width&result_columns[]=height&result_columns[]=creator_name"));
        return {{source, url, {{"x-api-key", key.toUtf8()}, {"X-Product", "TANGENTA/1.0"}}}};
    }
    if (source == QLatin1String("getty") || source == QLatin1String("istock"))
        return {{source,
                 withQuery(QStringLiteral("https://api.gettyimages.com/v3/search/images"),
                           {{QStringLiteral("phrase"), term},
                            {QStringLiteral("page_size"), n},
                            {QStringLiteral("page"), p},
                            {QStringLiteral("fields"), QStringLiteral("id,title,artist,display_set,max_dimensions,referral_destinations")}}),
                 {{"Api-Key", key.toUtf8()}}}};
    return {};
}

QList<Hit> parse(const QString& source, const QHash<QString, QByteArray>& replies, const QHash<QString, QString>& keys)
{
    QList<Hit> out;
    const QJsonObject d = object(replies.value(source));
    if (source == QLatin1String("commons")) {
        // Kachelbilder aus dem zweiten Abruf, über die Seitennummer zugeordnet
        QHash<QString, QString> small;
        const QJsonObject smallPages = object(replies.value(QStringLiteral("commons_preview")))[QStringLiteral("query")]
                                           .toObject()[QStringLiteral("pages")].toObject();
        for (const QJsonValue& v : smallPages) {
            const QJsonObject s = v.toObject();
            const QString u = s[QStringLiteral("imageinfo")].toArray().first().toObject()[QStringLiteral("thumburl")].toString();
            if (!u.isEmpty())
                small.insert(str(s[QStringLiteral("pageid")]), u);
        }
        const QJsonObject pages = d[QStringLiteral("query")].toObject()[QStringLiteral("pages")].toObject();
        QList<QPair<int, Hit>> sorted;
        for (const QJsonValue& v : pages) {
            const QJsonObject s = v.toObject();
            const QJsonObject info = s[QStringLiteral("imageinfo")].toArray().first().toObject();
            if (info.isEmpty())
                continue;
            const QJsonObject meta = info[QStringLiteral("extmetadata")].toObject();
            Hit h;
            h.source = source;
            h.id = str(s[QStringLiteral("pageid")]);
            h.title = s[QStringLiteral("title")].toString().remove(QRegularExpression(QStringLiteral("^File:")));
            const QString thumb = info[QStringLiteral("thumburl")].toString();
            h.image = QUrl(thumb.isEmpty() ? info[QStringLiteral("url")].toString() : thumb);
            h.preview = QUrl(small.value(h.id, h.image.toString()));
            h.page = QUrl(info[QStringLiteral("descriptionurl")].toString());
            h.width = info[QStringLiteral("width")].toInt();
            h.height = info[QStringLiteral("height")].toInt();
            // Commons liefert verkleinerte Fassungen nur in festen Breiten (sonst HTTP 400); die
            // bestellte steht als "/1920px-" im Pfad. Das Original nur, wenn Qt das Format liest (TIFF ...).
            const QString original = info[QStringLiteral("url")].toString();
            const QByteArray mime = info[QStringLiteral("mime")].toString().toLatin1();
            if (mime.isEmpty() || QImageReader::supportedMimeTypes().contains(mime))
                h.sizes << Size{QObject::tr("Original"), QUrl(original), h.width, h.height, false};
            static const QRegularExpression bucket(QStringLiteral("/(\\d+)px-"));
            const QRegularExpressionMatch m = bucket.match(thumb);
            for (int width : {3840, 1920, 1280, 960}) {
                if (!m.hasMatch() || width >= h.width)
                    continue;
                QString url = thumb;
                url.replace(m.capturedStart(1), m.capturedLength(1), QString::number(width));
                h.sizes << sized(QString(), QUrl(url), h.width, h.height, width);
            }
            if (!h.sizes.isEmpty())
                h.image = h.sizes.first().url;
            h.author = stripTags(meta[QStringLiteral("Artist")].toObject()[QStringLiteral("value")].toString());
            h.license = meta[QStringLiteral("LicenseShortName")].toObject()[QStringLiteral("value")].toString();
            if (h.license.isEmpty())
                h.license = QObject::tr("siehe Dateiseite");
            h.condition = QObject::tr("Urheber und Lizenz müssen genannt werden.");
            sorted.append({s[QStringLiteral("index")].toInt(), h});
        }
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& [index, h] : sorted)
            out << h;
    } else if (source == QLatin1String("openverse")) {
        for (const QJsonValue& v : d[QStringLiteral("results")].toArray()) {
            const QJsonObject t = v.toObject();
            Hit h;
            h.source = source;
            h.id = t[QStringLiteral("id")].toString();
            h.title = t[QStringLiteral("title")].toString();
            h.image = QUrl(t[QStringLiteral("url")].toString());
            // Kachel = das Bild selbst beim Anbieter (Flickr, Museum ...) wie in ThurWiki: Openverses
            // eigene Vorschau liegt auf dem gedrosselten Suchrechner, zählt aufs Tageskontingent und
            // kommt oft nicht. Sie bleibt als zweiter Versuch.
            h.preview = h.image;
            h.fallbackPreview = QUrl(t[QStringLiteral("thumbnail")].toString());
            h.page = QUrl(t[QStringLiteral("foreign_landing_url")].toString());
            h.width = t[QStringLiteral("width")].toInt();
            h.height = t[QStringLiteral("height")].toInt();
            h.sizes << Size{QObject::tr("Original"), h.image, h.width, h.height, false};
            h.author = t[QStringLiteral("creator")].toString();
            h.license = (t[QStringLiteral("license")].toString().toUpper() + QLatin1Char(' ') +
                         t[QStringLiteral("license_version")].toString()).trimmed();
            h.condition = QObject::tr("Gewerblich nutzbar. Je nach Lizenz Namensnennung; CC-BY-SA verlangt dieselbe "
                                      "Lizenz für das Ergebnis.");
            out << h;
        }
    } else if (source == QLatin1String("icons8")) {
        const bool paid = !keys.value(source).trimmed().isEmpty();
        for (const QJsonValue& v : d[QStringLiteral("icons")].toArray()) {
            const QJsonObject t = v.toObject();
            if (t[QStringLiteral("isAnimated")].toBool())
                continue;
            Hit h;
            h.source = source;
            h.id = str(t[QStringLiteral("id")]);
            h.title = t[QStringLiteral("name")].toString();
            h.preview = QUrl(QStringLiteral("https://img.icons8.com/?size=100&id=%1&format=png").arg(h.id));
            // Ohne Abo: kleines PNG (mit Link-Pflicht); mit Schlüssel: SVG über die Schnittstelle
            h.image = h.preview;
            h.vector = paid;
            h.width = h.height = 100;
            // Ohne Abo erlaubt Icons8 nur das kleine PNG
            auto png = [&](int px) {
                return Size{QStringLiteral("PNG"), QUrl(QStringLiteral("https://img.icons8.com/?size=%1&id=%2&format=png").arg(px).arg(h.id)), px, px, false};
            };
            if (paid)
                h.sizes = {Size{QObject::tr("SVG (Vektor)"), QUrl(), 0, 0, true}, png(1024), png(512), png(256)};
            else
                h.sizes = {png(100)};
            h.page = QUrl(QStringLiteral("https://icons8.com/icon/%1/%2").arg(h.id, t[QStringLiteral("commonName")].toString()));
            h.author = QStringLiteral("Icons8");
            h.license = paid ? QObject::tr("Icons8, Abo") : QObject::tr("Icons8, kostenlos mit Link");
            h.condition = paid ? QObject::tr("Mit Abo ohne Link nutzbar (Bedingungen des eigenen Icons8-Vertrags).")
                               : QObject::tr("Kostenlos nur mit Link auf icons8.com im fertigen Werk; nur kleines PNG. "
                                             "SVG und grosse Formate brauchen ein Abo.");
            out << h;
        }
    } else if (source == QLatin1String("flaticon")) {
        for (const QJsonValue& v : d[QStringLiteral("data")].toArray()) {
            const QJsonObject t = v.toObject();
            Hit h;
            h.source = source;
            h.id = str(t[QStringLiteral("id")]);
            h.title = t[QStringLiteral("name")].toString();
            const QJsonArray thumbs = t[QStringLiteral("thumbnails")].toArray();
            if (!thumbs.isEmpty()) {
                const QJsonObject th = thumbs.last().toObject();
                h.preview = h.image = QUrl(th[QStringLiteral("url")].toString());
                h.width = th[QStringLiteral("width")].toInt();
                h.height = th[QStringLiteral("height")].toInt();
            }
            const QString slug = t[QStringLiteral("slug")].toString();
            h.page = QUrl(QStringLiteral("https://www.flaticon.com/free-icon/%1_%2").arg(slug.isEmpty() ? QStringLiteral("icon") : slug, h.id));
            h.author = t[QStringLiteral("author")].toObject()[QStringLiteral("name")].toString();
            const QString style = t[QStringLiteral("style")].toObject()[QStringLiteral("name")].toString();
            h.freeToUse = t[QStringLiteral("free_svg")].toBool();
            // Premium lädt die Schnittstelle mit einem Premium-Abo genauso; sonst meldet sie es beim Download
            h.subscription = !h.freeToUse;
            h.vector = true;
            // Über den Download-Endpunkt: SVG oder PNG in wählbarer Grösse
            const QString endpoint = QStringLiteral("https://api.magnific.com/v1/icons/%1/download?format=").arg(h.id);
            h.sizes << Size{QObject::tr("SVG (Vektor)"), QUrl(endpoint + QStringLiteral("svg")), 0, 0, true};
            for (int px : {512, 256, 128})
                h.sizes << Size{QStringLiteral("PNG"), QUrl(endpoint + QStringLiteral("png&png_size=%1").arg(px)), px, px, false};
            h.license = h.freeToUse ? QObject::tr("Flaticon frei") : QObject::tr("Flaticon Premium");
            if (!style.isEmpty())
                h.license += QStringLiteral(" (") + style + QLatin1Char(')');
            h.condition = h.freeToUse ? QObject::tr("Frei mit Namensnennung (Urheber und Flaticon).")
                                      : QObject::tr("Premium: über das eigene Flaticon/Magnific-Abo, ohne Namensnennung.");
            out << h;
        }
    } else if (source == QLatin1String("magnific")) {
        for (const QJsonValue& v : d[QStringLiteral("data")].toArray()) {
            const QJsonObject t = v.toObject();
            const QJsonObject src = t[QStringLiteral("image")].toObject()[QStringLiteral("source")].toObject();
            if (src[QStringLiteral("url")].toString().isEmpty())
                continue;
            QStringList levels;
            for (const QJsonValue& l : t[QStringLiteral("licenses")].toArray())
                levels << l.toObject()[QStringLiteral("type")].toString().toLower();
            Hit h;
            h.source = source;
            h.id = str(t[QStringLiteral("id")]);
            h.title = t[QStringLiteral("title")].toString();
            h.preview = h.image = QUrl(src[QStringLiteral("url")].toString());
            const QJsonValue size = src[QStringLiteral("size")];
            if (size.isArray()) {
                h.width = size.toArray().at(0).toInt();
                h.height = size.toArray().at(1).toInt();
            } else {
                h.width = size.toString().section(QLatin1Char('x'), 0, 0).toInt();
                h.height = size.toString().section(QLatin1Char('x'), 1, 1).toInt();
            }
            h.page = QUrl(t[QStringLiteral("url")].toString());
            // Die Masse des Originals nennt die Suche nicht, nur die Dateigrösse
            QString original = QObject::tr("Original");
            const QJsonObject formats = t[QStringLiteral("meta")].toObject()[QStringLiteral("available_formats")].toObject();
            for (const QJsonValue& f : formats) {
                const double bytes = f.toObject()[QStringLiteral("items")].toArray().first().toObject()[QStringLiteral("size")].toDouble();
                if (bytes > 0) {
                    original += QStringLiteral(" (%1 MB)").arg(QLocale().toString(bytes / 1048576.0, 'f', 1));
                    break;
                }
            }
            h.sizes << Size{original, QUrl(QStringLiteral("https://api.magnific.com/v1/resources/%1/download").arg(h.id)), 0, 0, false};
            h.sizeNote = QObject::tr("Die Vorschau hat %1 × %2, eingefügt wird das Original.").arg(h.width).arg(h.height);
            h.author = t[QStringLiteral("author")].toObject()[QStringLiteral("name")].toString();
            h.freeToUse = !levels.isEmpty() && !levels.contains(QStringLiteral("premium"));
            h.subscription = !h.freeToUse;
            h.license = QStringLiteral("Magnific ") + (levels.isEmpty() ? QObject::tr("unbekannt") : levels.join(QLatin1Char('/')));
            h.condition = h.freeToUse ? QObject::tr("Freemium: Namensnennung von Magnific/Freepik nötig.")
                                      : QObject::tr("Premium: über das eigene Magnific-Abo, ohne Namensnennung.");
            out << h;
        }
    } else if (source == QLatin1String("pixabay")) {
        for (const QJsonValue& v : d[QStringLiteral("hits")].toArray()) {
            const QJsonObject t = v.toObject();
            Hit h;
            h.source = source;
            h.id = str(t[QStringLiteral("id")]);
            h.title = t[QStringLiteral("tags")].toString();
            h.image = QUrl(t[QStringLiteral("largeImageURL")].toString(t[QStringLiteral("webformatURL")].toString()));
            h.preview = QUrl(t[QStringLiteral("previewURL")].toString());
            h.width = t[QStringLiteral("imageWidth")].toInt();
            h.height = t[QStringLiteral("imageHeight")].toInt();
            // Original und Full HD nur mit freigeschaltetem Vollzugriff; webformatURL gibt es laut
            // Pixabay auch mit _960 statt _640
            const QString full = t[QStringLiteral("imageURL")].toString(), hd = t[QStringLiteral("fullHDURL")].toString();
            const QString large = t[QStringLiteral("largeImageURL")].toString(), web = t[QStringLiteral("webformatURL")].toString();
            if (!full.isEmpty())
                h.sizes << Size{QObject::tr("Original"), QUrl(full), h.width, h.height, false};
            if (!hd.isEmpty())
                h.sizes << sized(QStringLiteral("Full HD"), QUrl(hd), h.width, h.height, 1920, 1920);
            if (!large.isEmpty())
                h.sizes << sized(QString(), QUrl(large), h.width, h.height, 1280, 1280);
            if (web.contains(QLatin1String("_640")))
                h.sizes << sized(QString(), QUrl(QString(web).replace(QLatin1String("_640"), QLatin1String("_960"))), h.width, h.height, 960, 960);
            if (!web.isEmpty())
                h.sizes << sized(QString(), QUrl(web), h.width, h.height, 640, 640);
            if (full.isEmpty() && h.width > 1280)
                h.sizeNote = QObject::tr("Das Original (%1 × %2) gibt Pixabay über die Schnittstelle nur mit "
                                         "„Vollzugriff“ heraus (bei Pixabay beantragen); bis dahin höchstens 1280 Pixel.")
                                 .arg(h.width).arg(h.height);
            h.page = QUrl(t[QStringLiteral("pageURL")].toString());
            h.author = t[QStringLiteral("user")].toString();
            h.license = QStringLiteral("Pixabay Content License");
            h.condition = QObject::tr("Keine Namensnennung nötig, aber kein Weiterverkauf als Bild.");
            out << h;
        }
    } else if (source == QLatin1String("unsplash")) {
        for (const QJsonValue& v : d[QStringLiteral("results")].toArray()) {
            const QJsonObject t = v.toObject();
            Hit h;
            h.source = source;
            h.id = t[QStringLiteral("id")].toString();
            h.title = t[QStringLiteral("description")].toString(t[QStringLiteral("alt_description")].toString());
            if (h.title.isEmpty())
                h.title = t[QStringLiteral("alt_description")].toString();
            h.image = QUrl(t[QStringLiteral("urls")].toObject()[QStringLiteral("regular")].toString());
            h.preview = QUrl(t[QStringLiteral("urls")].toObject()[QStringLiteral("thumb")].toString());
            h.width = t[QStringLiteral("width")].toInt();
            h.height = t[QStringLiteral("height")].toInt();
            // raw nimmt beliebige Breiten (imgix): w=...
            const QJsonObject urls = t[QStringLiteral("urls")].toObject();
            const QString raw = urls[QStringLiteral("raw")].toString();
            if (!urls[QStringLiteral("full")].toString().isEmpty())
                h.sizes << Size{QObject::tr("Original"), QUrl(urls[QStringLiteral("full")].toString()), h.width, h.height, false};
            for (int width : {3840, 1920}) {
                if (!raw.isEmpty() && width < h.width)
                    h.sizes << sized(QString(), QUrl(raw + (raw.contains(QLatin1Char('?')) ? QStringLiteral("&") : QStringLiteral("?")) +
                                                     QStringLiteral("fm=jpg&q=90&w=%1").arg(width)),
                                     h.width, h.height, width);
            }
            h.sizes << sized(QString(), h.image, h.width, h.height, 1080);
            h.page = QUrl(t[QStringLiteral("links")].toObject()[QStringLiteral("html")].toString());
            h.author = t[QStringLiteral("user")].toObject()[QStringLiteral("name")].toString();
            h.license = QStringLiteral("Unsplash License");
            h.condition = QObject::tr("Unsplash bittet um Nennung von Fotograf und Unsplash.");
            out << h;
        }
    } else if (source == QLatin1String("pexels")) {
        for (const QJsonValue& v : d[QStringLiteral("photos")].toArray()) {
            const QJsonObject t = v.toObject();
            Hit h;
            h.source = source;
            h.id = str(t[QStringLiteral("id")]);
            h.title = t[QStringLiteral("alt")].toString();
            h.image = QUrl(t[QStringLiteral("src")].toObject()[QStringLiteral("large")].toString());
            h.preview = QUrl(t[QStringLiteral("src")].toObject()[QStringLiteral("tiny")].toString());
            h.width = t[QStringLiteral("width")].toInt();
            h.height = t[QStringLiteral("height")].toInt();
            // original nimmt Grössenangaben an: ?auto=compress&cs=tinysrgb&w=...
            const QJsonObject src = t[QStringLiteral("src")].toObject();
            const QString original = src[QStringLiteral("original")].toString();
            if (!original.isEmpty()) {
                h.sizes << Size{QObject::tr("Original"), QUrl(original), h.width, h.height, false};
                for (int width : {3840, 1920})
                    if (width < h.width)
                        h.sizes << sized(QString(), QUrl(original + QStringLiteral("?auto=compress&cs=tinysrgb&w=%1").arg(width)),
                                         h.width, h.height, width);
            }
            h.sizes << sized(QString(), h.image, h.width, h.height, 940, 650);
            h.page = QUrl(t[QStringLiteral("url")].toString());
            h.author = t[QStringLiteral("photographer")].toString();
            h.license = QStringLiteral("Pexels License");
            h.condition = QObject::tr("Keine Namensnennung nötig, wird aber gern gesehen.");
            out << h;
        }
    } else if (source == QLatin1String("adobe")) {
        for (const QJsonValue& v : d[QStringLiteral("files")].toArray()) {
            const QJsonObject t = v.toObject();
            Hit h;
            h.source = source;
            h.id = str(t[QStringLiteral("id")]);
            h.title = t[QStringLiteral("title")].toString();
            h.preview = h.image = QUrl(t[QStringLiteral("thumbnail_500_url")].toString());
            h.width = t[QStringLiteral("width")].toInt();
            h.height = t[QStringLiteral("height")].toInt();
            h.page = QUrl(QStringLiteral("https://stock.adobe.com/de/") + h.id);
            h.author = t[QStringLiteral("creator_name")].toString();
            h.license = QObject::tr("Adobe Stock, lizenzpflichtig");
            h.freeToUse = false;
            h.condition = QObject::tr("Vorschau MIT Wasserzeichen: ohne gekaufte Lizenz nicht verwenden.");
            out << h;
        }
    } else if (source == QLatin1String("getty") || source == QLatin1String("istock")) {
        for (const QJsonValue& v : d[QStringLiteral("images")].toArray()) {
            const QJsonObject t = v.toObject();
            QJsonArray sizes = t[QStringLiteral("display_sizes")].toArray();
            if (sizes.isEmpty())
                sizes = t[QStringLiteral("display_set")].toArray();
            QString small, large;
            for (const QJsonValue& s : sizes) {
                const QString u = s.toObject()[QStringLiteral("uri")].toString();
                if (u.isEmpty())
                    continue;
                large = u;
                if (small.isEmpty())
                    small = u;
            }
            Hit h;
            h.source = source;
            h.id = t[QStringLiteral("id")].toString();
            h.title = t[QStringLiteral("title")].toString();
            h.image = QUrl(large);
            h.preview = QUrl(small.isEmpty() ? large : small);
            h.width = t[QStringLiteral("max_dimensions")].toObject()[QStringLiteral("width")].toInt();
            h.height = t[QStringLiteral("max_dimensions")].toObject()[QStringLiteral("height")].toInt();
            h.page = QUrl(t[QStringLiteral("referral_destinations")].toArray().first().toObject()[QStringLiteral("uri")].toString());
            h.author = t[QStringLiteral("artist")].toString();
            h.license = (source == QLatin1String("istock") ? QStringLiteral("iStock") : QStringLiteral("Getty Images")) +
                        QObject::tr(", lizenzpflichtig");
            h.freeToUse = false;
            h.condition = QObject::tr("Vorschau MIT Wasserzeichen: ohne gekaufte Lizenz nicht verwenden.");
            out << h;
        }
    }
    return out;
}

QList<Size> choices(const Hit& hit)
{
    if (!hit.sizes.isEmpty())
        return hit.sizes;
    return {Size{QString(), hit.image, hit.width, hit.height, hit.vector}};
}

namespace {

// Fehlertext des Dienstes aus der Antwort holen (JSON-Felder der verschiedenen Anbieter oder Klartext)
QString serviceMessage(const QByteArray& body)
{
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isObject()) {
        const QJsonObject o = doc.object();
        for (const char* field : {"message", "error_description", "detail", "error"}) {
            const QJsonValue v = o.value(QLatin1String(field));
            if (v.isString() && !v.toString().isEmpty())
                return v.toString();
            if (v.isObject() && v.toObject().value(QStringLiteral("message")).isString())
                return v.toObject().value(QStringLiteral("message")).toString();
        }
        const QJsonArray errors = o.value(QStringLiteral("errors")).toArray();
        if (!errors.isEmpty())
            return errors.first().isString() ? errors.first().toString() : errors.first().toObject().value(QStringLiteral("message")).toString();
        return {};
    }
    QString text = stripTags(QString::fromUtf8(body.left(300)));
    return text.length() > 200 ? text.left(200) + QStringLiteral(" …") : text;
}

QString keySource(const QString& source)
{
    return source == QLatin1String("flaticon") ? QStringLiteral("magnific") : source;
}

} // namespace

QString keyProblem(const QString& src, int status, const QByteArray& body, const QString& networkError, bool timedOut)
{
    const Source* s = source(src);
    const QString name = s ? s->name : src;
    const Source* keyed = source(keySource(src));
    const QString where = keyed ? keyed->hint : QString();
    const QString said = serviceMessage(body);
    const QString quote = said.isEmpty() ? QString() : QStringLiteral("\n") + QObject::tr("Antwort von %1: „%2“").arg(name, said);
    if (status == 0) {
        if (timedOut)
            return QObject::tr("%1 hat innert 12 Sekunden nicht geantwortet. Meist ist der Dienst überlastet oder "
                               "drosselt; später nochmals versuchen.").arg(name);
        return QObject::tr("Keine Verbindung zu %1 (%2). Internetverbindung, Proxy oder Firewall prüfen; der "
                           "Schlüssel selbst wurde gar nicht geprüft.").arg(name, networkError);
    }
    if (status >= 200 && status < 300)
        return {};
    if (status == 401 || (status == 400 && src == QLatin1String("pixabay") && said.contains(QLatin1String("key"), Qt::CaseInsensitive)))
        return QObject::tr("%1 kennt diesen Schlüssel nicht (HTTP %2). Meist ist er unvollständig kopiert, enthält "
                           "Leerzeichen, oder es ist ein anderer Wert als der API-Schlüssel (z.B. das Secret statt "
                           "des Access Key). %3").arg(name).arg(status).arg(where) + quote;
    if (status == 403)
        return QObject::tr("%1 kennt den Schlüssel, verweigert aber den Zugriff (HTTP 403): die Anwendung ist "
                           "gesperrt, nicht für diese Schnittstelle freigeschaltet, oder das Kontingent ist "
                           "aufgebraucht.").arg(name) + quote;
    if (status == 429)
        return QObject::tr("Der Schlüssel stimmt, aber das Abrufkontingent von %1 ist im Moment aufgebraucht "
                           "(HTTP 429). Später nochmals versuchen.").arg(name) + quote;
    if (status >= 500)
        return QObject::tr("%1 hat gerade eine Störung (HTTP %2). Der Schlüssel ist vermutlich in Ordnung; später "
                           "nochmals versuchen.").arg(name).arg(status) + quote;
    return QObject::tr("%1 meldet HTTP %2.").arg(name).arg(status) + quote;
}

QString sizeLabel(const Size& size)
{
    QStringList parts;
    if (!size.label.isEmpty())
        parts << size.label;
    if (size.width > 0 && size.height > 0)
        parts << QStringLiteral("%1 × %2").arg(size.width).arg(size.height);
    return parts.isEmpty() ? QObject::tr("Standard") : parts.join(QStringLiteral(" · "));
}

QString attribution(const Hit& hit)
{
    const Source* s = source(hit.source);
    QStringList parts;
    if (!hit.title.isEmpty())
        parts << hit.title.left(80);
    if (!hit.author.isEmpty())
        parts << QObject::tr("von %1").arg(hit.author.left(60));
    QString tail = s ? s->name : hit.source;
    if (!hit.license.isEmpty())
        tail += QStringLiteral(", ") + hit.license;
    return parts.join(QLatin1Char(' ')) + QStringLiteral(" (") + tail + QLatin1Char(')');
}

NodeId insert(Editor& editor, const Hit& hit, const QByteArray& data)
{
    const QString name = attribution(hit);
    const QByteArray head = data.left(512).trimmed();
    if (head.startsWith("<?xml") || head.startsWith("<svg") || head.contains("<svg")) {
        svg::Import imported = svg::read(data);
        if (imported.group && imported.group->childCount() > 0) {
            imported.group->setName(name);
            if (imported.group->childCount() == 1)
                imported.group->child(0)->setName(name);
            return editor.importGroup(std::move(imported.group), true, QObject::tr("Bild aus Bildersuche"));
        }
        return InvalidNodeId;
    }
    return editor.importImage(data, std::nullopt, name);
}

} // namespace imagesearch

using namespace imagesearch;

ImageSearch::ImageSearch(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

ImageSearch::~ImageSearch()
{
    cancel();
}

void ImageSearch::cancel()
{
    ++m_generation;
    const QSet<QNetworkReply*> running = std::exchange(m_replies, {});
    for (QNetworkReply* reply : running) {
        reply->disconnect(this); // keine Rückmeldung mehr an Dialog oder Suche
        reply->abort();
        reply->deleteLater();
    }
    m_searchReplies.clear();
}

QNetworkReply* ImageSearch::get(const Request& request)
{
    QNetworkRequest r(request.url);
    r.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
    r.setRawHeader("Accept", "application/json");
    for (const auto& [name, value] : request.headers)
        r.setRawHeader(name, value);
    r.setTransferTimeout(kTimeoutMs);
    r.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_network->get(r);
    m_replies.insert(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply] { m_replies.remove(reply); });
    return reply;
}

QString ImageSearch::complaint(const QString& src, QNetworkReply* reply) const
{
    const Source* s = source(src);
    const QString name = s ? s->name : src;
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 401 || status == 403)
        return tr("%1 weist den Schlüssel ab (HTTP %2).").arg(name).arg(status);
    if (status == 429)
        return src == QLatin1String("openverse")
                   ? tr("Openverse drosselt gerade (429) - ohne Schlüssel gelten 20 Suchen je Minute und 200 je Tag.")
                   : tr("%1 drosselt gerade (429).").arg(name);
    if (status >= 400)
        return tr("%1 meldet HTTP %2.").arg(name).arg(status);
    // Openverse drosselt anonyme Abfragen, indem es die Verbindung stehen lässt (kein 429)
    if (src == QLatin1String("openverse") && (reply->error() == QNetworkReply::OperationCanceledError || reply->error() == QNetworkReply::TimeoutError))
        return tr("Openverse antwortet nicht (meist Drosselung: ohne Schlüssel 20 Suchen je Minute, 200 je Tag).");
    if (reply->error() != QNetworkReply::NoError)
        return tr("%1 ist nicht erreichbar: %2").arg(name, reply->errorString());
    return {};
}

void ImageSearch::search(const QString& term, const QStringList& wanted, int page, int perPage)
{
    const int generation = ++m_generation;
    // Eine abgelöste Suche (neuer Begriff, Nachladen) braucht ihre Abrufe nicht mehr
    for (const QPointer<QNetworkReply>& old : std::exchange(m_searchReplies, {}))
        if (old && old->isRunning())
            old->abort();
    struct Pending {
        bool overdue = false; // Gesamtzeit abgelaufen: Rest wird übersprungen
        int open = 0;
        QHash<QString, int> openPerSource;
        QHash<QString, QHash<QString, QByteArray>> replies; // Quelle -> tag -> JSON
        QStringList complaints;
        QStringList order;
    };
    auto state = std::make_shared<Pending>();
    const QStringList ready = available(m_keys);
    for (const QString& src : wanted) {
        if (!ready.contains(src)) {
            const Source* s = source(src);
            state->complaints << tr("Für %1 ist kein Schlüssel hinterlegt (Optionen > Global > Bildersuche).")
                                     .arg(s ? s->name : src);
            continue;
        }
        state->order << src;
        for (const Request& request : requests(src, term, perPage, page, m_keys)) {
            ++state->open;
            ++state->openPerSource[src];
            QNetworkReply* reply = get(request);
            m_searchReplies << reply;
            connect(reply, &QNetworkReply::finished, this, [this, reply, src, tag = request.tag, state, generation] {
                reply->deleteLater();
                QString problem = complaint(src, reply);
                if (state->overdue && reply->error() == QNetworkReply::OperationCanceledError) {
                    const Source* s = source(src);
                    problem = tr("%1 hat nicht innert %2 Sekunden geantwortet und wurde übersprungen.")
                                  .arg(s ? s->name : src)
                                  .arg(m_deadlineMs / 1000);
                }
                // Der Hilfsabruf für Commons-Kacheln ist keine Klage wert
                if (!problem.isEmpty() && tag == src)
                    state->complaints << problem;
                else if (problem.isEmpty())
                    state->replies[src].insert(tag, reply->readAll());
                if (generation != m_generation)
                    return;
                // Jede Quelle meldet sich, sobald sie fertig ist: eine hängende hält die anderen nicht auf
                if (--state->openPerSource[src] == 0)
                    emit arrived(parse(src, state->replies.value(src), m_keys));
                if (--state->open > 0)
                    return;
                QList<Hit> hits;
                for (const QString& s : std::as_const(state->order))
                    hits << parse(s, state->replies.value(s), m_keys);
                emit finished(hits, state->complaints);
            });
        }
    }
    // Gesamt-Zeitgrenze: eine hängende Quelle (z.B. gedrosseltes Openverse) hält die Suche nicht auf
    if (state->open > 0)
        QTimer::singleShot(m_deadlineMs, this, [this, state, generation] {
            if (generation != m_generation || state->open == 0)
                return;
            state->overdue = true;
            for (const QPointer<QNetworkReply>& reply : std::as_const(m_searchReplies))
                if (reply && reply->isRunning())
                    reply->abort();
        });
    if (state->open == 0)
        QMetaObject::invokeMethod(this, [this, state, generation] {
            if (generation == m_generation)
                emit finished({}, state->complaints);
        }, Qt::QueuedConnection);
}

void ImageSearch::fetch(const QUrl& url, std::function<void(const QByteArray&)> done)
{
    QNetworkReply* reply = get({QString(), url, {{"Accept", "image/*"}}});
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        done(reply->error() == QNetworkReply::NoError ? reply->readAll() : QByteArray());
    });
}

void ImageSearch::testKey(const QString& src, const QString& rawKey, std::function<void(bool, const QString&)> done)
{
    const QString key = rawKey.trimmed();
    if (key.isEmpty()) {
        done(false, tr("Kein Schlüssel eingetragen."));
        return;
    }
    if (key.contains(QRegularExpression(QStringLiteral("\\s")))) {
        done(false, tr("Der Schlüssel enthält Leerzeichen oder Zeilenumbrüche. Bitte nur den Schlüssel selbst einfügen."));
        return;
    }
    // Magnific-Schlüssel gilt auch für Flaticon: beide prüfen
    QStringList tested{src};
    if (src == QLatin1String("magnific"))
        tested << QStringLiteral("flaticon");
    struct Pending {
        int open = 0;
        QStringList good, bad;
        bool failed = false;
    };
    auto state = std::make_shared<Pending>();
    for (const QString& t : tested) {
        QHash<QString, QString> keys{{keySource(t), key}};
        Request request = requests(t, QStringLiteral("Schaffhausen"), 3, 1, keys).value(0);
        // Die Icons8-Suche nimmt jeden Schlüssel an; prüfen lässt er sich nur an der SVG-Schnittstelle
        if (t == QLatin1String("icons8"))
            request = {t, withQuery(QStringLiteral("https://api-icons.icons8.com/publicApi/icons/icon"),
                                    {{QStringLiteral("id"), QStringLiteral("39210")}, {QStringLiteral("token"), key}}), {}};
        ++state->open;
        QNetworkReply* reply = get(request);
        connect(reply, &QNetworkReply::finished, this, [reply, t, state, done] {
            reply->deleteLater();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray body = reply->readAll();
            const bool timedOut = reply->error() == QNetworkReply::OperationCanceledError || reply->error() == QNetworkReply::TimeoutError;
            const QString problem = keyProblem(t, status, body, reply->errorString(), timedOut);
            const Source* s = source(t);
            const QString name = s ? s->name : t;
            if (!problem.isEmpty()) {
                state->failed = true;
                state->bad << problem;
            } else {
                // Was der Schlüssel hergibt
                const QJsonObject o = object(body);
                QString line = tr("%1: Schlüssel funktioniert.").arg(name);
                int total = -1;
                if (t == QLatin1String("pixabay")) {
                    total = o[QStringLiteral("totalHits")].toInt();
                    const QJsonObject first = o[QStringLiteral("hits")].toArray().first().toObject();
                    line += QLatin1Char(' ') + (first.contains(QStringLiteral("imageURL"))
                                                     ? tr("Vollzugriff freigeschaltet: Originale in voller Auflösung.")
                                                     : tr("Ohne Vollzugriff: höchstens 1280 Pixel (Vollzugriff bei Pixabay beantragen)."));
                } else if (t == QLatin1String("unsplash")) {
                    total = o[QStringLiteral("total")].toInt();
                } else if (t == QLatin1String("pexels")) {
                    total = o[QStringLiteral("total_results")].toInt();
                } else if (t == QLatin1String("magnific") || t == QLatin1String("flaticon")) {
                    total = o[QStringLiteral("meta")].toObject()[QStringLiteral("total")].toInt(
                        o[QStringLiteral("meta")].toObject()[QStringLiteral("pagination")].toObject()[QStringLiteral("total")].toInt(-1));
                } else if (t == QLatin1String("getty") || t == QLatin1String("istock")) {
                    total = o[QStringLiteral("result_count")].toInt(-1);
                } else if (t == QLatin1String("adobe")) {
                    total = o[QStringLiteral("nb_results")].toInt(-1);
                } else if (t == QLatin1String("icons8")) {
                    line += QLatin1Char(' ') + tr("SVG-Download freigeschaltet.");
                }
                if (total >= 0)
                    line += QLatin1Char(' ') + tr("Probesuche „Schaffhausen“: %1 Treffer.").arg(total);
                const QByteArray remaining = reply->rawHeader("X-Ratelimit-Remaining");
                const QByteArray limit = reply->rawHeader("X-Ratelimit-Limit");
                if (!remaining.isEmpty())
                    line += QLatin1Char(' ') + (limit.isEmpty() ? tr("Noch %1 Abrufe im laufenden Zeitfenster.").arg(QString::fromLatin1(remaining))
                                                                : tr("Noch %1 von %2 Abrufen im laufenden Zeitfenster.")
                                                                      .arg(QString::fromLatin1(remaining), QString::fromLatin1(limit)));
                state->good << line;
            }
            if (--state->open == 0)
                done(!state->failed, (state->good + state->bad).join(QLatin1Char('\n')));
        });
    }
}

void ImageSearch::download(const Hit& hit, std::function<void(const QByteArray&, const QString&)> done, int size)
{
    const QList<Size> all = choices(hit);
    const Size chosen = all.value(std::clamp(size, 0, int(all.size()) - 1));
    if (!insertable(hit)) {
        done({}, tr("Dieses Bild ist lizenzpflichtig und lässt sich nicht einfügen."));
        return;
    }
    auto fetchFinal = [this, done](const QUrl& url) {
        QNetworkReply* reply = get({QString(), url, {{"Accept", "*/*"}}});
        connect(reply, &QNetworkReply::finished, this, [this, reply, done] {
            reply->deleteLater();
            const QString problem = complaint(tr("Der Download"), reply);
            done(problem.isEmpty() ? reply->readAll() : QByteArray(), problem);
        });
    };
    const QString magnificKey = m_keys.value(QStringLiteral("magnific")).trimmed();
    if (hit.source == QLatin1String("flaticon") || hit.source == QLatin1String("magnific")) {
        // Magnific verlangt, dass jede Verwendung über den Download-Endpunkt läuft
        QUrl url = chosen.url;
        if (url.isEmpty())
            url = hit.source == QLatin1String("flaticon")
                      ? QUrl(QStringLiteral("https://api.magnific.com/v1/icons/%1/download?format=svg").arg(hit.id))
                      : QUrl(QStringLiteral("https://api.magnific.com/v1/resources/%1/download").arg(hit.id));
        QNetworkReply* reply = get({hit.source, url, {{"x-magnific-api-key", magnificKey.toUtf8()}}});
        connect(reply, &QNetworkReply::finished, this, [this, reply, hit, done, fetchFinal] {
            reply->deleteLater();
            const QString problem = complaint(hit.source, reply);
            const QUrl file(object(reply->readAll())[QStringLiteral("data")].toObject()[QStringLiteral("url")].toString());
            if (!problem.isEmpty() || !file.isValid() || file.isEmpty()) {
                done({}, problem.isEmpty() ? tr("Magnific liefert keine Download-Adresse.") : problem);
                return;
            }
            fetchFinal(file);
        });
        return;
    }
    const QString icons8Key = m_keys.value(QStringLiteral("icons8")).trimmed();
    if (hit.source == QLatin1String("icons8") && chosen.vector && !icons8Key.isEmpty()) {
        const QUrl url = withQuery(QStringLiteral("https://api-icons.icons8.com/publicApi/icons/icon"),
                                   {{QStringLiteral("id"), hit.id}, {QStringLiteral("token"), icons8Key}});
        QNetworkReply* reply = get({hit.source, url, {}});
        connect(reply, &QNetworkReply::finished, this, [this, reply, hit, done] {
            reply->deleteLater();
            const QString problem = complaint(hit.source, reply);
            const QJsonObject o = object(reply->readAll());
            QString svg = o[QStringLiteral("icon")].toObject()[QStringLiteral("svg")].toString();
            if (svg.isEmpty())
                svg = o[QStringLiteral("svg")].toString();
            QByteArray data = svg.toUtf8();
            if (!data.isEmpty() && !data.trimmed().startsWith('<'))
                data = QByteArray::fromBase64(data); // ältere Antworten: Base64
            done(problem.isEmpty() ? data : QByteArray(), data.isEmpty() && problem.isEmpty() ? tr("Icons8 liefert kein SVG.") : problem);
        });
        return;
    }
    if (hit.source == QLatin1String("unsplash")) {
        // Unsplash verlangt, dass ein Download gemeldet wird
        const QUrl track(QStringLiteral("https://api.unsplash.com/photos/%1/download").arg(hit.id));
        QNetworkReply* reply = get({hit.source, track, {{"Authorization", "Client-ID " + m_keys.value(QStringLiteral("unsplash")).toUtf8()}}});
        connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    }
    fetchFinal(chosen.url.isEmpty() ? hit.image : chosen.url);
}

} // namespace vek

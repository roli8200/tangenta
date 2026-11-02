#include "WebSearch.h"

#include "Options.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <QUrlQuery>

namespace vek {

namespace {

const QByteArray kUserAgent = "Mozilla/5.0 (compatible; TANGENTA/1.0; +Vektorgrafik)";
constexpr qint64 kMaxBytes = 6 * 1024 * 1024;

QString shorten(QString s, int n)
{
    s = s.simplified();
    return s.length() > n ? s.left(n) + QStringLiteral(" …") : s;
}

} // namespace

WebSearch::WebSearch(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

void WebSearch::search(const QString& query, int count, std::function<void(const QList<Result>&, const QString&)> done,
                       int provider, const QString& address, const QString& key)
{
    Options& o = Options::instance();
    if (provider < 0)
        provider = o.integer(QStringLiteral("global/web/provider"));
    count = std::clamp(count, 1, 20);
    QNetworkRequest request;
    QString service;
    if (provider == 1) {
        service = QStringLiteral("Brave Search");
        const QString k = (key.isEmpty() ? o.text(QStringLiteral("global/web/braveKey")) : key).trimmed();
        if (k.isEmpty()) {
            done({}, tr("Für Brave Search ist kein Schlüssel hinterlegt (Optionen > Global > Websuche)."));
            return;
        }
        QUrl url(QStringLiteral("https://api.search.brave.com/res/v1/web/search"));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("q"), QString::fromUtf8(QUrl::toPercentEncoding(query)));
        q.addQueryItem(QStringLiteral("count"), QString::number(count));
        q.addQueryItem(QStringLiteral("country"), QStringLiteral("ch"));
        q.addQueryItem(QStringLiteral("search_lang"), QStringLiteral("de"));
        url.setQuery(q);
        request.setUrl(url);
        request.setRawHeader("X-Subscription-Token", k.toUtf8());
    } else {
        service = QStringLiteral("SearXNG");
        QString base = (address.isEmpty() ? o.text(QStringLiteral("global/web/searxng")) : address).trimmed();
        while (base.endsWith(QLatin1Char('/')))
            base.chop(1);
        if (base.isEmpty()) {
            done({}, tr("Keine SearXNG-Adresse hinterlegt (Optionen > Global > Websuche)."));
            return;
        }
        QUrl url(base + QStringLiteral("/search"));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("q"), QString::fromUtf8(QUrl::toPercentEncoding(query)));
        q.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
        q.addQueryItem(QStringLiteral("language"), QStringLiteral("de"));
        q.addQueryItem(QStringLiteral("safesearch"), QStringLiteral("0"));
        url.setQuery(q);
        request.setUrl(url);
        const QString k = (key.isEmpty() ? o.text(QStringLiteral("global/web/searxngKey")) : key).trimmed();
        if (!k.isEmpty())
            request.setRawHeader("Authorization", "Bearer " + k.toUtf8());
    }
    request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(15000);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, done, provider, count, service] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        if (status == 0) {
            done({}, tr("%1 ist nicht erreichbar (%2). Adresse und Netzwerk prüfen.").arg(service, reply->errorString()));
            return;
        }
        if (status == 401 || status == 422)
            return done({}, tr("%1 kennt den Schlüssel nicht (HTTP %2).").arg(service).arg(status));
        if (status == 403 && provider == 0)
            return done({}, tr("SearXNG verweigert die JSON-Ausgabe (HTTP 403). In der settings.yml des Servers unter "
                               "search > formats „json“ ergänzen."));
        if (status == 429)
            return done({}, tr("%1 drosselt gerade (HTTP 429).").arg(service));
        if (status != 200)
            return done({}, tr("%1 meldet HTTP %2.").arg(service).arg(status));
        const QJsonObject o = QJsonDocument::fromJson(body).object();
        const QJsonArray items = provider == 1 ? o.value(QStringLiteral("web")).toObject().value(QStringLiteral("results")).toArray()
                                               : o.value(QStringLiteral("results")).toArray();
        QList<Result> results;
        for (const QJsonValue& v : items) {
            const QJsonObject r = v.toObject();
            const QString text = provider == 1 ? r.value(QStringLiteral("description")).toString() : r.value(QStringLiteral("content")).toString();
            results << Result{QTextDocumentFragment::fromHtml(r.value(QStringLiteral("title")).toString()).toPlainText(),
                              r.value(QStringLiteral("url")).toString(),
                              shorten(QTextDocumentFragment::fromHtml(text).toPlainText(), 400)};
            if (results.size() >= count)
                break;
        }
        done(results, {});
    });
}

bool WebSearch::isPrivate(const QUrl& url)
{
    const QString host = url.host().toLower();
    if (host.isEmpty() || host == QLatin1String("localhost") || host.endsWith(QLatin1String(".local")) ||
        host.endsWith(QLatin1String(".localhost")))
        return true;
    QHostAddress address;
    if (!address.setAddress(host))
        return false; // Namen werden nicht aufgelöst
    if (address.isLoopback() || address.isLinkLocal() || address.isNull())
        return true;
    for (const auto& [net, bits] : {QHostAddress::parseSubnet(QStringLiteral("10.0.0.0/8")), QHostAddress::parseSubnet(QStringLiteral("172.16.0.0/12")),
                                    QHostAddress::parseSubnet(QStringLiteral("192.168.0.0/16")), QHostAddress::parseSubnet(QStringLiteral("100.64.0.0/10")),
                                    QHostAddress::parseSubnet(QStringLiteral("fc00::/7"))})
        if (address.isInSubnet(net, bits))
            return true;
    return false;
}

QString WebSearch::htmlToText(const QString& html, QString* title)
{
    QString h = html;
    static const QRegularExpression titleRe(QStringLiteral("<title[^>]*>(.*?)</title>"),
                                            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    if (title)
        *title = QTextDocumentFragment::fromHtml(titleRe.match(h).captured(1)).toPlainText().simplified();
    static const QRegularExpression noise(QStringLiteral("<(script|style|noscript|svg|nav|footer|header|form|iframe|template)\\b.*?</\\1>"),
                                          QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    h.remove(noise);
    static const QRegularExpression comments(QStringLiteral("<!--.*?-->"), QRegularExpression::DotMatchesEverythingOption);
    h.remove(comments);
    QString text = QTextDocumentFragment::fromHtml(h).toPlainText();
    static const QRegularExpression blank(QStringLiteral("\\n\\s*\\n(\\s*\\n)+"));
    text.replace(blank, QStringLiteral("\n\n"));
    return text.trimmed();
}

void WebSearch::read(const QUrl& url, int maxChars, std::function<void(const QString&, const QString&, const QString&)> done)
{
    if (!url.isValid() || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))) {
        done({}, {}, tr("Nur http- und https-Adressen lassen sich lesen."));
        return;
    }
    if (isPrivate(url)) {
        done({}, {}, tr("Adressen im eigenen Netz (%1) liest die KI nicht.").arg(url.host()));
        return;
    }
    maxChars = std::clamp(maxChars, 500, 60000);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
    request.setRawHeader("Accept", "text/html,application/xhtml+xml,text/plain;q=0.9,*/*;q=0.5");
    request.setRawHeader("Accept-Language", "de-CH,de;q=0.9,en;q=0.7");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(20000);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received) {
        if (received > kMaxBytes)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [reply, done, maxChars] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (isPrivate(reply->url()))
            return done({}, {}, tr("Die Seite leitet ins eigene Netz um; nicht gelesen."));
        if (reply->error() != QNetworkReply::NoError && status == 0)
            return done({}, {}, tr("Seite nicht erreichbar: %1").arg(reply->errorString()));
        if (status >= 400)
            return done({}, {}, tr("Die Seite meldet HTTP %1.").arg(status));
        const QString type = reply->header(QNetworkRequest::ContentTypeHeader).toString().toLower();
        const QByteArray body = reply->readAll();
        if (type.startsWith(QLatin1String("image/")))
            return done({}, {}, tr("Das ist ein Bild (%1), keine Textseite; einfügen mit „bild_von_url“.").arg(type));
        if (type.contains(QLatin1String("pdf")))
            return done({}, {}, tr("PDF-Dateien kann TANGENTA nicht als Text lesen."));
        QString title, text;
        if (type.contains(QLatin1String("html")) || type.isEmpty() || body.left(512).toLower().contains("<html"))
            text = htmlToText(QString::fromUtf8(body), &title);
        else
            text = QString::fromUtf8(body);
        if (text.length() > maxChars)
            text = text.left(maxChars) + QStringLiteral("\n[… gekürzt]");
        done(title, text, {});
    });
}

void WebSearch::test(int provider, const QString& address, const QString& key, std::function<void(bool, const QString&)> done)
{
    search(QStringLiteral("Schaffhausen Munot"), 3, [done, provider](const QList<Result>& results, const QString& error) {
        if (!error.isEmpty())
            return done(false, error);
        if (results.isEmpty())
            return done(false, tr("Die Suche funktioniert, liefert aber keine Treffer (Suchmaschinen im Server prüfen)."));
        done(true, tr("%1 funktioniert. Erster Treffer: %2").arg(provider == 1 ? QStringLiteral("Brave Search") : QStringLiteral("SearXNG"),
                                                                  results.first().title));
    }, provider, address, key);
}

} // namespace vek

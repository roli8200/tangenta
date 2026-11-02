#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;

namespace vek {

// Websuche und Webseiten lesen für die KI (wie WebSuche/WebZugriff in ThurWiki).
// Suche über einen SearXNG-Server (Adresse in Optionen > Global > Websuche, Ausgabe "json" muss
// dort erlaubt sein) oder über die Brave Search API (eigener Schlüssel).
class WebSearch : public QObject {
    Q_OBJECT

public:
    explicit WebSearch(QObject* parent = nullptr);

    struct Result {
        QString title;
        QString url;
        QString text;
    };
    // provider: 0 = SearXNG, 1 = Brave; ohne Angabe aus den Optionen
    void search(const QString& query, int count, std::function<void(const QList<Result>&, const QString& error)> done,
                int provider = -1, const QString& address = QString(), const QString& key = QString());
    // Seite holen und als Text liefern (höchstens maxChars Zeichen)
    void read(const QUrl& url, int maxChars, std::function<void(const QString& title, const QString& text, const QString& error)> done);
    // Probesuche für den Test-Knopf
    void test(int provider, const QString& address, const QString& key, std::function<void(bool ok, const QString& message)> done);

    // HTML zu lesbarem Text (ohne Skripte, Stile, Navigation)
    static QString htmlToText(const QString& html, QString* title = nullptr);
    // Adressen im eigenen Netz (localhost, 10.x, 192.168.x ...) liest die KI nicht
    static bool isPrivate(const QUrl& url);

private:
    QNetworkAccessManager* m_network = nullptr;
};

} // namespace vek

#pragma once

#include "Node.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QNetworkReply> // vollständig für QPointer<QNetworkReply> (Qt 6.8)
#include <QObject>
#include <QPair>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;

namespace vek {

class Editor;

// Bildersuche wie in ThurWiki: sucht in mehreren Bildarchiven zugleich, jeder Treffer trägt
// Quelle, Urheber, Lizenz und ob er ohne Weiteres verwendet werden darf. Dazu Symbole von
// Flaticon (über die Magnific-Schnittstelle, Flaticon gehört zu Freepik/Magnific) und Icons8.
//
// Abgefragt werden nur offizielle Schnittstellen; keine Webseite wird ausgelesen. Die
// Schlüssel stehen lokal in den Optionen (Global > Bildersuche), je Benutzer.
// Getty, iStock und Adobe Stock liefern nur Vorschauen mit Wasserzeichen: sichtbar, damit man
// ein Motiv findet, aber nicht einfügbar.
namespace imagesearch {

enum class Kind { Photo, Icon };

struct Source {
    QString key;
    QString name;
    bool needsKey = true;
    Kind kind = Kind::Photo;
    QString hint; // Woher der Schlüssel kommt, Besonderheiten
};
const QList<Source>& sources();
const Source* source(const QString& key);
// Optionsschlüssel des API-Schlüssels einer Quelle ("global/imageSearch/key/<quelle>")
QString optionKey(const QString& source);

// Eine wählbare Fassung eines Treffers (Auflösung oder Format)
struct Size {
    QString label; // z.B. "Original", "SVG (Vektor)", "PNG"
    QUrl url;      // Datei; bei Flaticon der Download-Endpunkt, bei Icons8-SVG leer
    int width = 0;
    int height = 0;
    bool vector = false;
};

struct Hit {
    QString source;
    QString id;
    QString title;
    QUrl image;    // zum Einfügen (bei Magnific/Icons8 mit Schlüssel: siehe Download)
    QUrl preview;  // Kachel
    QUrl fallbackPreview; // zweiter Versuch, wenn die Kachel nicht kommt
    QUrl page;     // Seite beim Anbieter
    int width = 0;
    int height = 0;
    QString author;
    QString license;
    QString condition; // was die Lizenz verlangt
    bool freeToUse = true;
    bool subscription = false; // Premium, aber mit dem eigenen Abo (Schlüssel) einfügbar
    QString sizeNote;          // z.B. warum das Original fehlt
    bool vector = false; // als SVG einfügbar (bearbeitbare Kurven)
    QList<Size> sizes;   // wählbare Fassungen, grösste zuerst (leer: nur image)
};

// Bild für den Betrachter: gross genug für den Bildschirm, aber kein (abgerechneter) Download
QUrl viewUrl(const Hit& hit);
// Darf eingefügt werden: frei oder über das eigene Abo
bool insertable(const Hit& hit);
// Treffer je Abruf, die die Quelle höchstens liefert (Kontingente zählen Abrufe, nicht Bilder)
int pageLimit(const QString& source);
// Verständliche Erklärung einer Antwort beim Schlüsseltest (status 0 = keine HTTP-Antwort; networkError
// = Qt-Fehlertext). Leer, wenn alles in Ordnung ist.
QString keyProblem(const QString& source, int status, const QByteArray& body, const QString& networkError, bool timedOut);
// Fassungen zur Auswahl, grösste zuerst; ohne eigene Liste eine aus image
QList<Size> choices(const Hit& hit);
// Anzeigetext einer Fassung, z.B. "Original · 5827 × 3885"
QString sizeLabel(const Size& size);

struct Request {
    QString tag; // Quelle oder Hilfsabruf (z.B. "commons_preview")
    QUrl url;
    QList<QPair<QByteArray, QByteArray>> headers;
};

// Quellen, die mit diesen Schlüsseln bereitstehen
QStringList available(const QHash<QString, QString>& keys);
// Schlüssel aus den Optionen
QHash<QString, QString> keysFromOptions();

// Abrufe einer Quelle für eine Suche (Seite 1 = erste)
QList<Request> requests(const QString& source, const QString& term, int perPage, int page,
                        const QHash<QString, QString>& keys);
// Antworten einer Quelle auswerten (tag -> JSON)
QList<Hit> parse(const QString& source, const QHash<QString, QByteArray>& replies, const QHash<QString, QString>& keys);

// Name des eingefügten Objekts: Titel, Urheber, Quelle und Lizenz (der Nachweis bleibt am Objekt)
QString attribution(const Hit& hit);
// Heruntergeladenes einfügen: SVG als bearbeitbare Kurven, sonst als Bitmap; Name = Nachweis
NodeId insert(Editor& editor, const Hit& hit, const QByteArray& data);

} // namespace imagesearch

// Führt Suchen und Downloads aus (alle Quellen nebeneinander, 12 s Zeitgrenze je Abruf)
class ImageSearch : public QObject {
    Q_OBJECT

public:
    explicit ImageSearch(QObject* parent = nullptr);
    ~ImageSearch() override;

    // Wie lange eine Suche höchstens auf langsame Quellen wartet; danach werden sie übersprungen
    static constexpr int kSearchDeadlineMs = 10000;
    void setSearchDeadline(int ms) { m_deadlineMs = ms; }
    // Alle laufenden Abrufe abbrechen, ohne dass noch Rückmeldungen kommen (beim Schliessen)
    void cancel();
    // Abrufe, die gerade laufen (für Tests)
    int running() const { return int(m_replies.size()); }

    void setKeys(const QHash<QString, QString>& keys) { m_keys = keys; }
    const QHash<QString, QString>& keys() const { return m_keys; }

    // Ergebnis kommt mit finished(); eine neue Suche bricht die laufende ab
    void search(const QString& term, const QStringList& sources, int page = 1, int perPage = 100);
    // Vorschaubild holen
    void fetch(const QUrl& url, std::function<void(const QByteArray&)> done);
    // Das Bild zum Einfügen holen (Magnific: Download-Endpunkt, Icons8 mit Schlüssel: SVG)
    // Schlüssel einer Quelle mit einem kleinen echten Abruf prüfen (ohne Download). ok und eine
    // Meldung für den Benutzer: was funktioniert, oder was schiefging und was zu tun ist.
    void testKey(const QString& source, const QString& key, std::function<void(bool ok, const QString& message)> done);
    // size = Index in choices(hit), 0 = grösste
    void download(const imagesearch::Hit& hit, std::function<void(const QByteArray& data, const QString& error)> done,
                  int size = 0);

signals:
    // Treffer einer Quelle, sobald sie geantwortet hat
    void arrived(const QList<imagesearch::Hit>& hits);
    // Alle Quellen fertig: alle Treffer in Quellenreihenfolge und was schiefging
    void finished(const QList<imagesearch::Hit>& hits, const QStringList& complaints);

private:
    QNetworkReply* get(const imagesearch::Request& request);
    QString complaint(const QString& source, QNetworkReply* reply) const;

    QNetworkAccessManager* m_network = nullptr;
    QSet<QNetworkReply*> m_replies;       // alle laufenden Abrufe
    QList<QPointer<QNetworkReply>> m_searchReplies; // die der aktuellen Suche
    int m_deadlineMs = kSearchDeadlineMs;
    QHash<QString, QString> m_keys;
    int m_generation = 0;
};

} // namespace vek

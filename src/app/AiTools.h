#pragma once

#include "Ai.h"
#include "ImageSearch.h"

#include <QHash>
#include <QObject>

#include <functional>

class QAction;
class QMainWindow;

namespace vek {

class Canvas;
class Editor;
class ScriptHost;
class WebSearch;

// Die Werkzeuge der KI: dieselben für alle Zugänge (direkt aufgerufen oder über die
// MCP-Schnittstelle für Claude Code). Lesen geht immer; was das Dokument ändert, nur mit
// "darf ändern". Fehler gehen als Ergebnis an die KI zurück, nie als Ausnahme.
//
//  Lesen:    dokument_ansehen, objekt_ansehen, seite_ansehen (Bild), befehle,
//            web_suchen, web_lesen, bild_suchen, bild_ansehen
//  Ändern:   skript (JavaScript mit der tg-API), svg_einfuegen, befehl (Menübefehl),
//            auswaehlen, loeschen, bild_einfuegen, bild_von_url, nachzeichnen,
//            seite_wechseln, exportieren
class AiTools : public QObject {
    Q_OBJECT

public:
    AiTools(Editor& editor, Canvas& canvas, QMainWindow* window, QObject* parent = nullptr);
    ~AiTools() override;

    struct Result {
        QString text;
        QList<QByteArray> images; // PNG
        bool error = false;
    };

    QList<ai::Tool> tools(bool mayWrite) const;
    bool writes(const QString& name) const;
    // Asynchron (Web, Bilder); done kommt immer genau einmal
    void call(const QString& name, const QJsonObject& input, bool mayWrite, std::function<void(const Result&)> done);
    // Menübefehle, die die KI auslösen darf ("Anordnen > Gruppieren")
    QStringList commands() const;
    // Kurzbeschreibung der Skript-API (steht in der Werkzeugbeschreibung)
    static QString scriptGuide();
    // Für Tests: Treffer der Bildersuche unter einem Kürzel ablegen
    QString rememberHit(const imagesearch::Hit& hit);
    ImageSearch* imageSearch() const { return m_images; }

private:
    QHash<QString, QAction*> commandActions() const;
    Result readDocument();
    Result readObject(int id);
    Result viewPage(const QJsonObject& input);
    Result runScript(const QString& code);
    Result insertSvg(const QJsonObject& input);
    Result runCommand(const QJsonObject& input);
    Result trace(const QJsonObject& input);
    Result exportFile(const QJsonObject& input);
    void searchImages(const QJsonObject& input, std::function<void(const Result&)> done);
    void viewImage(const QJsonObject& input, std::function<void(const Result&)> done);
    void insertImage(const QJsonObject& input, std::function<void(const Result&)> done);
    void insertFromUrl(const QJsonObject& input, std::function<void(const Result&)> done);
    // Eingefügtes Objekt an x/y (links oben, mm) und auf breite/hoehe bringen
    void place(NodeId id, const QJsonObject& input);
    QString selectionText() const;

    Editor& m_editor;
    Canvas& m_canvas;
    QMainWindow* m_window = nullptr;
    ScriptHost* m_script = nullptr;
    ImageSearch* m_images = nullptr;
    WebSearch* m_web = nullptr;
    QHash<QString, imagesearch::Hit> m_hits;
    int m_hitCount = 0;
};

} // namespace vek

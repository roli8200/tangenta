#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <functional>

class QProcess;

namespace vek {

// Zugang "Claude (Abo über Claude Code)": TANGENTA startet das installierte Claude Code im
// Hintergrund (claude -p, Ausgabe als stream-json). Claude Code ist mit dem eigenen Claude-Konto
// angemeldet (claude auth login) und rechnet über das Abo ab; die Werkzeuge von TANGENTA bekommt
// es über die MCP-Schnittstelle (McpServer). Eigene Werkzeuge von Claude Code (Dateien, Shell)
// sind abgeschaltet, Einstellungen und Hooks des Benutzers bleiben aussen vor.
class ClaudeCode : public QObject {
    Q_OBJECT

public:
    explicit ClaudeCode(QObject* parent = nullptr);
    ~ClaudeCode() override;

    // Pfad zu claude(.exe): Optionen > Global > KI, sonst gesucht (PATH, npm, ~/.local/bin)
    static QString executable();
    // Anmeldestatus: claude auth status --json
    static void status(QObject* context, std::function<void(bool found, bool loggedIn, const QString& info)> done);
    // Anmeldung in einem eigenen Fenster (claude auth login); false, wenn Claude Code fehlt
    static bool login();

    struct Request {
        QString system;     // Systemanweisung (ersetzt die von Claude Code)
        QString model;      // "opus", "sonnet", "haiku" oder Modell-ID
        QString mcpUrl;     // http://127.0.0.1:<port>/mcp
        QString mcpToken;   // Bearer-Token der MCP-Schnittstelle
        QString session;    // Gespräch fortsetzen (Sitzungs-ID von Claude Code)
        QString directory;  // Arbeitsverzeichnis des Prozesses
        bool keepSession = true;
    };
    // Eine Frage; Ereignisse wie bei ai::Provider, am Ende finished() oder failed()
    void ask(const QString& question, const Request& options);
    void abort();
    bool busy() const;
    // Argumente für claude (für Tests)
    static QStringList arguments(const QString& question, const Request& options);
    // Eine Zeile der stream-json-Ausgabe verarbeiten (für Tests)
    void line(const QByteArray& json);

    // Echter kleiner Abruf über das Abo ("Antworte mit dem Wort: bereit.")
    void test(const QString& model, std::function<void(bool ok, const QString& message)> done);

signals:
    void text(const QString& piece);
    void toolComing(const QString& name);
    void toolDone(const QString& name, bool ok, const QString& summary);
    void usage(const QString& info); // z.B. Auslastung des Abos
    void finished(const QString& answer, const QString& session, int tokensIn, int tokensOut);
    void failed(const QString& message);

private:
    QPointer<QProcess> m_process;
    QByteArray m_buffer;
    QByteArray m_errors;
    QString m_answer;
    QString m_session;
    QHash<QString, QString> m_toolNames; // tool_use_id -> Name
    bool m_done = false;
};

} // namespace vek

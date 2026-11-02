#pragma once

#include "Ai.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>

#include <cstddef>

class QMainWindow;

namespace vek {

class AiTools;
class Canvas;
class ClaudeCode;
class Editor;
class McpServer;

// Ein Gespräch mit der KI (wie AiGespraech in ThurWiki).
//
// Claude über das Abo: Claude Code führt die Schleife selbst und ruft die Werkzeuge über die
// MCP-Schnittstelle. Alle anderen Zugänge: TANGENTA führt die Schleife (Runde -> Werkzeuge ->
// Runde ...) mit Grenzen für Runden, Tokens und Zeit. Alles, was eine Antwort am Dokument
// ändert, ist danach ein einziger Rückgängig-Schritt ("KI: <Frage>").
class AiChat : public QObject {
    Q_OBJECT

public:
    static constexpr int kMaxRounds = 40;
    static constexpr int kMaxTokens = 500000;
    static constexpr int kMaxSeconds = 1200;
    static constexpr int kHistory = 30; // so viele Nachrichten früherer Fragen gehen mit

    AiChat(Editor& editor, Canvas& canvas, QMainWindow* window, QObject* parent = nullptr);
    ~AiChat() override;

    void setAccount(const ai::Account& account);
    const ai::Account& account() const { return m_account; }

    void ask(const QString& question, bool mayWrite);
    void stop();
    bool busy() const { return m_busy; }
    // Neues Gespräch: Verlauf leeren
    void reset();

    // Systemanweisung mit dem aktuellen Stand des Dokuments
    QString systemPrompt(bool mayWrite) const;
    AiTools& tools() { return *m_tools; }
    McpServer* mcp() const { return m_mcp; }

    // Gespräch sichern/laden (nur Text, wie in ThurWiki)
    struct Entry {
        enum class Kind { Question, Answer, Step, Note };
        Kind kind = Kind::Answer;
        QString text;
        bool ok = true;
    };
    const QList<Entry>& transcript() const { return m_transcript; }
    // Gestreamter Text, der noch nicht im Verlauf steht
    QString pending() const { return m_answer.mid(m_flushed); }
    QJsonObject save() const;
    void load(const QJsonObject& json);

signals:
    void started();
    void text(const QString& piece);
    void status(const QString& line);
    void step(const QString& tool, bool ok, const QString& summary);
    void finished(const QString& summary); // "Modell · 3 Schritte · 12 s · 4.2k Token"
    void failed(const QString& message);
    void transcriptChanged();

private:
    void nextRound();
    void runTools(QList<ai::ToolCall> calls);
    void finish(const QString& note = QString());
    void fail(const QString& message);
    void addEntry(Entry::Kind kind, const QString& text, bool ok = true);
    QString toolLabel(const QString& name) const;

    Editor& m_editor;
    AiTools* m_tools = nullptr;
    McpServer* m_mcp = nullptr;
    ClaudeCode* m_claude = nullptr;
    QPointer<ai::Provider> m_provider;
    ai::Account m_account;

    QList<ai::Message> m_history; // Text früherer Fragen + Werkzeugverkehr der laufenden
    QString m_session;            // Claude Code: Sitzung fortsetzen
    QList<Entry> m_transcript;
    bool m_busy = false;
    bool m_mayWrite = false;
    bool m_stopped = false;
    QString m_question;
    QString m_answer;
    qsizetype m_flushed = 0; // so viel von m_answer steht schon im Verlauf
    std::size_t m_undoMark = 0;
    int m_rounds = 0;
    int m_steps = 0;
    int m_tokensIn = 0;
    int m_tokensOut = 0;
    QElapsedTimer m_clock;
};

} // namespace vek

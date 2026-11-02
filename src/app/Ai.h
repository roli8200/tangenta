#pragma once

#include <QByteArray>
#include <QHash>
#include <QMap>
#include <QPair>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace vek {

// KI-Schnittstelle wie in ThurWiki: mehrere Zugänge (Connectoren) zu verschiedenen Anbietern,
// eine gemeinsame Gesprächsform und dieselben Werkzeuge für alle.
//
// Zugänge stehen lokal in den Optionen (Global > KI), je Benutzer, nie in einer Datei des
// Dokuments. "Claude (Abo über Claude Code)" ruft das installierte Claude Code auf, das mit dem
// eigenen Claude-Konto angemeldet ist (claude auth login); die Werkzeuge bekommt es über die
// MCP-Schnittstelle von TANGENTA. Alle anderen Zugänge sprechen die Schnittstelle des Anbieters
// direkt an, und TANGENTA führt die Werkzeuge selbst aus.
namespace ai {

// --- Gesprächsform -----------------------------------------------------------------------------

struct Message {
    enum class Role { User, Assistant, ToolUse, ToolResult };
    Role role = Role::User;
    QString text;
    QString tool;            // ToolUse/ToolResult: Name des Werkzeugs
    QJsonObject input;       // ToolUse: Eingabe
    QString callId;          // ToolUse/ToolResult: Zuordnung
    bool error = false;      // ToolResult: Werkzeug ist gescheitert
    QList<QByteArray> images; // ToolResult: PNG-Bilder (z.B. Ansicht der Seite)
};

struct Tool {
    QString name;
    QString description;
    QJsonObject schema; // JSON-Schema der Eingabe
    bool writes = false; // ändert das Dokument
};

struct ToolCall {
    QString id;
    QString name;
    QJsonObject input;
};

struct Reply {
    QString text;
    QList<ToolCall> calls;
    int tokensIn = 0;
    int tokensOut = 0;
    QString stop; // "end", "tools", "max_tokens"
};

// --- Zugänge -----------------------------------------------------------------------------------

struct Kind {
    QString key;           // "claude_code", "claude", "openai", ...
    QString name;
    QString baseUrl;       // Vorgabe
    bool needsKey = true;
    bool needsUrl = false; // eigener Server (Ollama, LiteLLM)
    QString defaultModel;
    QStringList fallbackModels;
    QString hint;          // woher der Schlüssel kommt
};
const QList<Kind>& kinds();
const Kind* kind(const QString& key);

struct Account {
    QString id;
    QString kind;
    QString label;
    QString baseUrl;
    QString key;
    QString model;
    QString displayName() const;
};

// Optionen: "global/ai/accounts" (JSON-Liste), "global/ai/account" (gewählter Zugang)
QList<Account> accounts();
void saveAccounts(const QList<Account>& list);
QJsonArray toJson(const QList<Account>& list);
QList<Account> accountsFromJson(const QJsonArray& array);
// Gewählter Zugang, sonst der erste; leer ohne Zugänge
Account currentAccount();
QString newAccountId();

// Verständliche Meldung zu einer gescheiterten Anfrage (status 0 = keine HTTP-Antwort)
QString explainFailure(const QString& service, int status, const QByteArray& body, const QString& networkError,
                       bool loginAccount = false);

// --- Anbieter ----------------------------------------------------------------------------------

// Eine Runde: Systemanweisung, Verlauf und Werkzeuge hin, Antwort mit Text und Werkzeugaufrufen
// zurück. Läuft gestreamt; text() kommt stückweise, danach genau einmal done() oder failed().
// Überlastung und Drosselung (408, 425, 429, 5xx, abgerissene Verbindung vor der ersten Antwort)
// werden bis zu viermal wiederholt (1, 2, 4 s, Retry-After bis 20 s).
class Provider : public QObject {
    Q_OBJECT

public:
    // nullptr für "claude_code" (läuft über ClaudeCode)
    static Provider* create(const Account& account, QObject* parent = nullptr);
    ~Provider() override;

    const Account& account() const { return m_account; }
    QString model() const;

    void round(const QString& system, const QList<Message>& history, const QList<Tool>& tools);
    void abort();
    bool busy() const { return m_reply != nullptr || m_waiting; }

    // Modelle des Anbieters (live), sonst die bekannten
    virtual void models(std::function<void(const QStringList& models, const QString& error)> done) = 0;
    // Echter kleiner Abruf: "Antworte mit dem Wort: bereit."
    void test(std::function<void(bool ok, const QString& message)> done);

    // Für Tests: Anfrage bauen und Stream-Ereignisse einspeisen
    virtual QJsonObject requestBody(const QString& system, const QList<Message>& history, const QList<Tool>& tools) const = 0;
    void feed(const QByteArray& sse);

signals:
    void text(const QString& piece);
    void toolComing(const QString& name);
    void retrying(int attempt, const QString& reason);
    void done(const vek::ai::Reply& reply);
    void failed(const QString& message);

protected:
    Provider(const Account& account, QObject* parent);
    virtual QUrl endpoint() const = 0;
    virtual QList<QPair<QByteArray, QByteArray>> headers() const = 0;
    // Ein SSE-Ereignis; true, wenn der Stream damit fertig ist
    virtual bool onEvent(const QByteArray& name, const QByteArray& data) = 0;
    virtual QString serviceName() const;
    QNetworkAccessManager* network();
    void get(const QUrl& url, std::function<void(int status, const QByteArray& body, const QString& error)> done);
    void finish();
    void fail(const QString& message);

    Account m_account;
    Reply m_result;
    bool m_finished = false;

private:
    void send();
    void retryOrFail(int status, const QByteArray& body, const QString& networkError, int retryAfter);

    QNetworkAccessManager* m_network = nullptr;
    QPointer<QNetworkReply> m_reply;
    QTimer* m_retry = nullptr;
    QByteArray m_body;
    QByteArray m_buffer;
    QByteArray m_errorBody;
    bool m_gotData = false;
    bool m_waiting = false;
    int m_attempt = 0;
};

// Claude über die Anthropic-Schnittstelle mit API-Schlüssel
class AnthropicProvider final : public Provider {
public:
    AnthropicProvider(const Account& account, QObject* parent);
    void models(std::function<void(const QStringList&, const QString&)> done) override;
    QJsonObject requestBody(const QString& system, const QList<Message>& history, const QList<Tool>& tools) const override;

protected:
    QUrl endpoint() const override;
    QList<QPair<QByteArray, QByteArray>> headers() const override;
    bool onEvent(const QByteArray& name, const QByteArray& data) override;

private:
    struct Block {
        QString type, id, name;
        QByteArray json;
    };
    QHash<int, Block> m_blocks;
};

// OpenAI und alles, was dieselbe Schnittstelle spricht (Deepseek, Gemini, Ollama, LiteLLM)
class OpenAiProvider final : public Provider {
public:
    OpenAiProvider(const Account& account, QObject* parent);
    void models(std::function<void(const QStringList&, const QString&)> done) override;
    QJsonObject requestBody(const QString& system, const QList<Message>& history, const QList<Tool>& tools) const override;

protected:
    QUrl endpoint() const override;
    QList<QPair<QByteArray, QByteArray>> headers() const override;
    bool onEvent(const QByteArray& name, const QByteArray& data) override;

private:
    QString base() const;
    struct Call {
        QString id, name;
        QByteArray arguments;
    };
    QMap<int, Call> m_calls;
    QString m_finish;
};

} // namespace ai
} // namespace vek

Q_DECLARE_METATYPE(vek::ai::Reply)

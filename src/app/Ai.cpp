#include "Ai.h"

#include "Options.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTimer>
#include <QUuid>

namespace vek::ai {

namespace {

constexpr int kMaxTokens = 32000;       // je Antwort (Anthropic verlangt eine Grenze)
constexpr int kSilenceMs = 90000;       // so lange darf der Stream schweigen
constexpr int kAttempts = 4;            // Versuche bei Überlastung/Drosselung
constexpr int kMaxRetryAfterS = 20;
const QByteArray kUserAgent = "TANGENTA/1.0 (Vektorgrafik; Qt) KI";

QString accountsKey() { return QStringLiteral("global/ai/accounts"); }
QString currentKey() { return QStringLiteral("global/ai/account"); }

// Fehlertext eines Dienstes aus JSON (verschiedene Anbieter) oder Klartext
QString serviceMessage(const QByteArray& body)
{
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isArray() && !doc.array().isEmpty()) // Gemini: [{"error":{...}}]
        return serviceMessage(QJsonDocument(doc.array().first().toObject()).toJson());
    if (doc.isObject()) {
        const QJsonObject o = doc.object();
        const QJsonValue error = o.value(QStringLiteral("error"));
        if (error.isObject() && error.toObject().value(QStringLiteral("message")).isString())
            return error.toObject().value(QStringLiteral("message")).toString();
        for (const char* field : {"message", "error", "detail", "error_description"})
            if (o.value(QLatin1String(field)).isString())
                return o.value(QLatin1String(field)).toString();
        return {};
    }
    QString text = QString::fromUtf8(body.left(300)).remove(QRegularExpression(QStringLiteral("<[^>]*>"))).simplified();
    return text.length() > 200 ? text.left(200) + QStringLiteral(" …") : text;
}

QString imageBase64(const QByteArray& png) { return QString::fromLatin1(png.toBase64()); }

QJsonObject objectSchema(QJsonObject schema)
{
    if (!schema.contains(QStringLiteral("type")))
        schema.insert(QStringLiteral("type"), QStringLiteral("object"));
    if (!schema.value(QStringLiteral("properties")).isObject())
        schema.insert(QStringLiteral("properties"), QJsonObject());
    if (schema.value(QStringLiteral("required")).toArray().isEmpty())
        schema.remove(QStringLiteral("required"));
    return schema;
}

} // namespace

// --- Zugänge -----------------------------------------------------------------------------------

const QList<Kind>& kinds()
{
    static const QList<Kind> list{
        {QStringLiteral("claude_code"), QObject::tr("Claude (Abo über Claude Code)"), QString(), false, false,
         QStringLiteral("opus"), {QStringLiteral("opus"), QStringLiteral("sonnet"), QStringLiteral("haiku")},
         QObject::tr("Nutzt das installierte Claude Code mit dem eigenen Claude-Abo (Pro oder Max), ohne "
                     "API-Kosten. Anmelden mit dem Knopf „Anmelden“ (entspricht „claude auth login“). Claude "
                     "Code gibt es unter claude.com/claude-code.")},
        {QStringLiteral("claude"), QObject::tr("Claude (API-Schlüssel)"), QStringLiteral("https://api.anthropic.com"), true, false,
         QStringLiteral("claude-sonnet-5-5"),
         {QStringLiteral("claude-opus-5-5"), QStringLiteral("claude-sonnet-5-5"), QStringLiteral("claude-haiku-5-5")},
         QObject::tr("API-Schlüssel aus console.anthropic.com („API Keys“). Wird nach Verbrauch abgerechnet.")},
        {QStringLiteral("openai"), QStringLiteral("OpenAI"), QStringLiteral("https://api.openai.com/v1"), true, false,
         QStringLiteral("gpt-4o"), {QStringLiteral("gpt-4o"), QStringLiteral("gpt-4o-mini"), QStringLiteral("gpt-4-turbo")},
         QObject::tr("API-Schlüssel aus platform.openai.com/api-keys. Für Azure OpenAI hier die eigene Adresse eintragen.")},
        {QStringLiteral("gemini"), QStringLiteral("Google Gemini"), QStringLiteral("https://generativelanguage.googleapis.com/v1beta/openai"),
         true, false, QStringLiteral("gemini-2.5-flash"),
         {QStringLiteral("gemini-2.5-flash"), QStringLiteral("gemini-2.5-pro"), QStringLiteral("gemini-2.0-flash")},
         QObject::tr("API-Schlüssel aus aistudio.google.com/apikey.")},
        {QStringLiteral("deepseek"), QStringLiteral("DeepSeek"), QStringLiteral("https://api.deepseek.com/v1"), true, false,
         QStringLiteral("deepseek-chat"), {QStringLiteral("deepseek-chat"), QStringLiteral("deepseek-reasoner")},
         QObject::tr("API-Schlüssel aus platform.deepseek.com. DeepSeek sieht keine Bilder (keine Ansicht der Seite).")},
        {QStringLiteral("ollama"), QStringLiteral("Ollama"), QStringLiteral("http://localhost:11434/v1"), false, true, QString(), {},
         QObject::tr("Eigener Ollama-Server (Adresse mit /v1). Das Modell muss Werkzeuge beherrschen, z.B. qwen3 oder llama3.1.")},
        {QStringLiteral("litellm"), QStringLiteral("LiteLLM"), QString(), false, true, QString(), {},
         QObject::tr("Eigener LiteLLM-Proxy (Adresse mit /v1), Schlüssel falls verlangt.")},
    };
    return list;
}

const Kind* kind(const QString& key)
{
    for (const Kind& k : kinds())
        if (k.key == key)
            return &k;
    return nullptr;
}

QString Account::displayName() const
{
    if (!label.trimmed().isEmpty())
        return label.trimmed();
    const Kind* k = ai::kind(kind);
    return k ? k->name : kind;
}

QJsonArray toJson(const QList<Account>& list)
{
    QJsonArray out;
    for (const Account& a : list)
        out.append(QJsonObject{{QStringLiteral("id"), a.id},
                               {QStringLiteral("kind"), a.kind},
                               {QStringLiteral("label"), a.label},
                               {QStringLiteral("baseUrl"), a.baseUrl},
                               {QStringLiteral("key"), a.key},
                               {QStringLiteral("model"), a.model}});
    return out;
}

QList<Account> accountsFromJson(const QJsonArray& array)
{
    QList<Account> out;
    for (const QJsonValue& v : array) {
        const QJsonObject o = v.toObject();
        Account a{o.value(QStringLiteral("id")).toString(),   o.value(QStringLiteral("kind")).toString(),
                  o.value(QStringLiteral("label")).toString(), o.value(QStringLiteral("baseUrl")).toString(),
                  o.value(QStringLiteral("key")).toString(),   o.value(QStringLiteral("model")).toString()};
        if (!a.id.isEmpty() && kind(a.kind))
            out << a;
    }
    return out;
}

QList<Account> accounts()
{
    return accountsFromJson(QJsonDocument::fromJson(Options::instance().text(accountsKey()).toUtf8()).array());
}

void saveAccounts(const QList<Account>& list)
{
    Options::instance().set(accountsKey(), QString::fromUtf8(QJsonDocument(toJson(list)).toJson(QJsonDocument::Compact)));
}

Account currentAccount()
{
    const QList<Account> list = accounts();
    const QString wanted = Options::instance().text(currentKey());
    for (const Account& a : list)
        if (a.id == wanted)
            return a;
    return list.isEmpty() ? Account() : list.first();
}

QString newAccountId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

QString explainFailure(const QString& service, int status, const QByteArray& body, const QString& networkError, bool loginAccount)
{
    const QString said = serviceMessage(body);
    const QString quote = said.isEmpty() ? QString() : QStringLiteral("\n") + QObject::tr("Antwort von %1: „%2“").arg(service, said);
    if (status == 0)
        return QObject::tr("Keine Verbindung zu %1 (%2). Internetverbindung, Proxy oder Firewall prüfen; bei einem "
                           "eigenen Server die Adresse.").arg(service, networkError);
    if (status == 401)
        return (loginAccount ? QObject::tr("Die Anmeldung bei %1 ist abgelaufen oder ungültig: bitte neu anmelden.").arg(service)
                             : QObject::tr("%1 kennt diesen Schlüssel nicht (HTTP 401). Meist ist er unvollständig kopiert "
                                           "oder gehört zu einem anderen Dienst.").arg(service)) + quote;
    if (status == 403)
        return QObject::tr("%1 verweigert den Zugriff (HTTP 403): Schlüssel ohne Berechtigung für dieses Modell, Konto "
                           "gesperrt oder kein Guthaben.").arg(service) + quote;
    if (status == 404)
        return QObject::tr("%1 kennt das Modell oder die Adresse nicht (HTTP 404). Modell in den Optionen neu wählen; "
                           "bei eigenem Server die Adresse prüfen (meist mit /v1).").arg(service) + quote;
    if (status == 400 || status == 422)
        return QObject::tr("%1 lehnt die Anfrage ab (HTTP %2).").arg(service).arg(status) + quote;
    if (status == 429)
        return QObject::tr("%1 drosselt gerade oder das Kontingent ist aufgebraucht (HTTP 429). Etwas warten und "
                           "nochmals versuchen.").arg(service) + quote;
    if (status == 529 || status == 503)
        return QObject::tr("%1 ist im Moment überlastet (HTTP %2). Bitte später nochmals versuchen.").arg(service).arg(status) + quote;
    if (status >= 500)
        return QObject::tr("%1 hat eine Störung (HTTP %2).").arg(service).arg(status) + quote;
    return QObject::tr("%1 meldet HTTP %2.").arg(service).arg(status) + quote;
}

// --- Provider ----------------------------------------------------------------------------------

Provider* Provider::create(const Account& account, QObject* parent)
{
    if (account.kind == QLatin1String("claude"))
        return new AnthropicProvider(account, parent);
    if (account.kind == QLatin1String("claude_code") || !kind(account.kind))
        return nullptr;
    return new OpenAiProvider(account, parent);
}

Provider::Provider(const Account& account, QObject* parent)
    : QObject(parent)
    , m_account(account)
    , m_retry(new QTimer(this))
{
    m_retry->setSingleShot(true);
    connect(m_retry, &QTimer::timeout, this, [this] {
        m_waiting = false;
        send();
    });
}

Provider::~Provider()
{
    abort();
}

QString Provider::model() const
{
    if (!m_account.model.trimmed().isEmpty())
        return m_account.model.trimmed();
    const Kind* k = kind(m_account.kind);
    return k ? k->defaultModel : QString();
}

QString Provider::serviceName() const
{
    const Kind* k = kind(m_account.kind);
    return k ? k->name : m_account.kind;
}

QNetworkAccessManager* Provider::network()
{
    if (!m_network)
        m_network = new QNetworkAccessManager(this);
    return m_network;
}

void Provider::round(const QString& system, const QList<Message>& history, const QList<Tool>& tools)
{
    abort();
    m_finished = false;
    m_result = Reply();
    m_attempt = 0;
    m_body = QJsonDocument(requestBody(system, history, tools)).toJson(QJsonDocument::Compact);
    send();
}

void Provider::send()
{
    m_buffer.clear();
    m_errorBody.clear();
    m_gotData = false;
    QNetworkRequest request(endpoint());
    request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
    request.setRawHeader("Accept", "text/event-stream");
    for (const auto& [name, value] : headers())
        request.setRawHeader(name, value);
    request.setTransferTimeout(kSilenceMs);
    QNetworkReply* reply = network()->post(request, m_body);
    m_reply = reply;
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status != 200) {
            m_errorBody += reply->readAll();
            return;
        }
        feed(reply->readAll());
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (m_reply == reply)
            m_reply = nullptr;
        if (m_finished)
            return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 200 && reply->error() == QNetworkReply::NoError) {
            feed(reply->readAll() + QByteArrayLiteral("\n\n"));
            if (m_finished)
                return;
            // Ende ohne Schlusszeichen: brauchbar, wenn schon etwas kam
            if (!m_result.text.isEmpty() || !m_result.calls.isEmpty()) {
                if (m_result.stop.isEmpty())
                    m_result.stop = m_result.calls.isEmpty() ? QStringLiteral("end") : QStringLiteral("tools");
                finish();
            } else {
                retryOrFail(0, {}, tr("Die Antwort brach vorzeitig ab."), 0);
            }
            return;
        }
        m_errorBody += reply->readAll();
        const int retryAfter = reply->rawHeader("Retry-After").toInt();
        retryOrFail(status, m_errorBody, reply->errorString(), retryAfter);
    });
}

void Provider::retryOrFail(int status, const QByteArray& body, const QString& networkError, int retryAfter)
{
    const bool retryable = status == 0 || status == 408 || status == 425 || status == 429 || status >= 500;
    if (retryable && !m_gotData && m_attempt + 1 < kAttempts && retryAfter <= kMaxRetryAfterS) {
        ++m_attempt;
        const int delay = retryAfter > 0 ? retryAfter * 1000
                                         : (1000 << (m_attempt - 1)) + int(QRandomGenerator::global()->bounded(400));
        const QString reason = status == 0 ? networkError
                               : status == 429 ? tr("gedrosselt")
                               : status == 529 || status == 503 ? tr("überlastet")
                                                                : QStringLiteral("HTTP %1").arg(status);
        emit retrying(m_attempt + 1, reason);
        m_waiting = true;
        m_retry->start(delay);
        return;
    }
    fail(explainFailure(serviceName(), status, body, networkError));
}

void Provider::abort()
{
    m_retry->stop();
    m_waiting = false;
    if (QNetworkReply* reply = m_reply) {
        m_reply = nullptr;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    m_finished = true;
}

void Provider::feed(const QByteArray& sse)
{
    m_buffer += sse;
    m_buffer.replace("\r\n", "\n");
    for (;;) {
        const qsizetype end = m_buffer.indexOf("\n\n");
        if (end < 0 || m_finished)
            break;
        const QByteArray chunk = m_buffer.left(end);
        m_buffer.remove(0, end + 2);
        QByteArray name, data;
        for (const QByteArray& line : chunk.split('\n')) {
            if (line.startsWith("event:"))
                name = line.mid(6).trimmed();
            else if (line.startsWith("data:"))
                data += (data.isEmpty() ? QByteArray() : QByteArray("\n")) + line.mid(5).trimmed();
        }
        if (data.isEmpty() && name.isEmpty())
            continue;
        m_gotData = true;
        if (onEvent(name, data))
            finish();
    }
}

void Provider::finish()
{
    if (m_finished) // schon gemeldet oder abgebrochen
        return;
    m_finished = true;
    if (QNetworkReply* reply = m_reply) {
        m_reply = nullptr;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    emit done(m_result);
}

void Provider::fail(const QString& message)
{
    if (m_finished)
        return;
    m_finished = true;
    if (QNetworkReply* reply = m_reply) {
        m_reply = nullptr;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    emit failed(message);
}

void Provider::get(const QUrl& url, std::function<void(int, const QByteArray&, const QString&)> done)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
    for (const auto& [name, value] : headers())
        request.setRawHeader(name, value);
    request.setTransferTimeout(20000);
    QNetworkReply* reply = network()->get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        done(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), reply->readAll(),
             reply->error() == QNetworkReply::NoError ? QString() : reply->errorString());
    });
}

void Provider::test(std::function<void(bool, const QString&)> done)
{
    connect(this, &Provider::done, this, [this, done](const Reply& reply) {
        done(true, tr("Zugang funktioniert. %1 antwortet mit Modell %2: „%3“")
                       .arg(serviceName(), model(), reply.text.trimmed().left(80)));
    }, Qt::SingleShotConnection);
    connect(this, &Provider::failed, this, [done](const QString& message) { done(false, message); }, Qt::SingleShotConnection);
    Message question;
    question.text = QStringLiteral("Bereit?");
    round(QStringLiteral("Antworte mit dem Wort: bereit."), {question}, {});
}

// --- Anthropic ---------------------------------------------------------------------------------

AnthropicProvider::AnthropicProvider(const Account& account, QObject* parent)
    : Provider(account, parent)
{
}

QUrl AnthropicProvider::endpoint() const
{
    const QString base = m_account.baseUrl.trimmed().isEmpty() ? kind(m_account.kind)->baseUrl : m_account.baseUrl.trimmed();
    return QUrl(base.endsWith(QLatin1Char('/')) ? base + QStringLiteral("v1/messages") : base + QStringLiteral("/v1/messages"));
}

QList<QPair<QByteArray, QByteArray>> AnthropicProvider::headers() const
{
    return {{"x-api-key", m_account.key.trimmed().toUtf8()}, {"anthropic-version", "2023-06-01"}};
}

QJsonObject AnthropicProvider::requestBody(const QString& system, const QList<Message>& history, const QList<Tool>& tools) const
{
    // Aufeinanderfolgende Blöcke derselben Rolle werden eine Nachricht (Anthropic verlangt den Wechsel)
    QJsonArray messages;
    auto add = [&messages](const QString& role, const QJsonObject& block) {
        if (!messages.isEmpty() && messages.last().toObject().value(QStringLiteral("role")).toString() == role) {
            QJsonObject last = messages.last().toObject();
            QJsonArray content = last.value(QStringLiteral("content")).toArray();
            content.append(block);
            last.insert(QStringLiteral("content"), content);
            messages.replace(messages.size() - 1, last);
        } else {
            messages.append(QJsonObject{{QStringLiteral("role"), role}, {QStringLiteral("content"), QJsonArray{block}}});
        }
    };
    for (const Message& m : history) {
        switch (m.role) {
        case Message::Role::User:
            add(QStringLiteral("user"), {{QStringLiteral("type"), QStringLiteral("text")}, {QStringLiteral("text"), m.text}});
            break;
        case Message::Role::Assistant:
            if (!m.text.trimmed().isEmpty())
                add(QStringLiteral("assistant"), {{QStringLiteral("type"), QStringLiteral("text")}, {QStringLiteral("text"), m.text}});
            break;
        case Message::Role::ToolUse:
            add(QStringLiteral("assistant"), {{QStringLiteral("type"), QStringLiteral("tool_use")},
                                              {QStringLiteral("id"), m.callId},
                                              {QStringLiteral("name"), m.tool},
                                              {QStringLiteral("input"), m.input}});
            break;
        case Message::Role::ToolResult: {
            QJsonArray content{QJsonObject{{QStringLiteral("type"), QStringLiteral("text")},
                                           {QStringLiteral("text"), m.text.isEmpty() ? QStringLiteral("(leer)") : m.text}}};
            for (const QByteArray& png : m.images)
                content.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("image")},
                                           {QStringLiteral("source"), QJsonObject{{QStringLiteral("type"), QStringLiteral("base64")},
                                                                                  {QStringLiteral("media_type"), QStringLiteral("image/png")},
                                                                                  {QStringLiteral("data"), imageBase64(png)}}}});
            QJsonObject block{{QStringLiteral("type"), QStringLiteral("tool_result")},
                              {QStringLiteral("tool_use_id"), m.callId},
                              {QStringLiteral("content"), content}};
            if (m.error)
                block.insert(QStringLiteral("is_error"), true);
            add(QStringLiteral("user"), block);
            break;
        }
        }
    }
    QJsonObject body{{QStringLiteral("model"), model()},
                     {QStringLiteral("max_tokens"), kMaxTokens},
                     {QStringLiteral("stream"), true},
                     {QStringLiteral("system"), system},
                     {QStringLiteral("messages"), messages}};
    if (!tools.isEmpty()) {
        QJsonArray list;
        for (const Tool& t : tools)
            list.append(QJsonObject{{QStringLiteral("name"), t.name},
                                    {QStringLiteral("description"), t.description},
                                    {QStringLiteral("input_schema"), objectSchema(t.schema)}});
        body.insert(QStringLiteral("tools"), list);
    }
    return body;
}

bool AnthropicProvider::onEvent(const QByteArray&, const QByteArray& data)
{
    const QJsonObject e = QJsonDocument::fromJson(data).object();
    const QString type = e.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("message_start")) {
        m_blocks.clear();
        const QJsonObject usage = e.value(QStringLiteral("message")).toObject().value(QStringLiteral("usage")).toObject();
        m_result.tokensIn = usage.value(QStringLiteral("input_tokens")).toInt() + usage.value(QStringLiteral("cache_creation_input_tokens")).toInt() +
                            usage.value(QStringLiteral("cache_read_input_tokens")).toInt();
    } else if (type == QLatin1String("content_block_start")) {
        const QJsonObject b = e.value(QStringLiteral("content_block")).toObject();
        Block block{b.value(QStringLiteral("type")).toString(), b.value(QStringLiteral("id")).toString(),
                    b.value(QStringLiteral("name")).toString(), {}};
        m_blocks.insert(e.value(QStringLiteral("index")).toInt(), block);
        if (block.type == QLatin1String("tool_use"))
            emit toolComing(block.name);
    } else if (type == QLatin1String("content_block_delta")) {
        const QJsonObject d = e.value(QStringLiteral("delta")).toObject();
        const QString kind = d.value(QStringLiteral("type")).toString();
        if (kind == QLatin1String("text_delta")) {
            const QString piece = d.value(QStringLiteral("text")).toString();
            m_result.text += piece;
            emit text(piece);
        } else if (kind == QLatin1String("input_json_delta")) {
            m_blocks[e.value(QStringLiteral("index")).toInt()].json += d.value(QStringLiteral("partial_json")).toString().toUtf8();
        }
    } else if (type == QLatin1String("content_block_stop")) {
        const Block block = m_blocks.value(e.value(QStringLiteral("index")).toInt());
        if (block.type == QLatin1String("tool_use"))
            m_result.calls << ToolCall{block.id, block.name, QJsonDocument::fromJson(block.json.isEmpty() ? "{}" : block.json).object()};
    } else if (type == QLatin1String("message_delta")) {
        const QString stop = e.value(QStringLiteral("delta")).toObject().value(QStringLiteral("stop_reason")).toString();
        m_result.stop = stop == QLatin1String("tool_use")     ? QStringLiteral("tools")
                        : stop == QLatin1String("max_tokens") ? QStringLiteral("max_tokens")
                                                              : QStringLiteral("end");
        m_result.tokensOut = e.value(QStringLiteral("usage")).toObject().value(QStringLiteral("output_tokens")).toInt();
    } else if (type == QLatin1String("message_stop")) {
        return true;
    } else if (type == QLatin1String("error")) {
        const QJsonObject err = e.value(QStringLiteral("error")).toObject();
        const QString t = err.value(QStringLiteral("type")).toString();
        const int status = t == QLatin1String("overloaded_error") ? 529 : t == QLatin1String("rate_limit_error") ? 429 : 500;
        fail(explainFailure(serviceName(), status, data, QString()));
    }
    return false;
}

void AnthropicProvider::models(std::function<void(const QStringList&, const QString&)> done)
{
    const QString base = m_account.baseUrl.trimmed().isEmpty() ? kind(m_account.kind)->baseUrl : m_account.baseUrl.trimmed();
    const QStringList fallback = kind(m_account.kind)->fallbackModels;
    get(QUrl(base + QStringLiteral("/v1/models?limit=100")), [this, done, fallback](int status, const QByteArray& body, const QString& error) {
        QStringList ids;
        for (const QJsonValue& v : QJsonDocument::fromJson(body).object().value(QStringLiteral("data")).toArray())
            ids << v.toObject().value(QStringLiteral("id")).toString();
        if (status == 200 && !ids.isEmpty())
            done(ids, {});
        else
            done(fallback, explainFailure(serviceName(), status, body, error));
    });
}

// --- OpenAI-kompatibel -------------------------------------------------------------------------

OpenAiProvider::OpenAiProvider(const Account& account, QObject* parent)
    : Provider(account, parent)
{
}

QString OpenAiProvider::base() const
{
    QString b = m_account.baseUrl.trimmed().isEmpty() ? kind(m_account.kind)->baseUrl : m_account.baseUrl.trimmed();
    while (b.endsWith(QLatin1Char('/')))
        b.chop(1);
    return b;
}

QUrl OpenAiProvider::endpoint() const
{
    return QUrl(base() + QStringLiteral("/chat/completions"));
}

QList<QPair<QByteArray, QByteArray>> OpenAiProvider::headers() const
{
    QString key = m_account.key.trimmed();
    if (key.isEmpty() && m_account.kind == QLatin1String("ollama"))
        key = QStringLiteral("ollama");
    if (key.isEmpty())
        return {};
    return {{"Authorization", "Bearer " + key.toUtf8()}};
}

QJsonObject OpenAiProvider::requestBody(const QString& system, const QList<Message>& history, const QList<Tool>& tools) const
{
    const bool images = m_account.kind != QLatin1String("deepseek");
    QJsonArray messages{QJsonObject{{QStringLiteral("role"), QStringLiteral("system")}, {QStringLiteral("content"), system}}};
    QList<QByteArray> pending; // Bilder aus Werkzeugergebnissen: als Benutzer-Nachricht nach den Ergebnissen
    auto flushImages = [&] {
        if (pending.isEmpty())
            return;
        QJsonArray content{QJsonObject{{QStringLiteral("type"), QStringLiteral("text")},
                                       {QStringLiteral("text"), QStringLiteral("Bild(er) zum letzten Werkzeugergebnis:")}}};
        for (const QByteArray& png : std::as_const(pending))
            content.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("image_url")},
                                       {QStringLiteral("image_url"),
                                        QJsonObject{{QStringLiteral("url"), QStringLiteral("data:image/png;base64,") + imageBase64(png)}}}});
        messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")}, {QStringLiteral("content"), content}});
        pending.clear();
    };
    for (const Message& m : history) {
        if (m.role != Message::Role::ToolResult)
            flushImages();
        switch (m.role) {
        case Message::Role::User:
            messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")}, {QStringLiteral("content"), m.text}});
            break;
        case Message::Role::Assistant:
            if (!m.text.trimmed().isEmpty())
                messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("assistant")}, {QStringLiteral("content"), m.text}});
            break;
        case Message::Role::ToolUse: {
            const QJsonObject call{{QStringLiteral("id"), m.callId},
                                   {QStringLiteral("type"), QStringLiteral("function")},
                                   {QStringLiteral("function"),
                                    QJsonObject{{QStringLiteral("name"), m.tool},
                                                {QStringLiteral("arguments"),
                                                 QString::fromUtf8(QJsonDocument(m.input).toJson(QJsonDocument::Compact))}}}};
            QJsonObject last = messages.last().toObject();
            if (last.value(QStringLiteral("role")).toString() == QLatin1String("assistant")) {
                QJsonArray calls = last.value(QStringLiteral("tool_calls")).toArray();
                calls.append(call);
                last.insert(QStringLiteral("tool_calls"), calls);
                messages.replace(messages.size() - 1, last);
            } else {
                messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("assistant")},
                                            {QStringLiteral("content"), QJsonValue::Null},
                                            {QStringLiteral("tool_calls"), QJsonArray{call}}});
            }
            break;
        }
        case Message::Role::ToolResult:
            messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("tool")},
                                        {QStringLiteral("tool_call_id"), m.callId},
                                        {QStringLiteral("content"), (m.error ? QStringLiteral("FEHLER: ") : QString()) + m.text}});
            if (images)
                pending << m.images;
            break;
        }
    }
    flushImages();
    QJsonObject body{{QStringLiteral("model"), model()},
                     {QStringLiteral("stream"), true},
                     {QStringLiteral("stream_options"), QJsonObject{{QStringLiteral("include_usage"), true}}},
                     {QStringLiteral("messages"), messages}};
    if (!tools.isEmpty()) {
        QJsonArray list;
        for (const Tool& t : tools)
            list.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("function")},
                                    {QStringLiteral("function"), QJsonObject{{QStringLiteral("name"), t.name},
                                                                             {QStringLiteral("description"), t.description},
                                                                             {QStringLiteral("parameters"), objectSchema(t.schema)}}}});
        body.insert(QStringLiteral("tools"), list);
    }
    return body;
}

bool OpenAiProvider::onEvent(const QByteArray&, const QByteArray& data)
{
    if (data == "[DONE]") {
        for (const Call& c : std::as_const(m_calls))
            m_result.calls << ToolCall{c.id.isEmpty() ? QStringLiteral("call_%1").arg(m_result.calls.size()) : c.id, c.name,
                                       QJsonDocument::fromJson(c.arguments.isEmpty() ? "{}" : c.arguments).object()};
        m_calls.clear();
        m_result.stop = m_finish == QLatin1String("length") ? QStringLiteral("max_tokens")
                        : m_result.calls.isEmpty()          ? QStringLiteral("end")
                                                            : QStringLiteral("tools");
        return true;
    }
    const QJsonObject e = QJsonDocument::fromJson(data).object();
    if (e.contains(QStringLiteral("error"))) {
        fail(explainFailure(serviceName(), 500, data, QString()));
        return false;
    }
    const QJsonObject usage = e.value(QStringLiteral("usage")).toObject();
    if (!usage.isEmpty()) {
        m_result.tokensIn = usage.value(QStringLiteral("prompt_tokens")).toInt();
        m_result.tokensOut = usage.value(QStringLiteral("completion_tokens")).toInt();
    }
    const QJsonArray choices = e.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty())
        return false;
    const QJsonObject choice = choices.first().toObject();
    const QJsonObject delta = choice.value(QStringLiteral("delta")).toObject();
    const QString piece = delta.value(QStringLiteral("content")).toString();
    if (!piece.isEmpty()) {
        m_result.text += piece;
        emit text(piece);
    }
    for (const QJsonValue& v : delta.value(QStringLiteral("tool_calls")).toArray()) {
        const QJsonObject t = v.toObject();
        const int index = t.value(QStringLiteral("index")).toInt(int(m_calls.size()));
        Call& c = m_calls[index];
        if (!t.value(QStringLiteral("id")).toString().isEmpty())
            c.id = t.value(QStringLiteral("id")).toString();
        const QJsonObject f = t.value(QStringLiteral("function")).toObject();
        if (!f.value(QStringLiteral("name")).toString().isEmpty() && c.name.isEmpty()) {
            c.name = f.value(QStringLiteral("name")).toString();
            emit toolComing(c.name);
        }
        c.arguments += f.value(QStringLiteral("arguments")).toString().toUtf8();
    }
    if (!choice.value(QStringLiteral("finish_reason")).toString().isEmpty())
        m_finish = choice.value(QStringLiteral("finish_reason")).toString();
    return false;
}

void OpenAiProvider::models(std::function<void(const QStringList&, const QString&)> done)
{
    const QStringList fallback = kind(m_account.kind)->fallbackModels;
    get(QUrl(base() + QStringLiteral("/models")), [this, done, fallback](int status, const QByteArray& body, const QString& error) {
        static const QRegularExpression skip(QStringLiteral("embedding|whisper|tts|dall-e|moderation|audio|image|realtime|"
                                                            "transcribe|search|similarity|edit|davinci|babbage|curie|ada"));
        QStringList ids;
        for (const QJsonValue& v : QJsonDocument::fromJson(body).object().value(QStringLiteral("data")).toArray()) {
            QString id = v.toObject().value(QStringLiteral("id")).toString();
            id.remove(QRegularExpression(QStringLiteral("^models/")));
            if (!id.isEmpty() && !id.contains(skip))
                ids << id;
        }
        std::sort(ids.begin(), ids.end(), std::greater<>());
        if (status == 200 && !ids.isEmpty())
            done(ids, {});
        else
            done(fallback, explainFailure(serviceName(), status, body, error));
    });
}

} // namespace vek::ai

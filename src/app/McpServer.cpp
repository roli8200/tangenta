#include "McpServer.h"

#include "AiTools.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>

namespace vek {

namespace {

constexpr qsizetype kMaxBody = 32 * 1024 * 1024; // SVG und Skripte können gross sein

QByteArray rpcResult(const QJsonValue& id, const QJsonObject& result)
{
    return QJsonDocument(QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("id"), id}, {QStringLiteral("result"), result}})
        .toJson(QJsonDocument::Compact);
}

QByteArray rpcError(const QJsonValue& id, int code, const QString& message)
{
    return QJsonDocument(QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                                     {QStringLiteral("id"), id},
                                     {QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), code}, {QStringLiteral("message"), message}}}})
        .toJson(QJsonDocument::Compact);
}

QByteArray reason(int status)
{
    switch (status) {
    case 200: return "OK";
    case 202: return "Accepted";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 413: return "Payload Too Large";
    default: return "Error";
    }
}

} // namespace

McpServer::McpServer(AiTools& tools, std::function<bool()> mayWrite, QObject* parent)
    : QObject(parent)
    , m_tools(tools)
    , m_mayWrite(std::move(mayWrite))
{
    QByteArray random(24, Qt::Uninitialized);
    for (char& c : random)
        c = char(QRandomGenerator::system()->bounded(256));
    m_token = QString::fromLatin1(random.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

McpServer::~McpServer() = default;

bool McpServer::start()
{
    if (running())
        return true;
    m_server = new QTcpServer(this);
    if (!m_server->listen(QHostAddress::LocalHost, 0)) {
        delete m_server;
        m_server = nullptr;
        return false;
    }
    connect(m_server, &QTcpServer::newConnection, this, [this] {
        while (QTcpSocket* socket = m_server->nextPendingConnection()) {
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] { readSocket(socket); });
            connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
                m_buffers.remove(socket);
                socket->deleteLater();
            });
        }
    });
    return true;
}

bool McpServer::running() const
{
    return m_server && m_server->isListening();
}

QString McpServer::url() const
{
    return running() ? QStringLiteral("http://127.0.0.1:%1/mcp").arg(m_server->serverPort()) : QString();
}

void McpServer::respond(QTcpSocket* socket, int status, const QByteArray& body, const QByteArray& contentType)
{
    QByteArray head = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason(status) + "\r\n";
    if (!body.isEmpty())
        head += "Content-Type: " + contentType + "\r\n";
    head += "Content-Length: " + QByteArray::number(body.size()) + "\r\nConnection: keep-alive\r\n\r\n";
    socket->write(head + body);
}

void McpServer::readSocket(QTcpSocket* socket)
{
    QByteArray& buffer = m_buffers[socket];
    buffer += socket->readAll();
    for (;;) {
        const qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return;
        const QList<QByteArray> lines = buffer.left(headerEnd).split('\n');
        const QList<QByteArray> requestLine = lines.value(0).trimmed().split(' ');
        QHash<QByteArray, QByteArray> headers;
        for (int i = 1; i < lines.size(); ++i) {
            const qsizetype colon = lines[i].indexOf(':');
            if (colon > 0)
                headers.insert(lines[i].left(colon).trimmed().toLower(), lines[i].mid(colon + 1).trimmed());
        }
        const qsizetype length = headers.value("content-length").toLongLong();
        if (length > kMaxBody) {
            respond(socket, 413, {});
            socket->disconnectFromHost();
            return;
        }
        if (buffer.size() < headerEnd + 4 + length)
            return; // Rest kommt noch
        const QByteArray body = buffer.mid(headerEnd + 4, length);
        buffer.remove(0, headerEnd + 4 + length);
        const QByteArray method = requestLine.value(0);
        const QByteArray path = requestLine.value(1).split('?').value(0);
        if (path != "/mcp") {
            respond(socket, 404, {});
            continue;
        }
        if (headers.value("authorization") != "Bearer " + m_token.toLatin1()) {
            respond(socket, 401, {});
            continue;
        }
        if (method == "DELETE") { // Sitzung beenden
            respond(socket, 200, {});
            continue;
        }
        if (method != "POST") { // kein Server-Push (GET/SSE)
            respond(socket, 405, {});
            continue;
        }
        QPointer<QTcpSocket> target(socket);
        handle(body, [target](int status, const QByteArray& json) {
            if (target)
                respond(target, status, json);
        });
    }
}

void McpServer::handle(const QByteArray& body, std::function<void(int, const QByteArray&)> reply)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return reply(400, rpcError(QJsonValue::Null, -32700, QStringLiteral("Parse error")));
    const QJsonObject msg = doc.object();
    const QString method = msg.value(QStringLiteral("method")).toString();
    const QJsonValue id = msg.value(QStringLiteral("id"));
    if (!msg.contains(QStringLiteral("id"))) // Mitteilung (z.B. notifications/initialized)
        return reply(202, {});
    const QJsonObject params = msg.value(QStringLiteral("params")).toObject();
    if (method == QLatin1String("initialize")) {
        const QString version = params.value(QStringLiteral("protocolVersion")).toString(QStringLiteral("2025-06-18"));
        return reply(200, rpcResult(id, {{QStringLiteral("protocolVersion"), version},
                                         {QStringLiteral("capabilities"), QJsonObject{{QStringLiteral("tools"), QJsonObject{}}}},
                                         {QStringLiteral("serverInfo"), QJsonObject{{QStringLiteral("name"), QStringLiteral("tangenta")},
                                                                                    {QStringLiteral("version"), QStringLiteral("1.0")}}},
                                         {QStringLiteral("instructions"),
                                          QStringLiteral("Werkzeuge des Vektorgrafikprogramms TANGENTA für das gerade offene Dokument.")}}));
    }
    if (method == QLatin1String("ping"))
        return reply(200, rpcResult(id, {}));
    if (method == QLatin1String("tools/list")) {
        QJsonArray list;
        for (const ai::Tool& t : m_tools.tools(m_mayWrite()))
            list.append(QJsonObject{{QStringLiteral("name"), t.name}, {QStringLiteral("description"), t.description}, {QStringLiteral("inputSchema"), t.schema}});
        return reply(200, rpcResult(id, {{QStringLiteral("tools"), list}}));
    }
    if (method == QLatin1String("tools/call")) {
        const QString name = params.value(QStringLiteral("name")).toString();
        const QJsonObject input = params.value(QStringLiteral("arguments")).toObject();
        emit toolStarted(name, input);
        QPointer<McpServer> self(this);
        m_tools.call(name, input, m_mayWrite(), [self, id, name, reply](const AiTools::Result& r) {
            QJsonArray content{QJsonObject{{QStringLiteral("type"), QStringLiteral("text")}, {QStringLiteral("text"), r.text}}};
            for (const QByteArray& png : r.images)
                content.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("image")},
                                           {QStringLiteral("data"), QString::fromLatin1(png.toBase64())},
                                           {QStringLiteral("mimeType"), QStringLiteral("image/png")}});
            if (self)
                emit self->toolFinished(name, !r.error, r.text.left(400));
            reply(200, rpcResult(id, {{QStringLiteral("content"), content}, {QStringLiteral("isError"), r.error}}));
        });
        return;
    }
    reply(200, rpcError(id, -32601, QStringLiteral("Unbekannte Methode: %1").arg(method)));
}

} // namespace vek

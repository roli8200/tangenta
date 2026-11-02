#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include <functional>

class QTcpServer;
class QTcpSocket;

namespace vek {

class AiTools;

// MCP-Schnittstelle (Model Context Protocol, "Streamable HTTP") für Claude Code: lauscht nur auf
// 127.0.0.1 an einem freien Port, verlangt ein zufälliges Bearer-Token und bietet die Werkzeuge
// von AiTools an (initialize, tools/list, tools/call, ping). Antworten kommen als JSON.
class McpServer : public QObject {
    Q_OBJECT

public:
    McpServer(AiTools& tools, std::function<bool()> mayWrite, QObject* parent = nullptr);
    ~McpServer() override;

    bool start();
    bool running() const;
    QString url() const;
    QString token() const { return m_token; }

    // Eine JSON-RPC-Nachricht behandeln (für Tests); reply bekommt die Antwort (leer bei Mitteilungen)
    void handle(const QByteArray& body, std::function<void(int status, const QByteArray& json)> reply);

signals:
    void toolStarted(const QString& name, const QJsonObject& input);
    void toolFinished(const QString& name, bool ok, const QString& summary);

private:
    void readSocket(QTcpSocket* socket);
    static void respond(QTcpSocket* socket, int status, const QByteArray& body, const QByteArray& contentType = "application/json");

    AiTools& m_tools;
    std::function<bool()> m_mayWrite;
    QTcpServer* m_server = nullptr;
    QString m_token;
    QHash<QTcpSocket*, QByteArray> m_buffers;
};

} // namespace vek

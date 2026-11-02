#include "ClaudeCode.h"

#include "Options.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace vek {

namespace {

// Werkzeuge der MCP-Schnittstelle heissen in Claude Code "mcp__tangenta__<name>"
const QString kServer = QStringLiteral("tangenta");

QString plainToolName(QString name)
{
    const QString prefix = QStringLiteral("mcp__") + kServer + QStringLiteral("__");
    if (name.startsWith(prefix))
        name.remove(0, prefix.length());
    return name;
}

QString resultText(const QJsonValue& content)
{
    if (content.isString())
        return content.toString();
    QStringList parts;
    for (const QJsonValue& v : content.toArray())
        if (v.toObject().value(QStringLiteral("type")).toString() == QLatin1String("text"))
            parts << v.toObject().value(QStringLiteral("text")).toString();
    return parts.join(QLatin1Char('\n'));
}

// Ohne diese Variablen rechnet Claude Code über das Abo statt über einen API-Schlüssel ab
QProcessEnvironment cleanEnvironment()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove(QStringLiteral("ANTHROPIC_API_KEY"));
    env.remove(QStringLiteral("ANTHROPIC_AUTH_TOKEN"));
    env.remove(QStringLiteral("CLAUDECODE")); // falls TANGENTA selbst aus Claude Code gestartet wurde
    return env;
}

QString notLoggedIn()
{
    return QObject::tr("Claude Code ist nicht angemeldet. Unter Optionen > Global > KI beim Zugang „Anmelden“ "
                       "wählen (oder in einer Konsole „claude auth login“).");
}

} // namespace

ClaudeCode::ClaudeCode(QObject* parent)
    : QObject(parent)
{
}

ClaudeCode::~ClaudeCode()
{
    abort();
}

QString ClaudeCode::executable()
{
    const QString configured = Options::instance().text(QStringLiteral("global/ai/claudePath")).trimmed();
    if (!configured.isEmpty() && QFileInfo(configured).isExecutable())
        return configured;
    const QString home = QDir::homePath();
    QStringList candidates;
#ifdef Q_OS_WIN
    const QString appData = qEnvironmentVariable("APPDATA");
    // npm legt claude.cmd an, das eigentliche Programm liegt im Paket
    const QString npmCmd = QStandardPaths::findExecutable(QStringLiteral("claude.cmd"));
    if (!npmCmd.isEmpty())
        candidates << QFileInfo(npmCmd).absolutePath() + QStringLiteral("/node_modules/@anthropic-ai/claude-code/bin/claude.exe");
    candidates << QStandardPaths::findExecutable(QStringLiteral("claude.exe"))
               << appData + QStringLiteral("/npm/node_modules/@anthropic-ai/claude-code/bin/claude.exe")
               << home + QStringLiteral("/.local/bin/claude.exe")
               << qEnvironmentVariable("LOCALAPPDATA") + QStringLiteral("/Programs/claude/claude.exe");
#else
    candidates << QStandardPaths::findExecutable(QStringLiteral("claude")) << home + QStringLiteral("/.local/bin/claude")
               << home + QStringLiteral("/.claude/local/claude") << home + QStringLiteral("/.npm-global/bin/claude");
#endif
    for (const QString& c : std::as_const(candidates))
        if (!c.isEmpty() && QFileInfo(c).isFile())
            return QDir::toNativeSeparators(c);
    return {};
}

void ClaudeCode::status(QObject* context, std::function<void(bool, bool, const QString&)> done)
{
    const QString exe = executable();
    if (exe.isEmpty()) {
        done(false, false, tr("Claude Code ist nicht installiert (claude.com/claude-code) oder nicht gefunden; der Pfad "
                              "lässt sich unter Optionen > Global > KI angeben."));
        return;
    }
    auto* process = new QProcess(context);
    process->setProcessEnvironment(cleanEnvironment());
    QObject::connect(process, &QProcess::finished, context, [process, done] {
        process->deleteLater();
        const QJsonObject o = QJsonDocument::fromJson(process->readAllStandardOutput()).object();
        if (o.isEmpty()) {
            done(true, false, tr("Claude Code antwortet nicht wie erwartet: %1").arg(QString::fromLocal8Bit(process->readAllStandardError()).trimmed()));
            return;
        }
        const bool loggedIn = o.value(QStringLiteral("loggedIn")).toBool();
        const QString plan = o.value(QStringLiteral("subscriptionType")).toString();
        const QString method = o.value(QStringLiteral("authMethod")).toString();
        QString info;
        if (!loggedIn)
            info = notLoggedIn();
        else if (method == QLatin1String("claude.ai"))
            info = tr("Angemeldet mit dem Claude-Konto %1%2.")
                       .arg(o.value(QStringLiteral("email")).toString(),
                            plan.isEmpty() ? QString() : QStringLiteral(" (") + tr("Abo: %1").arg(plan) + QLatin1Char(')'));
        else
            info = tr("Angemeldet (%1). Achtung: das ist kein Abo, sondern Abrechnung nach Verbrauch.").arg(method);
        done(true, loggedIn, info);
    });
    QObject::connect(process, &QProcess::errorOccurred, context, [process, done](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        process->deleteLater();
        done(false, false, tr("Claude Code lässt sich nicht starten: %1").arg(process->errorString()));
    });
    process->start(exe, {QStringLiteral("auth"), QStringLiteral("status"), QStringLiteral("--json")});
}

bool ClaudeCode::login()
{
    const QString exe = executable();
    if (exe.isEmpty())
        return false;
    QProcess process;
    process.setProcessEnvironment(cleanEnvironment());
#ifdef Q_OS_WIN
    // Eigenes Konsolenfenster: Claude Code öffnet den Browser und zeigt dort den Ablauf
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NEW_CONSOLE;
        args->startupInfo->dwFlags &= ~DWORD(STARTF_USESTDHANDLES);
    });
    process.setProgram(exe);
    process.setArguments({QStringLiteral("auth"), QStringLiteral("login")});
    return process.startDetached();
#else
    for (const QStringList& terminal : {QStringList{QStringLiteral("x-terminal-emulator"), QStringLiteral("-e")},
                                        QStringList{QStringLiteral("gnome-terminal"), QStringLiteral("--")},
                                        QStringList{QStringLiteral("konsole"), QStringLiteral("-e")},
                                        QStringList{QStringLiteral("xterm"), QStringLiteral("-e")}}) {
        if (QStandardPaths::findExecutable(terminal.first()).isEmpty())
            continue;
        return QProcess::startDetached(terminal.first(), {terminal.at(1), exe, QStringLiteral("auth"), QStringLiteral("login")});
    }
    return QProcess::startDetached(exe, {QStringLiteral("auth"), QStringLiteral("login")});
#endif
}

QStringList ClaudeCode::arguments(const QString&, const Request& o)
{
    QStringList args{QStringLiteral("-p"),
                     QStringLiteral("--output-format"), QStringLiteral("stream-json"),
                     QStringLiteral("--verbose"),
                     QStringLiteral("--include-partial-messages"),
                     // keine eigenen Werkzeuge (Dateien, Shell, Web), keine Einstellungen/Hooks des Benutzers
                     QStringLiteral("--tools"), QString(),
                     QStringLiteral("--setting-sources"), QString(),
                     QStringLiteral("--strict-mcp-config"),
                     QStringLiteral("--disable-slash-commands"),
                     QStringLiteral("--system-prompt"), o.system,
                     QStringLiteral("--model"), o.model.isEmpty() ? QStringLiteral("opus") : o.model};
    if (!o.mcpUrl.isEmpty()) {
        const QJsonObject server{{QStringLiteral("type"), QStringLiteral("http")},
                                 {QStringLiteral("url"), o.mcpUrl},
                                 {QStringLiteral("headers"), QJsonObject{{QStringLiteral("Authorization"), QStringLiteral("Bearer ") + o.mcpToken}}}};
        const QJsonObject config{{QStringLiteral("mcpServers"), QJsonObject{{kServer, server}}}};
        args << QStringLiteral("--mcp-config") << QString::fromUtf8(QJsonDocument(config).toJson(QJsonDocument::Compact))
             << QStringLiteral("--allowedTools") << QStringLiteral("mcp__") + kServer;
    }
    if (!o.session.isEmpty())
        args << QStringLiteral("--resume") << o.session;
    if (!o.keepSession)
        args << QStringLiteral("--no-session-persistence");
    return args;
}

void ClaudeCode::ask(const QString& question, const Request& options)
{
    abort();
    m_buffer.clear();
    m_errors.clear();
    m_answer.clear();
    m_session = options.session;
    m_toolNames.clear();
    m_done = false;
    const QString exe = executable();
    if (exe.isEmpty()) {
        m_done = true;
        emit failed(tr("Claude Code ist nicht installiert oder nicht gefunden (Optionen > Global > KI)."));
        return;
    }
    auto* process = new QProcess(this);
    m_process = process;
    process->setProcessEnvironment(cleanEnvironment());
    if (!options.directory.isEmpty()) {
        QDir().mkpath(options.directory);
        process->setWorkingDirectory(options.directory);
    }
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process] {
        m_buffer += process->readAllStandardOutput();
        for (;;) {
            const qsizetype end = m_buffer.indexOf('\n');
            if (end < 0)
                break;
            const QByteArray l = m_buffer.left(end).trimmed();
            m_buffer.remove(0, end + 1);
            if (!l.isEmpty())
                line(l);
        }
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, process] { m_errors += process->readAllStandardError(); });
    connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus) {
        process->deleteLater();
        if (!m_buffer.trimmed().isEmpty())
            line(m_buffer.trimmed());
        m_buffer.clear();
        if (m_done)
            return;
        m_done = true;
        const QString errors = QString::fromLocal8Bit(m_errors).trimmed();
        if (errors.contains(QLatin1String("login"), Qt::CaseInsensitive) || errors.contains(QLatin1String("authenticat"), Qt::CaseInsensitive))
            emit failed(notLoggedIn());
        else
            emit failed(tr("Claude Code wurde unerwartet beendet (Code %1).").arg(code) +
                        (errors.isEmpty() ? QString() : QStringLiteral("\n") + errors.left(600)));
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_done)
            return;
        m_done = true;
        process->deleteLater();
        emit failed(tr("Claude Code lässt sich nicht starten: %1").arg(process->errorString()));
    });
    process->start(exe, arguments(question, options));
    // Die Frage über die Standardeingabe: keine Längen- und Anführungszeichen-Probleme
    process->write(question.toUtf8());
    process->closeWriteChannel();
}

void ClaudeCode::line(const QByteArray& json)
{
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    const QString type = o.value(QStringLiteral("type")).toString();
    if (!o.value(QStringLiteral("session_id")).toString().isEmpty())
        m_session = o.value(QStringLiteral("session_id")).toString();
    if (type == QLatin1String("stream_event")) {
        const QJsonObject e = o.value(QStringLiteral("event")).toObject();
        const QString kind = e.value(QStringLiteral("type")).toString();
        if (kind == QLatin1String("message_start") && !m_answer.isEmpty() && !m_answer.endsWith(QLatin1String("\n\n"))) {
            m_answer += QStringLiteral("\n\n");
            emit text(QStringLiteral("\n\n"));
        } else if (kind == QLatin1String("content_block_start")) {
            const QJsonObject b = e.value(QStringLiteral("content_block")).toObject();
            if (b.value(QStringLiteral("type")).toString() == QLatin1String("tool_use")) {
                m_toolNames.insert(b.value(QStringLiteral("id")).toString(), plainToolName(b.value(QStringLiteral("name")).toString()));
                emit toolComing(plainToolName(b.value(QStringLiteral("name")).toString()));
            }
        } else if (kind == QLatin1String("content_block_delta")) {
            const QJsonObject d = e.value(QStringLiteral("delta")).toObject();
            if (d.value(QStringLiteral("type")).toString() == QLatin1String("text_delta")) {
                const QString piece = d.value(QStringLiteral("text")).toString();
                m_answer += piece;
                emit text(piece);
            }
        }
    } else if (type == QLatin1String("assistant")) {
        for (const QJsonValue& v : o.value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toArray()) {
            const QJsonObject b = v.toObject();
            if (b.value(QStringLiteral("type")).toString() == QLatin1String("tool_use"))
                m_toolNames.insert(b.value(QStringLiteral("id")).toString(), plainToolName(b.value(QStringLiteral("name")).toString()));
        }
    } else if (type == QLatin1String("user")) {
        for (const QJsonValue& v : o.value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toArray()) {
            const QJsonObject b = v.toObject();
            if (b.value(QStringLiteral("type")).toString() != QLatin1String("tool_result"))
                continue;
            const QString name = m_toolNames.value(b.value(QStringLiteral("tool_use_id")).toString(), tr("Werkzeug"));
            emit toolDone(name, !b.value(QStringLiteral("is_error")).toBool(), resultText(b.value(QStringLiteral("content"))).left(400));
        }
    } else if (type == QLatin1String("rate_limit_event")) {
        const QJsonObject info = o.value(QStringLiteral("rate_limit_info")).toObject();
        const QJsonObject windows = info.value(QStringLiteral("unifiedWindows")).toObject();
        const double five = windows.value(QStringLiteral("five_hour")).toObject().value(QStringLiteral("utilization")).toDouble(-1);
        const double week = windows.value(QStringLiteral("seven_day")).toObject().value(QStringLiteral("utilization")).toDouble(-1);
        if (five >= 0)
            emit usage(tr("Abo-Auslastung: %1 % (5 Stunden), %2 % (Woche)").arg(qRound(five * 100)).arg(qRound(std::max(0.0, week) * 100)));
    } else if (type == QLatin1String("result")) {
        if (m_done)
            return;
        m_done = true;
        const QJsonObject u = o.value(QStringLiteral("usage")).toObject();
        const int in = u.value(QStringLiteral("input_tokens")).toInt() + u.value(QStringLiteral("cache_creation_input_tokens")).toInt() +
                       u.value(QStringLiteral("cache_read_input_tokens")).toInt();
        const int out = u.value(QStringLiteral("output_tokens")).toInt();
        const QString result = o.value(QStringLiteral("result")).toString();
        if (o.value(QStringLiteral("is_error")).toBool()) {
            const QString lower = result.toLower();
            emit failed(lower.contains(QLatin1String("login")) || lower.contains(QLatin1String("api key")) ? notLoggedIn()
                        : result.isEmpty() ? tr("Claude Code meldet einen Fehler (%1).").arg(o.value(QStringLiteral("subtype")).toString())
                                           : result);
            return;
        }
        emit finished(m_answer.trimmed().isEmpty() ? result : m_answer, m_session, in, out);
    }
}

void ClaudeCode::abort()
{
    if (QProcess* process = m_process) {
        m_process = nullptr;
        m_done = true;
        process->disconnect(this);
        process->kill();
        process->waitForFinished(2000);
        process->deleteLater();
    }
}

bool ClaudeCode::busy() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

void ClaudeCode::test(const QString& model, std::function<void(bool, const QString&)> done)
{
    connect(this, &ClaudeCode::finished, this, [model, done](const QString& answer) {
        done(true, tr("Zugang funktioniert. Claude (%1) antwortet über das Abo: „%2“").arg(model, answer.trimmed().left(80)));
    }, Qt::SingleShotConnection);
    connect(this, &ClaudeCode::failed, this, [done](const QString& message) { done(false, message); }, Qt::SingleShotConnection);
    Request o;
    o.system = QStringLiteral("Antworte mit dem Wort: bereit.");
    o.model = model;
    o.keepSession = false;
    o.directory = QDir::tempPath();
    ask(QStringLiteral("Bereit?"), o);
}

} // namespace vek

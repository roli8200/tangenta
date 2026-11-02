#include "AiPanel.h"

#include "AiChat.h"
#include "ClaudeCode.h"
#include "Options.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalBlocker>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace vek {

namespace {

// Markdown der KI als HTML-Ausschnitt (Tabellen, Listen, Code, Links)
QString markdownHtml(const QString& text)
{
    QTextDocument doc;
    doc.setMarkdown(text);
    const QString html = doc.toHtml();
    const qsizetype body = html.indexOf(QLatin1String("<body"));
    const qsizetype start = html.indexOf(QLatin1Char('>'), body) + 1;
    const qsizetype end = html.lastIndexOf(QLatin1String("</body>"));
    return body < 0 || end < start ? text.toHtmlEscaped() : html.mid(start, end - start);
}

} // namespace

AiPanel::AiPanel(Editor& editor, Canvas& canvas, QMainWindow* window, QWidget* parent)
    : QWidget(parent)
    , m_chat(new AiChat(editor, canvas, window, this))
    , m_renderTimer(new QTimer(this))
    , m_clockTimer(new QTimer(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* top = new QHBoxLayout;
    m_account = new QComboBox(this);
    m_account->setObjectName(QStringLiteral("ai-account"));
    m_account->setToolTip(tr("KI-Zugang (Optionen > Global > KI)"));
    top->addWidget(m_account, 1);
    auto* settings = new QToolButton(this);
    settings->setText(QStringLiteral("⚙"));
    settings->setToolTip(tr("KI-Zugänge einrichten"));
    top->addWidget(settings);
    auto* fresh = new QToolButton(this);
    fresh->setObjectName(QStringLiteral("ai-new"));
    fresh->setText(tr("Neu"));
    fresh->setToolTip(tr("Neues Gespräch (der bisherige Verlauf geht nicht mehr mit)"));
    top->addWidget(fresh);
    layout->addLayout(top);

    m_view = new QTextBrowser(this);
    m_view->setObjectName(QStringLiteral("ai-view"));
    m_view->setOpenExternalLinks(true);
    m_view->document()->setDefaultStyleSheet(QStringLiteral(
        ".q { background-color: #e8f0fe; color: #0b2a55; } .step { color: #555555; } .bad { color: #b3261e; } "
        ".note { color: #777777; } .live { color: #1a56a8; }"));
    layout->addWidget(m_view, 1);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("ai-status"));
    m_status->setStyleSheet(QStringLiteral("color: #555;"));
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    m_input = new QPlainTextEdit(this);
    m_input->setObjectName(QStringLiteral("ai-input"));
    m_input->setPlaceholderText(tr("Was soll ich tun? z.B. „Zeichne ein Logo für eine Bäckerei“ (Enter sendet, Umschalt+Enter = neue Zeile)"));
    m_input->setMaximumHeight(90);
    m_input->installEventFilter(this);
    layout->addWidget(m_input);

    auto* bottom = new QHBoxLayout;
    m_mayWrite = new QCheckBox(tr("darf ändern"), this);
    m_mayWrite->setObjectName(QStringLiteral("ai-maywrite"));
    m_mayWrite->setToolTip(tr("Ohne Häkchen liest die KI nur (Dokument, Web, Bildersuche) und ändert nichts."));
    m_mayWrite->setChecked(Options::instance().flag(QStringLiteral("global/ai/mayWrite")));
    bottom->addWidget(m_mayWrite);
    bottom->addStretch(1);
    m_send = new QPushButton(tr("Senden"), this);
    m_send->setObjectName(QStringLiteral("ai-send"));
    bottom->addWidget(m_send);
    layout->addLayout(bottom);

    m_renderTimer->setSingleShot(true);
    m_renderTimer->setInterval(80);
    connect(m_renderTimer, &QTimer::timeout, this, &AiPanel::render);
    m_clockTimer->setInterval(1000);
    connect(m_clockTimer, &QTimer::timeout, this, &AiPanel::updateControls);

    connect(settings, &QToolButton::clicked, this, &AiPanel::optionsRequested);
    connect(fresh, &QToolButton::clicked, this, [this] {
        m_chat->reset();
        m_live.clear();
        saveConversation();
        render();
    });
    connect(m_account, &QComboBox::currentIndexChanged, this, [this] {
        const QString id = m_account->currentData().toString();
        for (const ai::Account& a : ai::accounts())
            if (a.id == id) {
                m_chat->setAccount(a);
                Options::instance().set(QStringLiteral("global/ai/account"), id);
            }
        updateControls();
    });
    connect(m_send, &QPushButton::clicked, this, [this] {
        if (m_chat->busy())
            m_chat->stop();
        else
            send(m_input->toPlainText());
    });
    connect(m_chat, &AiChat::started, this, [this] {
        m_live.clear();
        m_liveStatus.clear();
        m_clock.start();
        m_clockTimer->start();
        updateControls();
        render();
    });
    connect(m_chat, &AiChat::text, this, [this](const QString& piece) {
        m_live += piece;
        scheduleRender();
    });
    connect(m_chat, &AiChat::status, this, [this](const QString& line) {
        m_liveStatus = line;
        updateControls();
    });
    connect(m_chat, &AiChat::transcriptChanged, this, [this] {
        // Ein Schritt ist erledigt: die bis dahin gestreamte Antwort steht im Verlauf fort
        scheduleRender();
    });
    auto done = [this] {
        m_live.clear();
        m_liveStatus.clear();
        m_clockTimer->stop();
        saveConversation();
        updateControls();
        render();
    };
    connect(m_chat, &AiChat::finished, this, done);
    connect(m_chat, &AiChat::failed, this, done);

    // Erster Start: ist Claude Code installiert, gleich den Abo-Zugang anlegen
    if (ai::accounts().isEmpty() && !ClaudeCode::executable().isEmpty()) {
        ai::Account account;
        account.id = ai::newAccountId();
        account.kind = QStringLiteral("claude_code");
        ai::saveAccounts({account});
    }
    reloadAccounts();
    loadConversation();
    render();
}

AiPanel::~AiPanel()
{
    saveConversation();
}

void AiPanel::reloadAccounts()
{
    const QList<ai::Account> list = ai::accounts();
    const QString current = ai::currentAccount().id;
    {
        const QSignalBlocker block(m_account);
        m_account->clear();
        for (const ai::Account& a : list) {
            const ai::Kind* k = ai::kind(a.kind);
            const QString model = a.model.isEmpty() && k ? k->defaultModel : a.model;
            m_account->addItem(a.displayName() + (model.isEmpty() ? QString() : QStringLiteral(" · ") + model), a.id);
        }
        m_account->setCurrentIndex(std::max(0, m_account->findData(current)));
    }
    m_account->setVisible(!list.isEmpty());
    for (const ai::Account& a : list)
        if (a.id == m_account->currentData().toString())
            m_chat->setAccount(a);
    updateControls();
    render();
}

void AiPanel::send(const QString& question)
{
    if (question.trimmed().isEmpty() || m_chat->busy())
        return;
    Options::instance().set(QStringLiteral("global/ai/mayWrite"), m_mayWrite->isChecked());
    m_input->clear();
    m_chat->ask(question, m_mayWrite->isChecked());
}

bool AiPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_input && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && !(key->modifiers() & Qt::ShiftModifier)) {
            send(m_input->toPlainText());
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void AiPanel::updateControls()
{
    const bool busy = m_chat->busy();
    m_send->setText(busy ? tr("Stopp") : tr("Senden"));
    m_send->setToolTip(busy ? tr("Anhalten; was schon geändert ist, bleibt (Strg+Z nimmt es zurück)") : QString());
    m_send->setEnabled(busy || !ai::accounts().isEmpty());
    m_account->setEnabled(!busy);
    QString status;
    if (busy)
        status = tr("⏳ %1 s").arg(m_clock.elapsed() / 1000) + (m_liveStatus.isEmpty() ? QString() : QStringLiteral(" · ") + m_liveStatus);
    else if (ai::accounts().isEmpty())
        status = tr("Noch kein KI-Zugang: ⚙ oben rechts (Optionen > Global > KI).");
    m_status->setText(status);
    m_status->setVisible(!status.isEmpty());
}

void AiPanel::scheduleRender()
{
    if (!m_renderTimer->isActive())
        m_renderTimer->start();
}

void AiPanel::render()
{
    QScrollBar* bar = m_view->verticalScrollBar();
    const bool atEnd = bar->value() >= bar->maximum() - 4;
    QString html;
    if (m_chat->transcript().isEmpty() && !m_chat->busy())
        html += tr("<p class='note'>Die KI arbeitet direkt in diesem Dokument: zeichnen, Bilder suchen und einfügen, "
                   "nachzeichnen, ausrichten, im Web nachschlagen. Was sie in einer Antwort ändert, nimmt ein Strg+Z zurück.</p>");
    for (const AiChat::Entry& e : m_chat->transcript()) {
        switch (e.kind) {
        case AiChat::Entry::Kind::Question:
            html += QStringLiteral("<table width='100%' cellpadding='6' class='q'><tr><td><b>%1</b><br>%2</td></tr></table>")
                        .arg(tr("Du"), e.text.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>")));
            break;
        case AiChat::Entry::Kind::Answer:
            html += markdownHtml(e.text);
            break;
        case AiChat::Entry::Kind::Step:
            html += QStringLiteral("<div class='%1'><small>%2 %3</small></div>")
                        .arg(e.ok ? QStringLiteral("step") : QStringLiteral("bad"), e.ok ? QStringLiteral("✓") : QStringLiteral("⚠"), e.text.toHtmlEscaped());
            break;
        case AiChat::Entry::Kind::Note:
            html += QStringLiteral("<p class='%1'><small>%2</small></p>").arg(e.ok ? QStringLiteral("note") : QStringLiteral("bad"), e.text.toHtmlEscaped());
            break;
        }
    }
    if (m_chat->busy() && !m_chat->pending().trimmed().isEmpty())
        html += QStringLiteral("<div class='live'>") + markdownHtml(m_chat->pending()) + QStringLiteral("</div>");
    m_view->setHtml(html);
    if (atEnd || m_chat->busy())
        bar->setValue(bar->maximum());
}

QString AiPanel::transcriptText() const
{
    return m_view->toPlainText();
}

QString AiPanel::conversationFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/ki/gespraech.json");
}

void AiPanel::saveConversation() const
{
    const QString path = conversationFile();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(m_chat->save()).toJson(QJsonDocument::Compact));
}

void AiPanel::loadConversation()
{
    QFile file(conversationFile());
    if (file.open(QIODevice::ReadOnly))
        m_chat->load(QJsonDocument::fromJson(file.readAll()).object());
}

} // namespace vek

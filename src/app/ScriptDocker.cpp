#include "ScriptDocker.h"

#include "Editor.h"
#include "Icons.h"
#include "Options.h"
#include "ScriptHost.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QSplitter>
#include <QStandardPaths>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace vek {

namespace {

const char* kExample = R"(// TANGENTA-Skript (JavaScript). Masse in mm, Farben "#rrggbb" oder Namen wie "orange".
// tg.rectangle/ellipse/polygon/line/polyline/text legen Objekte an, tg.selection() liefert die Auswahl.
// Ein Lauf ist ein Rückgängig-Schritt (Strg+Z). Strg+Eingabe führt aus.

const colors = ["crimson", "orange", "gold", "seagreen", "royalblue", "purple"];
for (let i = 0; i < colors.length; i++) {
    const r = tg.rectangle(20 + i * 25, 20, 20, 20, 3);
    r.fill = colors[i];
    r.rotate(i * 15);
}
console.log("Seite", tg.currentPage, "von", tg.pageCount, "–", tg.pageWidth, "x", tg.pageHeight, "mm");
)";

} // namespace

QString ScriptDocker::scriptFolder()
{
    QString dir = Options::instance().value(QStringLiteral("ws/scripts/folder")).toString();
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/Skripte");
    return dir;
}

QStringList ScriptDocker::scripts()
{
    QStringList list;
    for (const QFileInfo& fi : QDir(scriptFolder()).entryInfoList({QStringLiteral("*.js")}, QDir::Files, QDir::Name))
        list << fi.absoluteFilePath();
    return list;
}

ScriptDocker::ScriptDocker(Editor& editor, QWidget* parent) : QWidget(parent), m_editor(editor)
{
    m_host = new ScriptHost(editor, this);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* bar = new QHBoxLayout;
    m_list = new QComboBox(this);
    m_list->setToolTip(tr("Skripte im Skriptordner"));
    bar->addWidget(m_list, 1);
    auto button = [this, bar](const QString& iconName, const QString& tip) {
        auto* b = new QToolButton(this);
        b->setIcon(icon(iconName));
        b->setToolTip(tip);
        b->setAutoRaise(true);
        bar->addWidget(b);
        return b;
    };
    QToolButton* run = button(QStringLiteral("script-run"), tr("Ausführen (Strg+Eingabe)"));
    QToolButton* save = button(QStringLiteral("doc-save"), tr("Im Skriptordner speichern"));
    QToolButton* folder = button(QStringLiteral("doc-open"), tr("Skriptordner öffnen"));
    layout->addLayout(bar);

    auto* split = new QSplitter(Qt::Vertical, this);
    m_code = new QPlainTextEdit(split);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_code->setFont(mono);
    m_code->setTabStopDistance(QFontMetricsF(mono).horizontalAdvance(QLatin1Char(' ')) * 4);
    m_code->setPlainText(QString::fromUtf8(kExample));
    m_output = new QPlainTextEdit(split);
    m_output->setReadOnly(true);
    m_output->setFont(mono);
    m_output->setPlaceholderText(tr("Ausgabe von console.log"));
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 1);
    layout->addWidget(split, 1);

    connect(run, &QToolButton::clicked, this, &ScriptDocker::runCode);
    auto* shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), m_code);
    shortcut->setContext(Qt::WidgetShortcut);
    connect(shortcut, &QShortcut::activated, this, &ScriptDocker::runCode);
    connect(save, &QToolButton::clicked, this, [this] {
        bool ok = false;
        QString name = QInputDialog::getText(this, tr("Skript speichern"), tr("Name:"), QLineEdit::Normal,
                                             QFileInfo(m_list->currentData().toString()).completeBaseName(), &ok);
        if (!ok || name.trimmed().isEmpty())
            return;
        name = name.trimmed();
        if (!name.endsWith(QLatin1String(".js")))
            name += QStringLiteral(".js");
        QDir().mkpath(scriptFolder());
        QFile f(QDir(scriptFolder()).filePath(name));
        if (f.open(QIODevice::WriteOnly | QIODevice::Text))
            f.write(m_code->toPlainText().toUtf8());
        refreshList();
        m_list->setCurrentIndex(m_list->findData(QFileInfo(f).absoluteFilePath()));
    });
    connect(folder, &QToolButton::clicked, this, [] {
        QDir().mkpath(scriptFolder());
        QDesktopServices::openUrl(QUrl::fromLocalFile(scriptFolder()));
    });
    connect(m_list, &QComboBox::activated, this, [this](int index) {
        QFile f(m_list->itemData(index).toString());
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            m_code->setPlainText(QString::fromUtf8(f.readAll()));
    });
    connect(&Options::instance(), &Options::changed, this, &ScriptDocker::refreshList);
    refreshList();
}

void ScriptDocker::refreshList()
{
    m_list->clear();
    m_list->addItem(tr("(Neues Skript)"), QString());
    for (const QString& path : scripts())
        m_list->addItem(QFileInfo(path).completeBaseName(), path);
}

QString ScriptDocker::outputText() const
{
    return m_output->toPlainText();
}

void ScriptDocker::showResult(bool ok, const QString& error, const QStringList& output)
{
    m_output->clear();
    for (const QString& line : output)
        m_output->appendPlainText(line);
    if (!ok)
        m_output->appendHtml(QStringLiteral("<span style=\"color:#c02020\">%1</span>").arg(error.toHtmlEscaped()));
}

void ScriptDocker::runCode()
{
    const QString name = QFileInfo(m_list->currentData().toString()).completeBaseName();
    const ScriptHost::Result r = m_host->run(m_code->toPlainText(), name);
    showResult(r.ok, r.error, r.output);
}

bool ScriptDocker::runFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        showResult(false, tr("%1 lässt sich nicht lesen.").arg(path), {});
        return false;
    }
    const ScriptHost::Result r = m_host->run(QString::fromUtf8(f.readAll()), QFileInfo(path).completeBaseName());
    showResult(r.ok, r.error, r.output);
    return r.ok;
}

} // namespace vek

#include "SpellDialog.h"

#include "Editor.h"
#include "Options.h"
#include "SpellChecker.h"
#include "TextObject.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTextCursor>
#include <QTextDocument>

#include <functional>

namespace vek {

SpellDialog::SpellDialog(Editor& editor, QWidget* parent) : QDialog(parent), m_editor(editor)
{
    setWindowTitle(tr("Rechtschreibprüfung"));
    auto* grid = new QGridLayout(this);
    grid->addWidget(new QLabel(tr("Nicht im Wörterbuch:"), this), 0, 0, 1, 2);
    m_context = new QLabel(this);
    m_context->setWordWrap(true);
    m_context->setMinimumWidth(360);
    m_context->setFrameShape(QFrame::StyledPanel);
    grid->addWidget(m_context, 1, 0, 1, 2);
    grid->addWidget(new QLabel(tr("Ersetzen durch:"), this), 2, 0, 1, 2);
    m_replaceWith = new QLineEdit(this);
    grid->addWidget(m_replaceWith, 3, 0);
    m_suggestions = new QListWidget(this);
    grid->addWidget(m_suggestions, 4, 0, 4, 1);

    auto button = [this, grid](const QString& text, int row, auto slot) {
        auto* b = new QPushButton(text, this);
        b->setAutoDefault(false);
        connect(b, &QPushButton::clicked, this, slot);
        grid->addWidget(b, row, 1);
        return b;
    };
    m_replaceButton = button(tr("&Ersetzen"), 3, [this] { replace(m_replaceWith->text()); });
    button(tr("&Ignorieren"), 4, [this] { ignoreOnce(); });
    button(tr("&Alle ignorieren"), 5, [this] { ignoreAll(); });
    button(tr("&Hinzufügen"), 6, [this] { addWord(); });
    m_status = new QLabel(this);
    grid->addWidget(m_status, 8, 0);
    auto* close = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);
    grid->addWidget(close, 8, 1);

    connect(m_suggestions, &QListWidget::currentTextChanged, m_replaceWith, &QLineEdit::setText);
    connect(m_suggestions, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) { replace(item->text()); });

    m_ids = texts();
    // Bei ausgewähltem Text dort beginnen
    const NodeId selected = m_editor.selection().value(0);
    if (const int i = int(m_ids.indexOf(selected)); i >= 0)
        m_index = i;
    findNext();
}

QList<NodeId> SpellDialog::texts() const
{
    QList<NodeId> ids;
    const Document& doc = m_editor.document();
    std::function<void(const Node&)> visit = [&](const Node& n) {
        if (const auto* t = node_cast<TextObject>(&n)) {
            // Verkettete Rahmen: der Text steht im ersten
            const QList<NodeId> chain = t->chain();
            if (chain.isEmpty() || chain.first() == t->id())
                ids << t->id();
            return;
        }
        for (int i = 0; i < n.childCount(); ++i)
            visit(*n.child(i));
    };
    visit(doc.masterPage());
    for (int p = 0; p < doc.pageCount(); ++p)
        visit(doc.page(p));
    return ids;
}

QString SpellDialog::textOf(NodeId id) const
{
    const auto* t = m_editor.document().findAs<TextObject>(id);
    return t ? t->toDocument()->toPlainText() : QString();
}

bool SpellDialog::findNext()
{
    SpellChecker& sc = SpellChecker::instance();
    m_start = -1;
    m_word.clear();
    for (int n = 0; n <= m_ids.size() && !m_ids.isEmpty(); ++n) {
        const QString text = textOf(m_ids.value(m_index));
        for (const SpellChecker::Range& r : sc.check(text)) {
            if (r.start < m_pos)
                continue;
            m_start = r.start;
            m_word = text.mid(r.start, r.length);
            m_editor.setSelection({m_ids.value(m_index)});
            showCurrent();
            return true;
        }
        // Nächster Text, am Ende wieder von vorn bis zum Ausgangspunkt
        m_index = (m_index + 1) % int(m_ids.size());
        m_pos = 0;
    }
    showCurrent();
    return false;
}

void SpellDialog::showCurrent()
{
    m_suggestions->clear();
    const bool found = m_start >= 0;
    m_replaceButton->setEnabled(found);
    if (!found) {
        m_context->setText(QString());
        m_replaceWith->clear();
        m_status->setText(SpellChecker::instance().available() ? tr("Rechtschreibprüfung abgeschlossen.")
                                                              : tr("Kein Wörterbuch für %1 gefunden.").arg(SpellChecker::instance().language()));
        return;
    }
    const QString text = textOf(m_ids.value(m_index));
    const int from = std::max(0, m_start - 40), to = std::min(int(text.size()), m_start + int(m_word.size()) + 40);
    m_context->setText(text.mid(from, m_start - from).toHtmlEscaped() + QStringLiteral("<b style=\"color:#c02020\">")
                       + m_word.toHtmlEscaped() + QStringLiteral("</b>")
                       + text.mid(m_start + m_word.size(), to - m_start - m_word.size()).toHtmlEscaped());
    const QStringList list = SpellChecker::instance().suggestions(
        m_word, std::max(1, Options::instance().integer(QStringLiteral("ws/text/spell/suggestions"))));
    m_suggestions->addItems(list);
    m_replaceWith->setText(list.value(0, m_word));
    if (!list.isEmpty())
        m_suggestions->setCurrentRow(0);
    m_status->clear();
}

void SpellDialog::replace(const QString& with)
{
    if (m_start < 0)
        return;
    const int start = m_start, length = int(m_word.size());
    m_editor.modifyText(m_ids.value(m_index), [start, length, with](TextObject& t) {
        std::unique_ptr<QTextDocument> d = t.toDocument();
        QTextCursor c(d.get());
        c.setPosition(start);
        c.setPosition(start + length, QTextCursor::KeepAnchor);
        c.insertText(with);
        t.setFromDocument(*d);
    }, tr("Rechtschreibung"));
    m_pos = start + int(with.size());
    findNext();
}

void SpellDialog::ignoreOnce()
{
    if (m_start < 0)
        return;
    m_pos = m_start + int(m_word.size());
    findNext();
}

void SpellDialog::ignoreAll()
{
    if (m_start < 0)
        return;
    SpellChecker::instance().ignore(m_word);
    findNext();
}

void SpellDialog::addWord()
{
    if (m_start < 0)
        return;
    SpellChecker::instance().addToUserDictionary(m_word);
    findNext();
}

} // namespace vek

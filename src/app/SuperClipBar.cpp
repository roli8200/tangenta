#include "SuperClipBar.h"

#include "Canvas.h"
#include "Editor.h"
#include "Icons.h"

#include <QHBoxLayout>
#include <QMenu>
#include <QSignalBlocker>
#include <QToolButton>

namespace vek {

SuperClipBar::SuperClipBar(Canvas& canvas)
    : QFrame(&canvas), m_canvas(canvas)
{
    setObjectName(QStringLiteral("superClipBar"));
    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);
    setStyleSheet(QStringLiteral("#superClipBar{background:#f4f4f4;border:1px solid #9a9a9a;border-radius:3px;}"));
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(3, 2, 3, 2);
    row->setSpacing(1);
    Editor& ed = canvas.editor();
    auto button = [&](const QString& iconName, const QString& tip, const std::function<void()>& fn) {
        auto* b = new QToolButton(this);
        b->setIcon(icon(iconName));
        b->setToolTip(tip);
        b->setAutoRaise(true);
        if (fn)
            connect(b, &QToolButton::clicked, this, fn);
        row->addWidget(b);
        return b;
    };
    m_edit = button(QStringLiteral("clip-edit"), tr("SuperClip bearbeiten"), [&ed] {
        if (ed.selectSuperClipContents())
            ed.clearSelection();
    });
    m_select = button(QStringLiteral("clip-select"), tr("SuperClip-Inhalt auswählen"), [&ed] { ed.selectSuperClipContents(); });
    m_extract = button(QStringLiteral("clip-extract"), tr("Inhalt extrahieren"), [&ed] { ed.extractContents(); });
    m_lock = button(QStringLiteral("clip-lock"), tr("Inhalt sperren (bewegt sich mit dem Rahmen)"), {});
    m_lock->setCheckable(true);
    connect(m_lock, &QToolButton::toggled, this, [&ed](bool on) { ed.setSuperClipContentsLocked(on); });
    m_fit = button(QStringLiteral("clip-fit"), tr("Inhalt einpassen"), {});
    m_fit->setPopupMode(QToolButton::InstantPopup);
    auto* fitMenu = new QMenu(m_fit);
    using Fit = Editor::ClipFit;
    for (const auto& [fit, text] : {std::pair{Fit::Center, tr("Inhalt &zentrieren")}, std::pair{Fit::FitProportional, tr("Inhalt proportional &einpassen")},
                                    std::pair{Fit::FillProportional, tr("Rahmen proportional &füllen")}, std::pair{Fit::Stretch, tr("Inhalt auf Rahmengrösse &strecken")}})
        fitMenu->addAction(text, this, [&ed, f = fit] { ed.fitSuperClipContents(f); });
    m_fit->setMenu(fitMenu);
    m_finish = new QToolButton(this);
    m_finish->setIcon(icon(QStringLiteral("clip-finish")));
    m_finish->setText(tr("Fertig"));
    m_finish->setToolTip(tr("SuperClip-Bearbeitung beenden (Esc)"));
    m_finish->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_finish->setAutoRaise(true);
    connect(m_finish, &QToolButton::clicked, this, [&ed] { ed.endSuperClipEdit(); });
    row->addWidget(m_finish);
    hide();
}

void SuperClipBar::sync(bool editing, bool locked)
{
    for (QToolButton* b : {m_edit, m_select, m_extract, m_lock, m_fit})
        b->setVisible(!editing);
    m_finish->setVisible(editing);
    const QSignalBlocker block(m_lock);
    m_lock->setChecked(locked);
    adjustSize();
}

} // namespace vek

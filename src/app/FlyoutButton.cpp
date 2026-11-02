#include "FlyoutButton.h"

#include <QAction>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>

namespace vek {

namespace {

constexpr int kTriangle = 5;     // Kantenlänge des Dreiecks in px
constexpr int kTriangleHit = 11; // Klickfläche unten rechts, die die Liste öffnet

} // namespace

FlyoutButton::FlyoutButton(Kind kind, const QList<QAction*>& actions, QWidget* parent)
    : QToolButton(parent), m_kind(kind)
{
    auto* menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("flyout"));
    menu->addActions(actions);
    setMenu(menu);
    setPopupMode(kind == Kind::ToolGroup ? QToolButton::DelayedPopup : QToolButton::InstantPopup);
    if (kind == Kind::Commands)
        return;
    // Der Knopf zeigt das zuletzt gewählte Werkzeug der Gruppe
    for (QAction* a : actions) {
        if (!a->isCheckable())
            continue;
        if (!defaultAction())
            setDefaultAction(a);
        connect(a, &QAction::toggled, this, [this, a](bool on) {
            if (on)
                setDefaultAction(a);
        });
    }
}

void FlyoutButton::mousePressEvent(QMouseEvent* event)
{
    const QPoint p = event->position().toPoint();
    if (m_kind == Kind::ToolGroup && event->button() == Qt::LeftButton && p.x() >= width() - kTriangleHit
        && p.y() >= height() - kTriangleHit) {
        showMenu();
        return;
    }
    QToolButton::mousePressEvent(event);
}

void FlyoutButton::paintEvent(QPaintEvent* event)
{
    QToolButton::paintEvent(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(isEnabled() ? QColor(0x1d, 0x1d, 0x1d) : QColor(0xa0, 0xa0, 0xa0));
    const QPointF corner(width() - 2.0, height() - 2.0);
    p.drawPolygon(QPolygonF{corner, corner - QPointF(kTriangle, 0), corner - QPointF(0, kTriangle)});
}

} // namespace vek

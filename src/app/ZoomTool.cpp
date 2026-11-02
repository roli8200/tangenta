#include "ZoomTool.h"

#include "Canvas.h"
#include "Icons.h"
#include "Options.h"

#include <QMenu>
#include <QMouseEvent>

#include <cmath>

namespace vek {

namespace {

void zoomContextMenu(Canvas& canvas, QMouseEvent* event)
{
    QMenu menu(&canvas);
    const QPointF at = event->position();
    menu.addAction(QObject::tr("Vergrössern"), &canvas, [&canvas, at] { canvas.zoomBy(2.0, at); });
    menu.addAction(QObject::tr("Verkleinern"), &canvas, [&canvas, at] { canvas.zoomBy(0.5, at); });
    menu.addAction(QObject::tr("Seite einpassen"), &canvas, [&canvas] { canvas.fitPage(); });
    menu.addAction(QObject::tr("Zoom 100 %"), &canvas, [&canvas] { canvas.setZoomPercent(100.0); });
    menu.exec(event->globalPosition().toPoint());
}

} // namespace

void PanTool::activate()
{
    m_canvas.setCursor(Qt::OpenHandCursor);
}

void PanTool::mousePress(QMouseEvent* event, const QPointF&)
{
    if (event->button() == Qt::RightButton) {
        if (Options::instance().integer(QStringLiteral("ws/tools/zoom/rightPan")) == 1)
            zoomContextMenu(m_canvas, event);
        else
            m_canvas.zoomBy(0.5, event->position());
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;
    m_dragging = true;
    m_last = event->position();
    m_canvas.setCursor(Qt::ClosedHandCursor);
}

void PanTool::mouseMove(QMouseEvent* event, const QPointF&)
{
    if (!m_dragging)
        return;
    m_canvas.panBy(event->position() - m_last);
    m_last = event->position();
}

void PanTool::mouseRelease(QMouseEvent* event, const QPointF&)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_dragging = false;
    m_canvas.setCursor(Qt::OpenHandCursor);
}

void ZoomTool::activate()
{
    m_canvas.setCursor(QCursor(icon(QStringLiteral("zoom-in")).pixmap(24), 10, 10));
}

void ZoomTool::mousePress(QMouseEvent* event, const QPointF&)
{
    if (event->button() == Qt::RightButton) {
        if (Options::instance().integer(QStringLiteral("ws/tools/zoom/rightZoom")) == 1) {
            zoomContextMenu(m_canvas, event); // Kontextmenü statt Verkleinern
            return;
        }
        m_canvas.zoomBy(0.5, event->position());
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;
    m_dragging = true;
    m_startScreen = m_currentScreen = event->position();
}

void ZoomTool::mouseMove(QMouseEvent* event, const QPointF&)
{
    if (!m_dragging)
        return;
    m_currentScreen = event->position();
    m_canvas.update();
}

void ZoomTool::mouseRelease(QMouseEvent* event, const QPointF&)
{
    if (event->button() != Qt::LeftButton || !m_dragging)
        return;
    m_dragging = false;
    const QRectF r = QRectF(m_startScreen, event->position()).normalized();
    if (r.width() > 4 && r.height() > 4)
        m_canvas.zoomToRect(QRectF(m_canvas.toDoc(r.topLeft()), m_canvas.toDoc(r.bottomRight())));
    else
        m_canvas.zoomBy(event->modifiers() & Qt::ShiftModifier ? 0.5 : 2.0, event->position());
    m_canvas.update();
}

void ZoomTool::paintOverlay(QPainter& p)
{
    if (!m_dragging)
        return;
    p.save();
    p.setPen(QPen(QColor(80, 80, 80), 1, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(m_startScreen, m_currentScreen).normalized());
    p.restore();
}

} // namespace vek

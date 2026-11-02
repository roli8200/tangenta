#include "ArtisticMediaTool.h"

#include "Canvas.h"
#include "PathOps.h"

#include <QKeyEvent>
#include <QMouseEvent>

#include <cmath>

namespace vek {

void ArtisticMediaTool::activate()
{
    m_canvas.setCursor(Qt::CrossCursor);
}

void ArtisticMediaTool::deactivate()
{
    m_drawing = false;
    m_stroke.clear();
}

void ArtisticMediaTool::setMedia(const ArtisticMedia& media)
{
    m_media = media;
    // Geänderte Einstellungen gelten auch für die ausgewählten Medienkurven
    Editor& ed = m_canvas.editor();
    for (const DrawObject* o : ed.selectedObjects()) {
        const auto* curve = node_cast<CurveObject>(o);
        if (!curve || !curve->media().isActive() || curve->media() == media)
            continue;
        ed.modifyCurve(curve->id(), [&media](CurveObject& c) { c.setMedia(media); },
                       QObject::tr("Künstlerische Medien"));
    }
    m_canvas.update();
}

void ArtisticMediaTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_drawing = true;
    m_stroke = {doc};
    m_manual = 1.0;
    m_pressures = {std::clamp(event->points().isEmpty() ? 1.0 : event->points().constFirst().pressure(), 0.05, 1.0)};
}

QList<double> ArtisticMediaTool::resamplePressure(const QList<double>& values, int n)
{
    QList<double> out;
    if (values.isEmpty())
        return out;
    for (int i = 0; i < n; ++i) {
        const double x = double(i) / (n - 1) * (values.size() - 1);
        const int a = std::min(int(x), int(values.size()) - 1), b = std::min(a + 1, int(values.size()) - 1);
        out << values[a] + (values[b] - values[a]) * (x - a);
    }
    return out;
}

void ArtisticMediaTool::mouseMove(QMouseEvent* event, const QPointF& doc)
{
    if (!m_drawing)
        return;
    m_stroke.append(doc);
    // Druck vom Stift (Maus: 1), mal dem Faktor der Pfeiltasten
    const double device = event->points().isEmpty() ? 1.0 : event->points().constFirst().pressure();
    m_pressures.append(std::clamp((device > 0 ? device : 1.0) * m_manual, 0.05, 1.0));
    m_canvas.update();
}

void ArtisticMediaTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton || !m_drawing)
        return;
    m_drawing = false;
    m_stroke.append(doc);
    m_pressures.append(m_pressures.isEmpty() ? 1.0 : m_pressures.last());
    const QList<QPointF> stroke = m_stroke;
    const QList<double> pressures = m_pressures;
    m_stroke.clear();
    m_pressures.clear();
    m_canvas.update();

    // Glättung: höhere Werte erlauben mehr Abweichung von der Mausspur, also weniger Knoten
    const double tolerance = m_canvas.pixelsToDoc(0.5 + m_smoothing / 100.0 * 8.0);
    QList<QPointF> thinned;
    for (const QPointF& p : stroke) {
        if (thinned.isEmpty() || std::hypot(p.x() - thinned.last().x(), p.y() - thinned.last().y()) >= tolerance)
            thinned.append(p);
    }
    if (thinned.size() < 2)
        return; // nur geklickt
    if (thinned.last() != stroke.last())
        thinned.append(stroke.last());
    SubPath spine = pathops::fitPolyline(thinned, false, tolerance);
    if (spine.nodes.size() < 2)
        return;
    auto curve = std::make_unique<CurveObject>(QList<SubPath>{spine});
    ArtisticMedia media = m_media;
    if (media.kind == ArtisticMedia::Kind::Pressure)
        media.pressure = resamplePressure(pressures);
    if (media.kind == ArtisticMedia::Kind::Sprayer)
        media.seed = quint32(stroke.size() * 7919 + int(stroke.first().x() * 13));
    curve->setMedia(media);
    // Der Strich ist eine Fläche: Füllung in der Konturfarbe, keine Kontur
    Editor& ed = m_canvas.editor();
    Style style = ed.defaultStyle();
    style.fill = Fill::solid(style.stroke.value_or(Color(0, 0, 0)));
    style.stroke.reset();
    curve->setStyle(style);
    ed.addObject(std::move(curve), QObject::tr("Künstlerische Medien"));
}

bool ArtisticMediaTool::keyPress(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && m_drawing) {
        m_drawing = false;
        m_stroke.clear();
        m_pressures.clear();
        m_canvas.update();
        return true;
    }
    // Druck mit der Maus: Pfeil auf/ab macht den Strich breiter bzw. schmaler (wie)
    if (m_drawing && m_media.kind == ArtisticMedia::Kind::Pressure && (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)) {
        m_manual = std::clamp(m_manual + (event->key() == Qt::Key_Up ? 0.1 : -0.1), 0.1, 1.0);
        return true;
    }
    return false;
}

void ArtisticMediaTool::paintOverlay(QPainter& p)
{
    if (!m_drawing || m_stroke.size() < 2)
        return;
    QPainterPath spine(m_stroke.first());
    for (int i = 1; i < m_stroke.size(); ++i)
        spine.lineTo(m_stroke[i]);
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    QColor fill = Canvas::previewColor();
    fill.setAlpha(110);
    p.setPen(QPen(Canvas::previewColor(), 1.0));
    p.setBrush(fill);
    ArtisticMedia preview = m_media;
    if (preview.kind == ArtisticMedia::Kind::Pressure)
        preview.pressure = resamplePressure(m_pressures);
    p.drawPath(m_canvas.viewTransform().map(pathops::mediaOutline(spine, preview)));
    p.restore();
}

} // namespace vek

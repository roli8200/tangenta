#pragma once

#include "DrawObject.h"
#include "SnapEngine.h"
#include "Tool.h"

#include <QPointF>

#include <memory>

namespace vek {

// Grundform über ihr Begrenzungsrechteck aufziehen: Rechteck, Ellipse, Polygon, Stern,
// komplexer Stern. Strg = Quadrat bzw. Kreis, Umschalt = vom Mittelpunkt aus.
// Start- und Endpunkt rasten ein; danach ist wieder das Auswahlwerkzeug aktiv.
class RectangleTool final : public Tool {
public:
    enum class Shape { Rectangle, Ellipse, Polygon, Star, ComplexStar, Spiral, GraphPaper };
    // Spirale im Rechteck r (Seitenkoordinaten): symmetrisch oder logarithmisch
    static std::unique_ptr<DrawObject> makeSpiral(const QRectF& r, int revolutions, bool logarithmic, int expansion);
    RectangleTool(Canvas& canvas, Shape shape) : Tool(canvas), m_shape(shape) {}

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    bool keyPress(QKeyEvent* event) override;
    void paintOverlay(QPainter& p) override;

private:
    QRectF currentRect(Qt::KeyboardModifiers mods) const;
    // Neues Objekt im Rechteck r (Seitenkoordinaten) mit den Vorgaben des Editors
    std::unique_ptr<DrawObject> create(const QRectF& r) const;
    QString label() const;
    QPointF snapped(const QPointF& doc);

    Shape m_shape;
    SnapResult m_snap;
    bool m_dragging = false;
    QPointF m_start;
    QPointF m_current;
    Qt::KeyboardModifiers m_mods;
};

} // namespace vek

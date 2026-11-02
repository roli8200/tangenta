#pragma once

#include "Tool.h"

#include <QList>
#include <QPointF>
#include <QTimer>

#include <memory>

namespace vek {

class DrawObject;

// Smart-Zeichnen (Umschalt+S) mit Formerkennung: Freihand zeichnen; nach der
// Verzögerung (Optionen > Formerkennung) wird der Strich zu einer erkannten Grundform
// (Linie, Rechteck, Ellipse, Dreieck, Viereck) oder zu einer geglätteten Kurve.
class SmartDrawingTool final : public Tool {
public:
    explicit SmartDrawingTool(Canvas& canvas);

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    void paintOverlay(QPainter& p) override;

    // Erkennung ohne Werkzeug (Tests): Strich in Seitenkoordinaten -> Objekt
    static std::unique_ptr<DrawObject> recognize(const QList<QPointF>& stroke, double pixelInMm);
    // Sofort umwandeln statt nach der Verzögerung
    void finishNow();

private:
    QList<QPointF> m_stroke;
    QList<QList<QPointF>> m_pending; // gezeichnet, noch nicht umgewandelt
    bool m_drawing = false;
    QTimer m_timer;
};

} // namespace vek

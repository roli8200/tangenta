#pragma once

#include "CurveObject.h"
#include "Tool.h"

#include <QList>
#include <QPointF>

#include <algorithm>

namespace vek {

// Künstlerische Medien (I): ein frei gezogener Strich wird geglättet und
// als Kurve mit veränderlicher Breite angelegt. Die Kurve bleibt mit dem Formwerkzeug
// bearbeitbar, der Strich folgt ihr. Einstellungen gelten für neue Striche und für
// ausgewählte Medienkurven.
class ArtisticMediaTool final : public Tool {
public:
    using Tool::Tool;

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    bool keyPress(QKeyEvent* event) override;
    void paintOverlay(QPainter& p) override;

    const ArtisticMedia& media() const { return m_media; }
    void setMedia(const ArtisticMedia& media);
    // Freihandglättung 0..100
    int smoothing() const { return m_smoothing; }
    void setSmoothing(int value) { m_smoothing = std::clamp(value, 0, 100); }

private:
    ArtisticMedia m_media{ArtisticMedia::Kind::Preset, 0, 10.0, 30.0};
    int m_smoothing = 50;
    bool m_drawing = false;
    QList<QPointF> m_stroke;
    QList<double> m_pressures; // je Punkt (Stift-Druck mal Pfeiltasten-Faktor)
    double m_manual = 1.0;      // Druck mit Pfeil auf/ab beim Zeichnen mit der Maus

public:
    // Druckverlauf aus aufgenommenen Werten auf n gleich verteilte Stützstellen
    static QList<double> resamplePressure(const QList<double>& values, int n = 32);
};

} // namespace vek

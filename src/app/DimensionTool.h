#pragma once

#include "DimensionObject.h"
#include "SnapEngine.h"
#include "Tool.h"

#include <QList>
#include <QPointF>

namespace vek {

// Bemassungswerkzeuge: Parallel-, Horizontal/Vertikal- und Winkelbemassung.
// Klick auf den ersten und zweiten Punkt (bzw. Scheitel und zwei Schenkel), dann die Lage der
// Masslinie mit der Maus bestimmen und klicken. Punkte rasten ein; Esc bricht ab. Masszahl
// nach Optionen > Bemassung bzw. Winkelmasse.
class DimensionTool final : public Tool {
public:
    enum class Mode { Parallel, HorizontalVertical, Angular };
    DimensionTool(Canvas& canvas, Mode mode) : Tool(canvas), m_mode(mode) {}

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent*, const QPointF&) override {}
    bool keyPress(QKeyEvent* event) override;
    void paintOverlay(QPainter& p) override;

private:
    int pointsNeeded() const { return m_mode == Mode::Angular ? 4 : 3; }
    std::unique_ptr<DimensionObject> make(const QList<QPointF>& points) const;

    Mode m_mode;
    QList<QPointF> m_points;
    QPointF m_current;
    SnapResult m_snap;
};

} // namespace vek

#pragma once

#include "SnapEngine.h"
#include "Tool.h"

#include <QList>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>

#include <algorithm>
#include <optional>

namespace vek {

// Beschneiden: Rahmen aufziehen, verschieben und an den Griffen ändern;
// Doppelklick in den Rahmen oder Eingabe schneidet die Auswahl (ohne Auswahl alles auf der
// Seite) auf den Rahmen zu. Was ganz ausserhalb liegt, verschwindet. Esc löscht den Rahmen.
class CropTool final : public Tool {
public:
    using Tool::Tool;

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    void mouseDoubleClick(QMouseEvent* event, const QPointF& doc) override;
    bool keyPress(QKeyEvent* event) override;
    void paintOverlay(QPainter& p) override;

    const std::optional<QRectF>& frame() const { return m_frame; }
    void setFrame(const std::optional<QRectF>& frame);
    bool apply();

private:
    enum class Drag { None, New, Move, Handle };
    int handleAt(const QPointF& screen) const;
    QPointF handlePoint(int handle, const QRectF& r) const;
    QPointF snapped(const QPointF& doc);

    std::optional<QRectF> m_frame;
    Drag m_drag = Drag::None;
    int m_handle = -1;
    QPointF m_start;
    QRectF m_startFrame;
    SnapResult m_snap;
};

// Messer: Klick, Klick schneidet gerade, Ziehen schneidet frei. Jedes Objekt, das die Linie
// durchquert (bei einer Auswahl nur diese), zerfällt in zwei Teile.
class KnifeTool final : public Tool {
public:
    using Tool::Tool;

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    bool keyPress(QKeyEvent* event) override;
    void paintOverlay(QPainter& p) override;

    // Schnitt entlang der Linie (Seitenkoordinaten); true, wenn etwas geteilt wurde
    bool cut(const QPolygonF& line);

private:
    QPointF snapped(const QPointF& doc);

    bool m_pressed = false;
    bool m_dragging = false;
    bool m_pending = false; // erster Punkt gesetzt, zweiter Klick fehlt
    QPointF m_pressScreen;
    QPolygonF m_line;
    QPointF m_current;
    SnapResult m_snap;
};

// Radierer (X): entfernt entlang der Mausspur alles unter der runden oder eckigen Spitze aus
// den ausgewählten Objekten (ohne Auswahl aus allen getroffenen). Flächen bleiben ein Objekt.
class EraserTool final : public Tool {
public:
    using Tool::Tool;

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    void paintOverlay(QPainter& p) override;

    // Durchmesser bzw. Kantenlänge der Spitze in mm
    double width() const { return m_width; }
    void setWidth(double mm) { m_width = std::max(0.01, mm); }
    bool round() const { return m_round; }
    void setRound(bool round) { m_round = round; }
    // Spur (Seitenkoordinaten) ausradieren; true, wenn sich etwas geändert hat
    bool erase(const QPolygonF& track);
    QPainterPath eraseArea(const QPolygonF& track) const;

private:
    double m_width = 1.0;
    bool m_round = true;
    bool m_pressed = false;
    QPolygonF m_track;
    std::optional<QPointF> m_mouse;
};

} // namespace vek

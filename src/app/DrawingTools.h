#pragma once

#include "PathOps.h"
#include "Tool.h"

#include <memory>

#include <QList>
#include <QPointF>

namespace vek {

// B-Spline: Klicks setzen Steuerpunkte, die Kurve läuft glatt an ihnen
// vorbei. Doppelklick oder Eingabe beendet, Klick auf den ersten Punkt schliesst, Esc verwirft.
class BSplineTool final : public Tool {
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

    int pendingPoints() const { return int(m_points.size()); }
    // Kurve aus den gesetzten Punkten anlegen
    void finish(bool closed);

private:
    QList<QPointF> m_points;
    QPointF m_mouse;
    bool m_hasMouse = false;
};

// Polylinie: Klicks setzen gerade Strecken, Ziehen zeichnet frei dazwischen;
// Doppelklick oder Eingabe beendet, Klick auf den Anfang schliesst, Esc verwirft
class PolylineTool final : public Tool {
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

    void finish(bool closed);

private:
    QList<QPointF> m_points;
    QPointF m_mouse;
    QPointF m_pressScreen;
    bool m_pressed = false;
    bool m_hasMouse = false;
};

// 3-Punkt-Rechteck, 3-Punkt-Ellipse und 3-Punkt-Kurve: erst eine Linie ziehen
// (Grundlinie bzw. Achse bzw. Anfang–Ende), dann mit der Maus Höhe bzw. Wölbung bestimmen und
// klicken
class ThreePointTool final : public Tool {
public:
    enum class Shape { Rectangle, Ellipse, Curve };
    ThreePointTool(Canvas& canvas, Shape shape) : Tool(canvas), m_shape(shape) {}

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    bool keyPress(QKeyEvent* event) override;
    void paintOverlay(QPainter& p) override;

    // Objekt aus den drei Punkten (Seitenkoordinaten); nullptr bei Nulllinie
    static std::unique_ptr<DrawObject> build(Shape shape, const QPointF& a, const QPointF& b, const QPointF& c);

private:
    Shape m_shape;
    int m_phase = 0; // 0 = nichts, 1 = Linie wird gezogen, 2 = dritter Punkt
    QPointF m_a, m_b, m_c;
};

// Smart-Füllung: Klick in eine von Formen umschlossene Fläche legt dort ein neues
// Objekt in genau dieser Form an (z.B. die Schnittfläche zweier Kreise). Füllfarbe und Kontur
// stehen in der Eigenschaftsleiste.
class SmartFillTool final : public Tool {
public:
    using Tool::Tool;

    void activate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent*, const QPointF&) override {}
    void paintOverlay(QPainter& p) override;

    // Fläche unter doc: Schnitt aller geschlossenen Formen, die doc enthalten, ohne die übrigen
    QPainterPath regionAt(const QPointF& doc) const;
    // Neues Objekt anlegen; liefert seine Id (0 = keine umschlossene Fläche)
    NodeId fillAt(const QPointF& doc);

private:
    QPainterPath m_hover;
};

// Virtuelles Segment löschen: Klick auf ein Linienstück löscht es bis zu den
// nächsten Schnittpunkten (mit anderen Objekten oder sich selbst); ein aufgezogener Rahmen
// löscht alle Stücke, die ganz darin liegen.
class VirtualDeleteTool final : public Tool {
public:
    using Tool::Tool;

    void activate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    void paintOverlay(QPainter& p) override;

    // Stück unter doc löschen; true, wenn etwas gelöscht wurde
    bool deleteAt(const QPointF& doc);
    // Alle Stücke ganz im Rechteck löschen
    bool deleteInside(const QRectF& area);

private:
    // Objekt in Linienzügen (Seite) und die übrigen Objekte als Linienzüge
    bool prepare(const DrawObject& o, QList<SubPath>* paths, QList<QPolygonF>* others) const;
    bool replace(const DrawObject& o, const QList<SubPath>& paths, const QString& label);

    QPointF m_press;
    QPointF m_current;
    bool m_pressed = false;
    bool m_dragging = false;
    QPolygonF m_hoverPiece; // Stück unter der Maus (rot markiert)
};

} // namespace vek

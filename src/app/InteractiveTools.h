#pragma once

#include "CurveObject.h"
#include "Effects.h"
#include "Tool.h"

#include <QIcon>
#include <QPolygonF>

#include <optional>

namespace vek {

// Interaktive Werkzeuge (Ausklappmenü neben Transparenz/Schatten).

// Überblendung: von einem Objekt auf ein anderes ziehen
class BlendTool final : public Tool {
public:
    explicit BlendTool(Canvas& canvas) : Tool(canvas) {}
    void activate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    void paintOverlay(QPainter& p) override;

private:
    NodeId m_from = 0;
    QPointF m_press, m_current;
    bool m_dragging = false;
};

// Kontur: vom Objektrand nach aussen oder innen ziehen; Stufen, Abstand, Farben in der Leiste
class ContourTool final : public Tool {
public:
    explicit ContourTool(Canvas& canvas) : Tool(canvas) {}
    void activate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    void paintOverlay(QPainter& p) override;
    // Kontur aus einem Zug (aussen, wenn der Zug von der Mitte wegführt)
    static ContourEffect contourFor(const DrawObject& o, const QPointF& press, const QPointF& to, const ContourEffect& base);

private:
    NodeId m_target = 0;
    QPointF m_press;
    bool m_dragging = false;
    std::optional<ContourEffect> m_preview;
};

// Extrusion: vom Objekt in Richtung des Fluchtpunkts ziehen
class ExtrudeTool final : public Tool {
public:
    explicit ExtrudeTool(Canvas& canvas) : Tool(canvas) {}
    void activate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    void paintOverlay(QPainter& p) override;

private:
    NodeId m_target = 0;
    bool m_dragging = false;
    std::optional<Extrusion> m_preview;
};

// Verzerren: Stossen/Ziehen, Zickzack, Wirbel; Ziehen vom Objekt bestimmt die Stärke
class DistortTool final : public Tool {
public:
    enum class Mode { PushPull, Zipper, Twister };
    explicit DistortTool(Canvas& canvas) : Tool(canvas) {}
    void activate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    void paintOverlay(QPainter& p) override;
    // Linienzüge (Seitenkoordinaten) um center verzerren; amount: Stossen/Ziehen in % (−/+),
    // Zickzack Amplitude in mm, Wirbel Grad
    static QList<SubPath> distort(const QList<SubPath>& paths, Mode mode, double amount, int frequency, const QPointF& center);
    static Mode mode();

private:
    double amountFor(const QPointF& to) const;
    NodeId m_target = 0;
    QList<SubPath> m_original;
    QPointF m_center, m_press, m_current;
    bool m_dragging = false;
};

// Hülle: acht Griffe (Ecken und Seitenmitten) verformen die Hülle des Objekts;
// das Objekt selbst bleibt erhalten (Text bleibt Text). Der Modus (gerade, ein Bogen, zwei Bögen,
// ohne Einschränkung) bestimmt, wie die Ränder einem Griff folgen.
class EnvelopeTool final : public Tool {
public:
    explicit EnvelopeTool(Canvas& canvas) : Tool(canvas) {}
    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    bool keyPress(QKeyEvent* event) override;
    void paintOverlay(QPainter& p) override;

    bool active() const { return m_target != 0; }
    // Griff (Reihenfolge wie Envelope::handles) auf doc (Seite) ziehen; ein Undo-Schritt
    void moveHandle(int handle, const QPointF& doc);
    // Modus für neue Griffbewegungen (Optionen ws/tools/envelope/mode)
    static Envelope::Mode mode();
    // Vorlagen wie im Andockfenster "Hülle": Name und Form im Einheitsquadrat (y nach unten)
    static QList<std::pair<QString, QPainterPath>> presets();
    static QIcon presetIcon(const QPainterPath& shape);

private:
    // Ziel und bearbeitete Hülle aus der Auswahl übernehmen
    void refresh();
    // Griffe in Seitenkoordinaten
    QList<QPointF> pageHandles() const;
    NodeId m_target = 0;
    QList<Envelope> m_base; // Hüllen darunter (bleiben)
    Envelope m_env;         // bearbeitete Hülle, lokale Koordinaten
    Envelope m_start;       // beim Drücken
    QPainterPath m_local;   // Form unter m_env (lokal)
    QTransform m_toPage;
    int m_drag = -1;
};

} // namespace vek

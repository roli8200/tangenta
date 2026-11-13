#pragma once

#include "Style.h"
#include "Tool.h"

#include "Node.h"

#include <optional>

namespace vek {

class DrawObject;

class NodePopup;

// Interaktive Füllung (G): über ein Objekt ziehen legt einen Farbverlauf
// von der Start- zur Endstelle an; die beiden Endpunkte lassen sich danach verschieben.
// Ein Klick auf einen Knoten (Endpunkt oder Zwischenknoten) wählt ihn aus und zeigt daneben ein
// kleines Fenster (Farbe, Pipette, Transparenz, Lage); eine Palettenfarbe färbt diesen Knoten.
// Doppelklick auf die Verlaufslinie fügt einen Knoten ein, Rechtsklick auf einen Zwischenknoten
// entfernt ihn; eine aus der Palette gezogene Farbe färbt den Knoten bzw. fügt auf der Linie einen
// ein. Strg beim Ziehen: Winkel in 15°-Schritten. Funktioniert für jedes Zeichenobjekt.
class InteractiveFillTool final : public Tool {
public:
    using Tool::Tool;

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    void mouseDoubleClick(QMouseEvent* event, const QPointF& doc) override;
    bool keyPress(QKeyEvent* event) override;
    bool paletteColor(const std::optional<Color>& color, bool fill) override;
    bool colorDropped(const Color& color, const QPointF& screen) override;
    void paintOverlay(QPainter& p) override;

    // Knotenfenster (nullptr, solange keines gezeigt wurde) und gewählter Knoten (Index in stops)
    NodePopup* popup() const { return m_popup; }
    int selectedStop() const { return m_selectedStop; }

private:
    enum class Drag { None, Create, Start, End, Mid, Stop };

    const DrawObject* target() const;
    QPointF toUnit(const DrawObject& o, const QPointF& doc) const;
    QPointF fromUnit(const DrawObject& o, const QPointF& unit) const;
    QPointF constrained(const QPointF& from, const QPointF& to, Qt::KeyboardModifiers mods) const;
    Fill baseFill(const DrawObject& o) const;
    // Knoten unter der Bildschirmstelle: Index in stops, -1 = keiner (Mittelpunkt-Regler zählt nicht)
    int stopAt(const DrawObject& o, const Fill& f, const QPointF& screen) const;
    // Lage 0..1 auf der Verlaufslinie, wenn screen auf ihr liegt
    std::optional<double> lineAt(const DrawObject& o, const Fill& f, const QPointF& screen) const;
    QPointF stopScreen(const DrawObject& o, const Fill& f, int stop) const;
    // Zwischenknoten werden einzeln gezeigt (nicht als Mittelpunkt-Regler)
    static bool showsStops(const Fill& f);
    int insertStop(Fill& f, double position, std::optional<Color> color) const;
    void commit(const Fill& f, const QString& label);
    void showPopup();
    void hidePopup();
    void pickColor(const QPointF& screen);

public:
    // Mittelpunkt eines Zweifarbverlaufs wie im Verlaufsdialog (Lage des Mittelstopps mit der
    // Mischfarbe); leer bei eigenen Zwischenfarben
    static std::optional<double> midpoint(const Fill& f);
    static void setMidpoint(Fill& f, double position);
    // Farbe des Verlaufs an der Lage t (0..1)
    static Color colorAt(const Fill& f, double t);

private:
    Drag m_drag = Drag::None;
    NodeId m_target = InvalidNodeId;
    QPointF m_pressDoc;
    QPointF m_pressScreen;
    bool m_moved = false;
    std::optional<Fill> m_preview;
    int m_selectedStop = -1; // Index in stops
    NodePopup* m_popup = nullptr;
    bool m_eyedropper = false;
};

} // namespace vek

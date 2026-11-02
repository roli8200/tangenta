#pragma once

#include "Style.h"
#include "Tool.h"

#include <QList>

#include <optional>

namespace vek {

class DrawObject;

// Hilfsmittel 'Maschenfüllung' (M): Klick in ein ausgewähltes Objekt legt ein
// Netz mit den Spalten/Reihen aus den Optionen an. Knoten anklicken (Umschalt = hinzufügen,
// Aufziehen = Auswahlrahmen) und ziehen verschiebt sie, eine Palettenfarbe färbt die gewählten
// Knoten (ohne Auswahl alle). Strg+A wählt alle Knoten, Entf löscht das Netz, Esc hebt die
// Knotenauswahl auf.
class MeshTool final : public Tool {
public:
    explicit MeshTool(Canvas& canvas) : Tool(canvas) {}

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    bool keyPress(QKeyEvent* event) override;
    bool paletteColor(const std::optional<Color>& color, bool fill) override;
    void paintOverlay(QPainter& p) override;

    const QList<int>& selectedNodes() const { return m_nodes; }
    // Netz neu aufteilen (Eigenschaftsleiste); Farben werden aus dem alten Netz übernommen
    void setGrid(int columns, int rows);
    static bool canHaveMesh(const DrawObject& o);

private:
    const DrawObject* target() const;
    QPointF toUnit(const DrawObject& o, const QPointF& doc) const;
    QPointF fromUnit(const DrawObject& o, const QPointF& unit) const;
    int nodeAt(const DrawObject& o, const MeshFill& mesh, const QPointF& doc) const;

    QList<int> m_nodes;
    enum class Drag { None, Nodes, Marquee } m_drag = Drag::None;
    QPointF m_pressDoc;
    QPointF m_currentDoc;
    std::optional<Fill> m_preview;
};

} // namespace vek

#pragma once

#include "SnapEngine.h"
#include "TableObject.h"
#include "Tool.h"

#include <memory>

namespace vek {

// Hilfsmittel 'Tabelle': Ziehen legt eine Tabelle mit den Zeilen/Spalten aus
// den Optionen an. Klick in eine Zelle einer bestehenden Tabelle beginnt die Bearbeitung dieser
// Zelle; Tippen schreibt hinein, Tab springt je nach Option zur nächsten Zelle oder fügt ein
// Tabulatorzeichen ein, Umschalt+Tab zurück, Pfeiltasten wechseln die Zelle bei leerem Text.
// Eine Palettenfarbe (Linksklick) färbt den Hintergrund der aktuellen Zelle. Esc oder ein Klick
// ausserhalb beendet die Bearbeitung; erst dann entsteht ein Rückgängig-Schritt.
class TableTool final : public Tool {
public:
    explicit TableTool(Canvas& canvas);
    ~TableTool() override;

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    bool keyPress(QKeyEvent* event) override;
    bool capturesKeyboard() const override { return editing(); }
    bool paletteColor(const std::optional<Color>& color, bool fill) override;
    void paintOverlay(QPainter& p) override;

    bool editing() const { return m_proto != nullptr; }
    NodeId target() const { return m_target; }
    int row() const { return m_row; }
    int column() const { return m_col; }
    void beginEditing(NodeId table, int row, int column);
    void finishEditing();
    // Markierter Zellbereich (Umschalt+Klick oder Ziehen erweitert)
    TableObject::Range selection() const;
    void selectRange(const TableObject::Range& range);

private:
    // Nächste/vorige Zelle in Tabulatorreihenfolge
    void step(int direction);
    QPointF snapped(const QPointF& doc);

    std::unique_ptr<TableObject> m_proto; // Arbeitskopie während der Bearbeitung
    NodeId m_target = 0;
    int m_row = 0;
    int m_col = 0;
    int m_anchorRow = 0; // anderes Ende des markierten Bereichs
    int m_anchorCol = 0;
    bool m_selecting = false;
    bool m_dragging = false;
    QPointF m_start;
    QPointF m_current;
    SnapResult m_snap;
};

} // namespace vek

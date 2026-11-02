#pragma once

#include "DrawObject.h"
#include "TextObject.h"

#include <QList>
#include <QSizeF>
#include <QString>

#include <optional>

namespace vek {

// Tabelle (Hilfsmittel 'Tabelle'): Zeilen und Spalten mit eigenen Breiten
// und Höhen, je Zelle Text und auf Wunsch eine eigene Hintergrundfarbe. Füllung des Objekts =
// Zellhintergrund, Kontur = Rahmen und Gitterlinien. Lokal im Rechteck (0,0)–size; Skalieren
// verteilt sich auf Spalten und Zeilen. Zellen lassen sich verbinden: die Zelle oben links
// eines Bereichs trägt rowSpan/colSpan, die übrigen sind verdeckt.
class TableObject final : public DrawObject {
public:
    struct Cell {
        QString text;
        std::optional<Color> fill;
        int rowSpan = 1;
        int colSpan = 1;
        friend bool operator==(const Cell&, const Cell&) = default;
    };
    // Zellbereich (einschliesslich)
    struct Range {
        int row0 = 0, col0 = 0, row1 = 0, col1 = 0;
        bool contains(int r, int c) const { return r >= row0 && r <= row1 && c >= col0 && c <= col1; }
        friend bool operator==(const Range&, const Range&) = default;
    };

    TableObject() = default;
    TableObject(const QSizeF& size, int rows, int columns);

    QString typeName() const override { return QStringLiteral("table"); }
    QPainterPath localPath() const override;
    void applyTransform(const QTransform& m) override;
    bool hitTest(const QPointF& point, double tolerance, bool treatAsFilled) const override;

    QSizeF size() const;
    int rows() const { return int(m_rowHeights.size()); }
    int columns() const { return int(m_colWidths.size()); }
    // Zeilen/Spalten am Ende hinzufügen oder entfernen; die Tabelle behält ihre Grösse
    void setRows(int rows);
    void setColumns(int columns);
    const QList<double>& columnWidths() const { return m_colWidths; }
    const QList<double>& rowHeights() const { return m_rowHeights; }

    // Tabelle-Menü: einfügen (Höhe/Breite wie die Nachbarzeile, die Tabelle wächst), löschen
    void insertRow(int at);
    void insertColumn(int at);
    void removeRow(int at);
    void removeColumn(int at);
    // Zeilen bzw. Spalten im Bereich gleich hoch/breit verteilen
    void distributeRows(int from, int to);
    void distributeColumns(int from, int to);
    // Zellen verbinden; Text der verdeckten Zellen hängt an
    void merge(const Range& range);
    // Verbundene Zelle wieder in Einzelzellen teilen
    void unmerge(int row, int column);
    // Zelle in n Zeilen bzw. Spalten aufteilen ("In Zeilen aufteilen")
    void splitIntoRows(int row, int column, int n);
    void splitIntoColumns(int row, int column, int n);

    Cell cell(int row, int column) const;
    void setCellText(int row, int column, const QString& text);
    void setCellFill(int row, int column, std::optional<Color> fill);
    // Zelle, die (row, column) abdeckt (bei verbundenen Zellen die oben links)
    std::pair<int, int> anchorOf(int row, int column) const;
    bool isCovered(int row, int column) const;
    // Zellrechteck lokal (verbundene Zellen über ihre ganze Fläche); Zelle an einer lokalen
    // Stelle (-1 = keine), immer die Ankerzelle
    QRectF cellRect(int row, int column) const;
    std::pair<int, int> cellAt(const QPointF& local) const;

    // Gitterlinien (lokal) und Text je Zelle (lokal, gefärbt)
    QPainterPath gridPath() const;
    QList<TextObject::Piece> textPieces() const;
    CharFormat textFormat() const { return m_format; }
    void setTextFormat(const CharFormat& f);

    static constexpr double kCellMargin = 1.0; // mm Innenabstand

    QList<PropertyDescriptor> propertyDescriptors() const override;
    QVariant propertyValue(const QString& id) const override;
    bool setPropertyValue(const QString& id, const QVariant& value) override;

    void writeProperties(QJsonObject& out) const override;
    void readProperties(const QJsonObject& in) override;

private:
    int index(int row, int column) const { return row * columns() + column; }
    void invalidate() { m_cache.reset(); }
    QRectF spanRect(int row, int column, int rowSpan, int colSpan) const;

    QList<double> m_colWidths;  // mm
    QList<double> m_rowHeights; // mm
    QList<Cell> m_cells;        // zeilenweise
    CharFormat m_format{QStringLiteral("Arial"), 10.0};
    mutable std::shared_ptr<const QList<TextObject::Piece>> m_cache;
};

} // namespace vek

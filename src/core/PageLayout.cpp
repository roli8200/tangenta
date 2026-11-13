#include "PageLayout.h"

#include <algorithm>

namespace vek::pagelayout {

QList<QList<int>> spreads(int pageCount, bool facing, bool startRight)
{
    QList<QList<int>> result;
    int i = 0;
    if (facing && startRight && pageCount > 0)
        result.append(QList<int>{i++});
    while (i < pageCount) {
        if (facing && i + 1 < pageCount) {
            result.append(QList<int>{i, i + 1});
            i += 2;
        } else {
            result.append(QList<int>{i++});
        }
    }
    return result;
}

QList<QRectF> arrange(const QList<QSizeF>& sizes, const Settings& s)
{
    const int count = int(sizes.size());
    QList<QRectF> result(count);
    if (count == 0)
        return result;
    // Einheiten: Seite oder Bogen, mit Lage der Seiten innerhalb der Einheit
    struct Unit {
        QList<std::pair<int, double>> pages; // Index, x in der Einheit
        QSizeF size;
    };
    QList<Unit> units;
    for (const QList<int>& spread : spreads(count, s.facing, s.startRight)) {
        Unit u;
        double x = 0;
        // Einzelne rechte Seite (Beginn rechts): links bleibt Platz für die fehlende linke Seite
        if (s.facing && s.startRight && spread.size() == 1 && spread.first() == 0)
            x = sizes[0].width();
        for (int index : spread) {
            u.pages.append({index, x});
            x += sizes[index].width();
            u.size.setHeight(std::max(u.size.height(), sizes[index].height()));
        }
        u.size.setWidth(x);
        units.append(u);
    }
    int columns = 1;
    switch (s.arrangement) {
    case Arrangement::Horizontal: columns = int(units.size()); break;
    case Arrangement::Vertical: columns = 1; break;
    case Arrangement::Grid: columns = std::max(1, s.facing ? s.columns / 2 : s.columns); break;
    }
    const int rows = (int(units.size()) + columns - 1) / columns;
    QList<double> colWidth(columns, 0.0);
    QList<double> rowHeight(rows, 0.0);
    for (int i = 0; i < units.size(); ++i) {
        colWidth[i % columns] = std::max(colWidth[i % columns], units[i].size.width());
        rowHeight[i / columns] = std::max(rowHeight[i / columns], units[i].size.height());
    }
    const double spacing = std::max(0.0, s.spacing);
    for (int i = 0; i < units.size(); ++i) {
        const int c = i % columns;
        const int r = i / columns;
        double x = 0;
        for (int k = 0; k < c; ++k)
            x += colWidth[k] + spacing;
        double y = 0;
        for (int k = 0; k < r; ++k)
            y += rowHeight[k] + spacing;
        x += (colWidth[c] - units[i].size.width()) / 2.0;
        for (const auto& [index, px] : units[i].pages)
            result[index] = QRectF(QPointF(x + px, y), sizes[index]);
    }
    return result;
}

} // namespace vek::pagelayout

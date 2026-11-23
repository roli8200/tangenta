#include "BaselineGrid.h"

#include "Options.h"

namespace vek {

// Die Rasterwerte über die Options-Fassade der App (Undo, Signal changed); der Kern liest
// dieselben Schlüssel mit BaselineGrid::fromJson direkt aus der Masterseite.
BaselineGrid BaselineGrid::fromOptions()
{
    const Options& o = Options::instance();
    BaselineGrid g;
    g.top = o.number(QStringLiteral("doc/grid/baseline/top"));
    g.step = o.number(QStringLiteral("doc/grid/baseline/step"));
    g.bottom = o.number(QStringLiteral("doc/grid/baseline/bottom"));
    g.moved = parseMoved(o.text(QStringLiteral("doc/grid/baseline/moved")));
    return g;
}

QVariantMap BaselineGrid::toOptions() const
{
    return {{QStringLiteral("doc/grid/baseline/top"), top},
            {QStringLiteral("doc/grid/baseline/step"), step},
            {QStringLiteral("doc/grid/baseline/bottom"), bottom},
            {QStringLiteral("doc/grid/baseline/moved"), formatMoved(moved)}};
}

} // namespace vek

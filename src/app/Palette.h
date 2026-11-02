#pragma once

#include "Color.h"

#include <QList>
#include <QString>

namespace vek {

struct NamedColor {
    QString name;
    Color color;
};

// Standard-RGB-Palette (Reihenfolge wie die Farbleiste am rechten Rand)
const QList<NamedColor>& defaultPalette();

// Name einer Palettenfarbe, leer wenn die Farbe nicht in der Palette steht
QString colorName(const Color& c);

} // namespace vek

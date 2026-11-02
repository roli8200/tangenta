#pragma once

#include <QIcon>
#include <QString>

namespace vek {

// Symbol aus :/icons/<name>.svg. Wird bei Bedarf in genau der benötigten Pixelgrösse aus
// dem SVG gerendert und bleibt so bei jeder Skalierung scharf. Rendert selbst über
// QSvgRenderer, damit kein SVG-Plugin zur Laufzeit nötig ist.
QIcon icon(const QString& name);

} // namespace vek

#include "UiScale.h"

#include <QGuiApplication>
#include <QSettings>

#include <algorithm>

namespace vek {

InterfaceScale interfaceScale()
{
    QSettings settings;
    InterfaceScale s;
    s.fitScreen = settings.value(QStringLiteral("ui/fitScreen"), true).toBool();
    s.factor = std::clamp(settings.value(QStringLiteral("ui/scale"), 1.0).toDouble(), 0.5, 3.0);
    return s;
}

void setInterfaceScale(const InterfaceScale& scale)
{
    QSettings settings;
    settings.setValue(QStringLiteral("ui/fitScreen"), scale.fitScreen);
    settings.setValue(QStringLiteral("ui/scale"), scale.factor);
}

QList<double> interfaceScaleSteps()
{
    return {0.8, 0.9, 1.0, 1.1, 1.25, 1.5, 1.75, 2.0};
}

void applyInterfaceScale()
{
    // Werte aus der Umgebung haben Vorrang (z.B. zum Testen)
    const InterfaceScale s = interfaceScale();
    if (s.fitScreen && !qEnvironmentVariableIsSet("QT_USE_PHYSICAL_DPI"))
        qputenv("QT_USE_PHYSICAL_DPI", "1");
    if (s.factor != 1.0 && !qEnvironmentVariableIsSet("QT_SCALE_FACTOR"))
        qputenv("QT_SCALE_FACTOR", QByteArray::number(s.factor));
    // Bruchteile wie 1,25 oder 1,6 genau übernehmen, nicht auf ganze Stufen runden
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
}

} // namespace vek

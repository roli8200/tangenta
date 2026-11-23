#include "BaselineGrid.h"

#include <QStringList>

#include <algorithm>
#include <cmath>

namespace vek {

namespace {

constexpr double kMinStep = 0.5; // mm

double rounded(double mm, double unit = 0.01)
{
    return std::round(mm / unit) * unit;
}

} // namespace

BaselineGrid BaselineGrid::fromJson(const QJsonObject& doc)
{
    BaselineGrid g;
    auto number = [&doc](const char* key, double fallback) {
        const QJsonValue v = doc.value(QLatin1String(key));
        return v.isDouble() ? v.toDouble() : fallback;
    };
    g.top = number("doc/grid/baseline/top", g.top);
    // Ältere Dateien: Abstand in pt
    const QJsonValue spacing = doc.value(QLatin1String("doc/grid/baseline/spacing"));
    g.step = number("doc/grid/baseline/step", spacing.isDouble() ? spacing.toDouble() * 25.4 / 72.0 : g.step);
    g.bottom = number("doc/grid/baseline/bottom", g.bottom);
    g.moved = parseMoved(doc.value(QLatin1String("doc/grid/baseline/moved")).toString());
    return g;
}

bool BaselineGrid::alignsText(const QJsonObject& doc)
{
    // Ausdrücklich gesetzt (die App setzt es beim ersten Einblenden des Rasters): gilt auch bei
    // ausgeblendetem Raster, damit das Layout beim Ausblenden nicht springt. Sonst folgt es der Anzeige.
    const QJsonValue text = doc.value(QLatin1String("doc/grid/baseline/text"));
    return text.isBool() ? text.toBool() : doc.value(QLatin1String("doc/grid/baseline/show")).toBool(false);
}

QMap<int, double> BaselineGrid::parseMoved(const QString& text)
{
    QMap<int, double> out;
    for (const QString& item : text.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        const QStringList kv = item.split(QLatin1Char('='));
        bool okIndex = false;
        bool okValue = false;
        const int index = kv.value(0).toInt(&okIndex);
        const double value = kv.value(1).toDouble(&okValue);
        if (kv.size() == 2 && okIndex && okValue && index >= 0 && value != 0.0)
            out.insert(index, value);
    }
    return out;
}

QString BaselineGrid::formatMoved(const QMap<int, double>& moved)
{
    QStringList items;
    for (auto it = moved.cbegin(); it != moved.cend(); ++it)
        if (it.value() != 0.0)
            items << QStringLiteral("%1=%2").arg(it.key()).arg(it.value(), 0, 'g', 10);
    return items.join(QLatin1Char(';'));
}

int BaselineGrid::count(double pageHeight) const
{
    const double span = pageHeight - bottom - top;
    if (step < kMinStep || span < -1e-6)
        return 0;
    return int(std::floor(span / step + 1e-6)) + 1;
}

QList<double> BaselineGrid::lines(double pageHeight) const
{
    QList<double> out;
    const int n = count(pageHeight);
    out.reserve(n);
    for (int i = 0; i < n; ++i)
        out << line(i);
    return out;
}

BaselineGrid BaselineGrid::withFirstAt(double y, double pageHeight) const
{
    BaselineGrid g = *this;
    const int n = count(pageHeight);
    if (n < 2)
        return shifted(y - line(0));
    const double last = regular(n - 1);
    y -= moved.value(0);
    g.step = rounded(std::max(kMinStep, (last - y) / (n - 1)));
    g.top = rounded(last - (n - 1) * g.step);
    g.bottom = pageHeight - g.regular(n - 1);
    return g;
}

BaselineGrid BaselineGrid::withLastAt(double y, double pageHeight) const
{
    BaselineGrid g = *this;
    const int n = count(pageHeight);
    if (n < 2)
        return shifted(y - line(0));
    y -= moved.value(n - 1);
    g.step = rounded(std::max(kMinStep, (y - top) / (n - 1)));
    // Unterster Rand genau auf die letzte Linie, damit die Anzahl Linien gleich bleibt
    g.bottom = pageHeight - g.regular(n - 1);
    return g;
}

BaselineGrid BaselineGrid::shifted(double dy) const
{
    BaselineGrid g = *this;
    dy = rounded(dy, 0.1);
    g.top = std::max(0.0, top + dy);
    g.bottom = std::max(0.0, bottom - (g.top - top));
    return g;
}

BaselineGrid BaselineGrid::withLineAt(int index, double y) const
{
    BaselineGrid g = *this;
    const double offset = rounded(y - regular(index), 0.1);
    if (std::abs(offset) < 0.05)
        g.moved.remove(index);
    else
        g.moved.insert(index, offset);
    return g;
}

} // namespace vek

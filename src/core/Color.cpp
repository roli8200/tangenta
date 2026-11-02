#include "Color.h"

namespace vek {

QString Color::toHex() const
{
    QString s = QStringLiteral("#%1%2%3")
                    .arg(r, 2, 16, QLatin1Char('0'))
                    .arg(g, 2, 16, QLatin1Char('0'))
                    .arg(b, 2, 16, QLatin1Char('0'));
    if (a != 255)
        s += QStringLiteral("%1").arg(a, 2, 16, QLatin1Char('0'));
    return s;
}

std::optional<Color> Color::fromHex(const QString& text)
{
    if (!text.startsWith(QLatin1Char('#')) || (text.size() != 7 && text.size() != 9))
        return std::nullopt;
    bool ok = true;
    auto channel = [&](int pos) {
        bool partOk = false;
        const uint v = QStringView(text).mid(pos, 2).toUInt(&partOk, 16);
        ok = ok && partOk;
        return static_cast<std::uint8_t>(v);
    };
    Color c(channel(1), channel(3), channel(5), text.size() == 9 ? channel(7) : 255);
    if (!ok)
        return std::nullopt;
    return c;
}

Color Color::fromQColor(const QColor& c)
{
    const QColor rgb = c.toRgb();
    return Color(static_cast<std::uint8_t>(rgb.red()), static_cast<std::uint8_t>(rgb.green()),
                 static_cast<std::uint8_t>(rgb.blue()), static_cast<std::uint8_t>(rgb.alpha()));
}

} // namespace vek

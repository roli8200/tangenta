#pragma once

#include <QColor>
#include <QString>

#include <cstdint>
#include <optional>

namespace vek {

// RGB-Farbe mit Deckkraft, 8 Bit je Kanal, sRGB. CMYK ist bewusst nicht vorgesehen.
struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    constexpr Color() = default;
    constexpr Color(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}

    // "#rrggbb", bei Deckkraft < 255 "#rrggbbaa"
    QString toHex() const;
    static std::optional<Color> fromHex(const QString& text);

    QColor toQColor() const { return QColor(r, g, b, a); }
    static Color fromQColor(const QColor& c);

    friend bool operator==(const Color&, const Color&) = default;
};

} // namespace vek

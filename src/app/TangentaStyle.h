#pragma once

#include <QIcon>
#include <QProxyStyle>

namespace vek {

// Programmstil über dem Systemstil. Er zeichnet einige Teile selbst:
// - den Griff verschiebbarer Symbolleisten (zwei feine Striche),
// - die Fensterknöpfe eines maximierten Dokuments rechts in der Menüleiste (flach, feine Zeichen),
// - und rückt das Dokumentsymbol links in der Menüleiste so weit ein wie die Symbole der Leisten darunter.
class TangentaStyle final : public QProxyStyle {
public:
    using QProxyStyle::QProxyStyle;

    // Abstand des Dokumentsymbols vom linken Rand der Menüleiste
    static constexpr int DocumentIconInset = 7;
    // Breite des Dokumentsymbols in der Menüleiste samt Einzug
    static constexpr int MenuBarIconWidth = 16 + DocumentIconInset;

    // Symbol für Dokumentfenster: gewöhnlich wie icon, in der Menüleiste mit Einzug
    static QIcon documentIcon(const QIcon& icon);

    int pixelMetric(PixelMetric metric, const QStyleOption* option = nullptr, const QWidget* widget = nullptr) const override;
    void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter,
                       const QWidget* widget = nullptr) const override;
    void drawComplexControl(ComplexControl control, const QStyleOptionComplex* option, QPainter* painter,
                            const QWidget* widget = nullptr) const override;
    QRect subControlRect(ComplexControl control, const QStyleOptionComplex* option, SubControl sub,
                         const QWidget* widget = nullptr) const override;
    SubControl hitTestComplexControl(ComplexControl control, const QStyleOptionComplex* option, const QPoint& pos,
                                     const QWidget* widget = nullptr) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& size,
                           const QWidget* widget = nullptr) const override;
};

} // namespace vek

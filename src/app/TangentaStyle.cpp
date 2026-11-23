#include "TangentaStyle.h"

#include <QIconEngine>
#include <QMenuBar>
#include <QPainter>
#include <QStyleOption>

namespace vek {

namespace {

constexpr int MdiButtonWidth = 28;
constexpr int MdiButtonHeight = 20;

// Liefert das Symbol unverändert, nur in der Menüleiste (angefragt mit MenuBarIconWidth) links eingerückt
class InsetIconEngine final : public QIconEngine {
public:
    explicit InsetIconEngine(QIcon icon) : m_icon(std::move(icon)) {}

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override
    {
        m_icon.paint(painter, rect, Qt::AlignCenter, mode, state);
    }
    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }
    QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        if (size.width() != TangentaStyle::MenuBarIconWidth)
            return m_icon.pixmap(size, scale, mode, state);
        const int side = TangentaStyle::MenuBarIconWidth - TangentaStyle::DocumentIconInset;
        const QPixmap icon = m_icon.pixmap(QSize(side, side), scale, mode, state);
        QPixmap out(QSize(TangentaStyle::MenuBarIconWidth, side) * scale);
        out.setDevicePixelRatio(scale);
        out.fill(Qt::transparent);
        QPainter p(&out);
        p.drawPixmap(QPointF(TangentaStyle::DocumentIconInset, 0), icon);
        return out;
    }
    QSize actualSize(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        if (size.width() == TangentaStyle::MenuBarIconWidth)
            return {TangentaStyle::MenuBarIconWidth, TangentaStyle::MenuBarIconWidth - TangentaStyle::DocumentIconInset};
        return m_icon.actualSize(size, mode, state);
    }
    QList<QSize> availableSizes(QIcon::Mode mode, QIcon::State state) override
    {
        return m_icon.availableSizes(mode, state);
    }
    QString key() const override { return QStringLiteral("vek-inset"); }
    QIconEngine* clone() const override { return new InsetIconEngine(m_icon); }

private:
    QIcon m_icon;
};

bool isMdiControls(const QWidget* widget)
{
    return widget && widget->inherits("QMdi::ControllerWidget");
}

// Reihenfolge links nach rechts: Minimieren, Wiederherstellen, Schliessen; nur die sichtbaren, rechtsbündig
QRect mdiButtonRect(const QStyleOptionComplex* option, QStyle::SubControl sub)
{
    if (!(option->subControls & sub))
        return {};
    const QStyle::SubControl order[] = {QStyle::SC_MdiMinButton, QStyle::SC_MdiNormalButton, QStyle::SC_MdiCloseButton};
    int fromRight = 0;
    bool after = false;
    for (QStyle::SubControl s : order) {
        if (s == sub)
            after = true;
        else if (after && (option->subControls & s))
            ++fromRight;
    }
    const QRect r = option->rect;
    const int h = qMin(r.height(), MdiButtonHeight);
    return {r.right() + 1 - (fromRight + 1) * MdiButtonWidth, r.top() + (r.height() - h) / 2, MdiButtonWidth, h};
}

void drawMdiButton(QPainter* p, const QRect& rect, QStyle::SubControl sub, bool hover, bool pressed, bool enabled)
{
    const bool close = sub == QStyle::SC_MdiCloseButton;
    if (hover || pressed) {
        QColor fill = close ? QColor(pressed ? 0xc4 : 0xe8, pressed ? 0x2b : 0x11, pressed ? 0x1c : 0x23)
                            : QColor(pressed ? 0xd3e0f0 : 0xe4ecf6);
        p->fillRect(rect.adjusted(1, 1, -1, -1), fill);
        if (!close) {
            p->setPen(QColor(pressed ? 0x91afd6 : 0xb8cbe2));
            p->drawRect(rect.adjusted(1, 1, -2, -2));
        }
    }
    QColor ink = !enabled ? QColor(0x9aa0a8) : (close && (hover || pressed)) ? QColor(Qt::white) : QColor(0x3a3d42);
    const qreal dpr = p->device() ? p->device()->devicePixelRatioF() : 1.0;
    // Zeichen 10x10, auf Gerätepixel ausgerichtet, damit die Striche scharf bleiben
    const qreal g = 10.0;
    const QPointF c = QRectF(rect).center();
    const qreal x0 = std::round((c.x() - g / 2) * dpr) / dpr;
    const qreal y0 = std::round((c.y() - g / 2) * dpr) / dpr;
    const qreal half = 0.5 / dpr;
    p->save();
    QPen pen(ink, 1.0);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::MiterJoin);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    switch (sub) {
    case QStyle::SC_MdiMinButton:
        p->drawLine(QPointF(x0, y0 + 5 + half), QPointF(x0 + g, y0 + 5 + half));
        break;
    case QStyle::SC_MdiNormalButton:
        // zwei versetzte Fenster: vorne 8x8 unten links, hinten oben rechts angedeutet
        p->drawRect(QRectF(x0 + half, y0 + 2 + half, 8 - 1.0 / dpr, 8 - 1.0 / dpr));
        p->drawPolyline(QPolygonF({QPointF(x0 + 2 + half, y0 + 2), QPointF(x0 + 2 + half, y0 + half),
                                   QPointF(x0 + g - half, y0 + half), QPointF(x0 + g - half, y0 + 8 - half),
                                   QPointF(x0 + 8, y0 + 8 - half)}));
        break;
    case QStyle::SC_MdiCloseButton:
        p->setRenderHint(QPainter::Antialiasing);
        pen.setWidthF(1.1);
        p->setPen(pen);
        p->drawLine(QPointF(x0 + 0.5, y0 + 0.5), QPointF(x0 + g - 0.5, y0 + g - 0.5));
        p->drawLine(QPointF(x0 + g - 0.5, y0 + 0.5), QPointF(x0 + 0.5, y0 + g - 0.5));
        break;
    default:
        break;
    }
    p->restore();
}

} // namespace

QIcon TangentaStyle::documentIcon(const QIcon& icon)
{
    return QIcon(new InsetIconEngine(icon));
}

int TangentaStyle::pixelMetric(PixelMetric metric, const QStyleOption* option, const QWidget* widget) const
{
    switch (metric) {
    case PM_ToolBarHandleExtent:
        return 9;
    case PM_TitleBarButtonIconSize:
        // Das Dokumentsymbol in der Menüleiste fragt mit der Menüleiste als Widget an
        if (qobject_cast<const QMenuBar*>(widget))
            return MenuBarIconWidth;
        break;
    case PM_TitleBarButtonSize:
        if (isMdiControls(widget))
            return MdiButtonWidth;
        break;
    default:
        break;
    }
    return QProxyStyle::pixelMetric(metric, option, widget);
}

void TangentaStyle::drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter,
                                  const QWidget* widget) const
{
    if (element == PE_IndicatorToolBarHandle) {
        // Griff: zwei feine Striche quer zur Leiste
        const bool horizontal = option->state & State_Horizontal;
        const QRect r = option->rect;
        painter->save();
        painter->setPen(QPen(QColor(0x9aa0a8), 1.0));
        const int inset = 4;
        if (horizontal) {
            const int cx = r.center().x();
            for (int x : {cx - 1, cx + 2})
                painter->drawLine(x, r.top() + inset, x, r.bottom() - inset);
        } else {
            const int cy = r.center().y();
            for (int y : {cy - 1, cy + 2})
                painter->drawLine(r.left() + inset, y, r.right() - inset, y);
        }
        painter->restore();
        return;
    }
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

void TangentaStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex* option, QPainter* painter,
                                       const QWidget* widget) const
{
    if (control != CC_MdiControls) {
        QProxyStyle::drawComplexControl(control, option, painter, widget);
        return;
    }
    const bool enabled = option->state & State_Enabled;
    for (SubControl sub : {SC_MdiMinButton, SC_MdiNormalButton, SC_MdiCloseButton}) {
        const QRect r = mdiButtonRect(option, sub);
        if (r.isNull())
            continue;
        const bool active = option->activeSubControls & sub;
        drawMdiButton(painter, r, sub, active && (option->state & State_MouseOver), active && (option->state & State_Sunken),
                      enabled);
    }
}

QRect TangentaStyle::subControlRect(ComplexControl control, const QStyleOptionComplex* option, SubControl sub,
                                    const QWidget* widget) const
{
    if (control == CC_MdiControls)
        return mdiButtonRect(option, sub);
    return QProxyStyle::subControlRect(control, option, sub, widget);
}

QStyle::SubControl TangentaStyle::hitTestComplexControl(ComplexControl control, const QStyleOptionComplex* option,
                                                        const QPoint& pos, const QWidget* widget) const
{
    if (control == CC_MdiControls) {
        for (SubControl sub : {SC_MdiMinButton, SC_MdiNormalButton, SC_MdiCloseButton})
            if (mdiButtonRect(option, sub).contains(pos))
                return sub;
        return SC_None;
    }
    return QProxyStyle::hitTestComplexControl(control, option, pos, widget);
}

QSize TangentaStyle::sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& size,
                                      const QWidget* widget) const
{
    if (type == CT_MdiControls) {
        int count = 0;
        if (const auto* complex = qstyleoption_cast<const QStyleOptionComplex*>(option)) {
            for (SubControl sub : {SC_MdiMinButton, SC_MdiNormalButton, SC_MdiCloseButton})
                if (complex->subControls & sub)
                    ++count;
        } else {
            count = 3;
        }
        return {count * MdiButtonWidth + 2, MdiButtonHeight};
    }
    return QProxyStyle::sizeFromContents(type, option, size, widget);
}

} // namespace vek

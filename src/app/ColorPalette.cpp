#include "ColorPalette.h"

#include "Palette.h"

#include <QFrame>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QSettings>
#include <QToolTip>
#include <QWheelEvent>

#include <cmath>

namespace vek {

namespace {

constexpr int kButton = 14; // Höhe von Aufklappknopf und Rollpfeilen
constexpr int kFrame = 2;

const QColor kGrid(96, 96, 96);

void paintTriangle(QPainter& p, const QRect& r, Qt::ArrowType dir, const QColor& color)
{
    const QPointF c = QRectF(r).center();
    QPolygonF t;
    switch (dir) {
    case Qt::UpArrow: t = {c + QPointF(-4, 2), c + QPointF(4, 2), c + QPointF(0, -2)}; break;
    case Qt::DownArrow: t = {c + QPointF(-4, -2), c + QPointF(4, -2), c + QPointF(0, 2)}; break;
    default: t = {c + QPointF(2, -4), c + QPointF(2, 4), c + QPointF(-2, 0)}; break;
    }
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawPolygon(t);
    p.restore();
}

// Aufgeklappte Farbleiste: alle Farben in Spalten, von oben nach unten gefüllt
class PalettePopup : public QFrame {
public:
    PalettePopup(ColorPalette& palette, int rows)
        : QFrame(nullptr, Qt::Popup), m_palette(palette), m_rows(std::max(1, rows))
    {
        setAttribute(Qt::WA_DeleteOnClose);
        setFrameShape(QFrame::Box);
        setMouseTracking(true);
        m_columns = (m_palette.colorCount() + m_rows - 1) / m_rows;
        setFixedSize(2 * kFrame + m_columns * ColorPalette::Cell + 1, 2 * kFrame + m_rows * ColorPalette::Cell + 1);
    }

protected:
    QRect cell(int index) const
    {
        const int col = index / m_rows;
        const int row = index % m_rows;
        return QRect(kFrame + col * ColorPalette::Cell, kFrame + row * ColorPalette::Cell, ColorPalette::Cell,
                     ColorPalette::Cell);
    }
    int at(const QPoint& pos) const
    {
        for (int i = 0; i < m_palette.colorCount(); ++i) {
            if (cell(i).contains(pos))
                return i;
        }
        return -1;
    }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), palette().color(QPalette::Window));
        for (int i = 0; i < m_palette.colorCount(); ++i)
            paintSwatch(p, cell(i), m_palette.colorAt(i), i == m_hover);
        p.setPen(palette().color(QPalette::Highlight));
        p.drawRect(rect().adjusted(0, 0, -1, -1));
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        const int i = at(e->position().toPoint());
        if (i != m_hover) {
            m_hover = i;
            update();
        }
    }
    void mousePressEvent(QMouseEvent* e) override
    {
        const int i = at(e->position().toPoint());
        if (i >= 0)
            m_palette.choose(i, e->button());
        close();
    }
    bool event(QEvent* e) override
    {
        if (e->type() == QEvent::ToolTip) {
            auto* help = static_cast<QHelpEvent*>(e);
            const int i = at(help->pos());
            if (i >= 0)
                QToolTip::showText(help->globalPos(), m_palette.tooltipFor(i), this);
            return true;
        }
        return QFrame::event(e);
    }

private:
    ColorPalette& m_palette;
    int m_rows;
    int m_columns = 1;
    int m_hover = -1;
};

} // namespace

void paintSwatch(QPainter& p, const QRect& r, const std::optional<Color>& c, bool hover)
{
    const QRect inner = r.adjusted(1, 1, 0, 0);
    if (c) {
        p.fillRect(inner, c->toQColor());
    } else {
        p.fillRect(inner, Qt::white);
        p.save();
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(Qt::black, 1.1));
        p.drawLine(QPointF(inner.left() + 2.5, inner.top() + 2.5), QPointF(inner.right() - 1.5, inner.bottom() - 1.5));
        p.drawLine(QPointF(inner.right() - 1.5, inner.top() + 2.5), QPointF(inner.left() + 2.5, inner.bottom() - 1.5));
        p.restore();
    }
    p.setPen(kGrid);
    p.setBrush(Qt::NoBrush);
    p.drawRect(r);
    if (hover) {
        p.setPen(QPen(Qt::white, 1, Qt::DotLine));
        p.drawRect(r.adjusted(1, 1, -1, -1));
    }
}

namespace {

QList<ColorPalette*>& instances()
{
    static QList<ColorPalette*> list;
    return list;
}

} // namespace

ColorPalette::ColorPalette(QWidget* parent)
    : QWidget(parent)
{
    reload();
    instances() << this;
    setMouseTracking(true);
    setFixedWidth(Cell + 2 * kFrame + 1);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

ColorPalette::~ColorPalette()
{
    instances().removeAll(this);
}

void ColorPalette::reload()
{
    m_colors.clear();
    m_colors.push_back(std::nullopt);
    for (const NamedColor& n : defaultPalette())
        m_colors.push_back(n.color);
    for (const Color& c : customColors())
        m_colors.push_back(c);
    updateGeometry();
    update();
}

QList<Color> ColorPalette::customColors()
{
    QList<Color> out;
    for (const QString& hex : QSettings().value(QStringLiteral("palette/custom")).toStringList())
        if (const std::optional<Color> c = Color::fromHex(hex))
            out << *c;
    return out;
}

void ColorPalette::addCustomColor(const Color& color)
{
    QStringList list = QSettings().value(QStringLiteral("palette/custom")).toStringList();
    const QString hex = color.toHex();
    if (list.contains(hex, Qt::CaseInsensitive))
        return;
    for (const NamedColor& n : defaultPalette())
        if (n.color == color)
            return; // schon in der Standardpalette
    list << hex;
    QSettings().setValue(QStringLiteral("palette/custom"), list);
    for (ColorPalette* p : std::as_const(instances()))
        p->reload();
}

QSize ColorPalette::sizeHint() const
{
    return QSize(width(), kButton + colorCount() * Cell + 2 * kFrame);
}

QSize ColorPalette::minimumSizeHint() const
{
    return QSize(width(), 3 * kButton + 4 * Cell);
}

QRect ColorPalette::expandButton() const
{
    return QRect(kFrame, kFrame, Cell + 1, kButton);
}

bool ColorPalette::overflows() const
{
    return height() < kFrame * 2 + kButton + colorCount() * Cell + 1;
}

QRect ColorPalette::upArrow() const
{
    return QRect(kFrame, expandButton().bottom() + 1, Cell + 1, kButton);
}

QRect ColorPalette::downArrow() const
{
    return QRect(kFrame, height() - kFrame - kButton, Cell + 1, kButton);
}

QRect ColorPalette::swatchArea() const
{
    const int top = overflows() ? upArrow().bottom() + 1 : expandButton().bottom() + 1;
    const int bottom = overflows() ? downArrow().top() : height() - kFrame;
    return QRect(kFrame, top, Cell + 1, bottom - top);
}

int ColorPalette::visibleCount() const
{
    return std::max(1, (swatchArea().height() - 1) / Cell);
}

int ColorPalette::swatchAt(const QPoint& pos) const
{
    const QRect area = swatchArea();
    if (!area.contains(pos))
        return -1;
    const int i = m_first + (pos.y() - area.top()) / Cell;
    return i < colorCount() && i - m_first < visibleCount() ? i : -1;
}

void ColorPalette::scrollBy(int cells)
{
    const int maxFirst = std::max(0, colorCount() - visibleCount());
    const int first = std::clamp(m_first + cells, 0, maxFirst);
    if (first != m_first) {
        m_first = first;
        update();
    }
}

void ColorPalette::resizeEvent(QResizeEvent*)
{
    scrollBy(0);
}

void ColorPalette::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), palette().color(QPalette::Window));
    const QColor ink = palette().color(QPalette::WindowText);
    const QColor off = palette().color(QPalette::Disabled, QPalette::WindowText);

    p.fillRect(expandButton(), palette().color(QPalette::Button));
    p.setPen(palette().color(QPalette::Mid));
    p.drawRect(expandButton().adjusted(0, 0, -1, -1));
    paintTriangle(p, expandButton(), Qt::LeftArrow, ink);

    const QRect area = swatchArea();
    const int count = std::min(visibleCount(), colorCount() - m_first);
    for (int i = 0; i < count; ++i) {
        const int index = m_first + i;
        paintSwatch(p, QRect(area.left(), area.top() + i * Cell, Cell, Cell), m_colors[static_cast<std::size_t>(index)],
                    index == m_hover);
    }
    if (overflows()) {
        paintTriangle(p, upArrow(), Qt::UpArrow, m_first > 0 ? ink : off);
        paintTriangle(p, downArrow(), Qt::DownArrow, m_first + visibleCount() < colorCount() ? ink : off);
    }
}

void ColorPalette::expand()
{
    // So viele Zeilen wie die Leiste hoch ist, nach links aufgeklappt
    const int rows = std::max(4, (height() - 2 * kFrame) / Cell);
    auto* popup = new PalettePopup(*this, rows);
    const QPoint topRight = mapToGlobal(QPoint(width(), 0));
    popup->move(topRight.x() - popup->width(), topRight.y());
    popup->show();
}

void ColorPalette::choose(int index, Qt::MouseButton button)
{
    const auto& c = m_colors[static_cast<std::size_t>(index)];
    if (button == Qt::LeftButton)
        emit fillChosen(c);
    else if (button == Qt::RightButton)
        emit strokeChosen(c);
}

void ColorPalette::mousePressEvent(QMouseEvent* e)
{
    const QPoint pos = e->position().toPoint();
    if (expandButton().contains(pos)) {
        expand();
        return;
    }
    if (overflows() && upArrow().contains(pos)) {
        scrollBy(-1);
        return;
    }
    if (overflows() && downArrow().contains(pos)) {
        scrollBy(1);
        return;
    }
    const int i = swatchAt(pos);
    if (i >= 0)
        choose(i, e->button());
}

void ColorPalette::mouseMoveEvent(QMouseEvent* e)
{
    const int i = swatchAt(e->position().toPoint());
    if (i != m_hover) {
        m_hover = i;
        update();
    }
}

void ColorPalette::wheelEvent(QWheelEvent* e)
{
    scrollBy(e->angleDelta().y() > 0 ? -3 : 3);
    e->accept();
}

void ColorPalette::leaveEvent(QEvent*)
{
    m_hover = -1;
    update();
}

QString ColorPalette::tooltipFor(int index) const
{
    const auto& c = m_colors[static_cast<std::size_t>(index)];
    if (!c)
        return tr("Keine\nLinksklick: Füllung, Rechtsklick: Kontur");
    const QString name = colorName(*c);
    return tr("%1R:%2 G:%3 B:%4 (%5)\nLinksklick: Füllung, Rechtsklick: Kontur")
        .arg(name.isEmpty() ? QString() : name + QLatin1Char('\n'))
        .arg(c->r)
        .arg(c->g)
        .arg(c->b)
        .arg(c->toHex().toUpper());
}

bool ColorPalette::event(QEvent* e)
{
    if (e->type() == QEvent::ToolTip) {
        auto* help = static_cast<QHelpEvent*>(e);
        if (expandButton().contains(help->pos())) {
            QToolTip::showText(help->globalPos(), tr("Farbleiste aufklappen"), this);
        } else if (const int i = swatchAt(help->pos()); i >= 0) {
            QToolTip::showText(help->globalPos(), tooltipFor(i), this);
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    return QWidget::event(e);
}

} // namespace vek

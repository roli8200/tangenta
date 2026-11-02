#include "Ruler.h"

#include "Canvas.h"
#include "Options.h"
#include "TextTool.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace vek {

namespace {

QColor rulerBackground(const QWidget* w)
{
    return w->palette().color(QPalette::Window).lighter(103);
}

QColor rulerInk(const QWidget* w)
{
    return w->palette().color(QPalette::WindowText);
}

} // namespace

Ruler::Ruler(Qt::Orientation orientation, Canvas& canvas, QWidget* parent)
    : QWidget(parent), m_orientation(orientation), m_canvas(canvas)
{
    if (orientation == Qt::Horizontal)
        setFixedHeight(Thickness);
    else
        setFixedWidth(Thickness);
    setCursor(orientation == Qt::Horizontal ? Qt::SizeVerCursor : Qt::SizeHorCursor);
    setToolTip(orientation == Qt::Horizontal ? tr("Ziehen für eine waagrechte Hilfslinie")
                                             : tr("Ziehen für eine senkrechte Hilfslinie"));
    connect(&canvas, &Canvas::viewChanged, this, qOverload<>(&QWidget::update));
    connect(&canvas, &Canvas::toolStateChanged, this, qOverload<>(&QWidget::update));
    connect(&canvas, &Canvas::cursorMoved, this, [this](const QPointF& doc) {
        m_cursor = m_orientation == Qt::Horizontal ? doc.x() : doc.y();
        update();
    });
}

QSize Ruler::sizeHint() const
{
    return m_orientation == Qt::Horizontal ? QSize(200, Thickness) : QSize(Thickness, 200);
}

double Ruler::toDoc(double px) const
{
    const QPointF doc = m_canvas.toDoc(QPointF(px, px));
    return m_orientation == Qt::Horizontal ? doc.x() : doc.y();
}

double Ruler::toPx(double mm) const
{
    const QPointF s = m_canvas.toScreen(QPointF(mm, mm));
    return m_orientation == Qt::Horizontal ? s.x() : s.y();
}

void Ruler::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), rulerBackground(this));
    const QColor ink = rulerInk(this);
    p.setPen(ink);
    const bool horizontal = m_orientation == Qt::Horizontal;
    const int length = horizontal ? width() : height();

    // Werte in der Linealeinheit ab dem Ursprung, mit Zeichnungsmassstab (Optionen > Lineale).
    // Beschriftung höchstens alle ~60 px.
    const Options& opt = Options::instance();
    const double u0 = opt.rulerValue(0.0, horizontal);
    const double perMm = opt.rulerValue(1.0, horizontal) - u0; // Linealeinheiten je mm
    auto unitToMm = [&](double value) { return (value - u0) / perMm; };
    const double pxPerUnit = m_canvas.zoom() / std::abs(perMm);
    double major = 1.0;
    for (double decade = 1e-4; decade <= 1e7; decade *= 10.0) {
        bool found = false;
        for (double m : {1.0, 2.0, 5.0}) {
            major = m * decade;
            if (major * pxPerUnit >= 60.0) {
                found = true;
                break;
            }
        }
        if (found)
            break;
    }
    const bool more = opt.flag(QStringLiteral("doc/rulers/moreTicks"));
    const int wanted = std::clamp(opt.integer(QStringLiteral("doc/rulers/ticks")), 1, 100);
    int subdivisions = std::abs(std::log10(major) - std::round(std::log10(major))) < 1e-9 ? wanted : (major / std::pow(10.0, std::floor(std::log10(major))) < 3 ? 4 : 5);
    if (!more)
        subdivisions = std::min(subdivisions, 2);
    while (subdivisions > 1 && major / subdivisions * pxPerUnit < 4.0)
        subdivisions /= 2;
    const double minor = major / std::max(1, subdivisions);
    auto toDocUnits = [&](double px) { return opt.rulerValue(toDoc(px), horizontal); };

    QFont f = font();
    f.setPointSizeF(std::max(6.5, f.pointSizeF() * 0.8));
    p.setFont(f);

    const double a = toDocUnits(0);
    const double b = toDocUnits(length);
    const double first = std::floor(std::min(a, b) / minor) * minor;
    const double last = std::max(a, b);
    for (int i = 0; i < 100000; ++i) {
        const double value = first + i * minor;
        if (value > last)
            break;
        const double mm = unitToMm(value);
        const double px = std::round(toPx(mm)) + 0.5;
        const long index = std::lround(value / minor);
        const bool isMajor = index % std::max(1, subdivisions) == 0;
        const bool isHalf = !isMajor && subdivisions % 2 == 0 && index % (subdivisions / 2) == 0;
        const double tick = isMajor ? Thickness - 2 : isHalf ? 7 : 4;
        if (horizontal)
            p.drawLine(QPointF(px, Thickness), QPointF(px, Thickness - tick));
        else
            p.drawLine(QPointF(Thickness, px), QPointF(Thickness - tick, px));
        if (isMajor) {
            const QString label = QString::number(value, 'g', 6);
            if (horizontal) {
                p.drawText(QPointF(px + 2, 10), label);
            } else {
                p.save();
                p.translate(10, px - 2);
                p.rotate(-90);
                p.drawText(QPointF(0, 0), label);
                p.restore();
            }
        }
    }

    // Mengentext bearbeiten: Rahmen hell, Tabulatoren als kleine Winkel
    double frameLeft = 0.0;
    double frameRight = 0.0;
    QList<double> tabs;
    if (textFrame(&frameLeft, &frameRight, &tabs)) {
        const double x1 = toPx(frameLeft);
        const double x2 = toPx(frameRight);
        p.fillRect(QRectF(QPointF(x1, Thickness - 7), QPointF(x2, Thickness - 1)), QColor(255, 255, 255, 200));
        p.setPen(QPen(QColor(0x1d, 0x1d, 0x1d), 1.5));
        for (double t : tabs) {
            const double x = std::round(toPx(frameLeft + t)) + 0.5;
            p.drawLine(QPointF(x, Thickness - 9), QPointF(x, Thickness - 2));
            p.drawLine(QPointF(x, Thickness - 2), QPointF(x + 5, Thickness - 2));
        }
    }

    // Grundlinie zur Zeichenfläche
    p.setPen(palette().color(QPalette::Mid));
    if (horizontal)
        p.drawLine(0, Thickness - 1, width(), Thickness - 1);
    else
        p.drawLine(Thickness - 1, 0, Thickness - 1, height());

    // Mausposition
    if (!std::isnan(m_cursor)) {
        p.setPen(QPen(ink, 1, Qt::DotLine));
        const double px = std::round(toPx(m_cursor)) + 0.5;
        if (horizontal)
            p.drawLine(QPointF(px, 0), QPointF(px, Thickness));
        else
            p.drawLine(QPointF(0, px), QPointF(Thickness, px));
    }
}

Guide Ruler::makeGuide(double position) const
{
    // Ankerpunkt auf der Seitenmitte
    const QSizeF page = m_canvas.editor().currentPage().size();
    return m_orientation == Qt::Horizontal ? Guide::horizontal(position, page.width() / 2.0)
                                           : Guide::vertical(position, page.height() / 2.0);
}

bool Ruler::guidePosition(const QPoint& globalPos, double* position) const
{
    const QPoint local = m_canvas.mapFromGlobal(globalPos);
    if (!m_canvas.rect().contains(local))
        return false;
    const QPointF doc = m_canvas.toDoc(local);
    *position = m_orientation == Qt::Horizontal ? doc.y() : doc.x();
    return true;
}

bool Ruler::textFrame(double* left, double* right, QList<double>* tabs) const
{
    return m_orientation == Qt::Horizontal && m_canvas.toolId() == ToolId::Text
           && m_canvas.textTool().rulerFrame(left, right, tabs);
}

int Ruler::tabAt(double px, const QList<double>& tabs, double frameLeft) const
{
    for (int i = 0; i < tabs.size(); ++i) {
        if (std::abs(toPx(frameLeft + tabs[i]) - px) <= 5.0)
            return i;
    }
    return -1;
}

void Ruler::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton)
        return;
    // Klick ins Lineal über dem Textrahmen: Tabulator setzen oder einen vorhandenen greifen
    double left = 0.0;
    double right = 0.0;
    if (textFrame(&left, &right, &m_tabs)) {
        const double x = e->position().x();
        if (x >= toPx(left) && x <= toPx(right)) {
            m_draggedTab = tabAt(x, m_tabs, left);
            if (m_draggedTab < 0) {
                m_tabs.append(toDoc(x) - left);
                m_draggedTab = int(m_tabs.size()) - 1;
                m_canvas.textTool().setTabs(m_tabs);
            }
            return;
        }
    }
    m_draggingGuide = true;
}

void Ruler::mouseMoveEvent(QMouseEvent* e)
{
    if (m_draggedTab >= 0) {
        double left = 0.0;
        double right = 0.0;
        QList<double> current;
        if (!textFrame(&left, &right, &current))
            return;
        // Aus dem Lineal nach unten gezogen: Tabulator entfernen
        QList<double> tabs = m_tabs;
        if (e->position().y() > Thickness + 20)
            tabs.removeAt(m_draggedTab);
        else
            tabs[m_draggedTab] = std::clamp(toDoc(e->position().x()), left, right) - left;
        m_canvas.textTool().setTabs(tabs);
        return;
    }
    if (!m_draggingGuide)
        return;
    double pos = 0.0;
    if (guidePosition(e->globalPosition().toPoint(), &pos))
        m_canvas.setGuidePreview(makeGuide(pos));
    else
        m_canvas.setGuidePreview(std::nullopt);
}

void Ruler::mouseReleaseEvent(QMouseEvent* e)
{
    if (m_draggedTab >= 0 && e->button() == Qt::LeftButton) {
        m_draggedTab = -1;
        m_canvas.setFocus(); // gleich weitertippen
        return;
    }
    if (!m_draggingGuide || e->button() != Qt::LeftButton)
        return;
    m_draggingGuide = false;
    m_canvas.setGuidePreview(std::nullopt);
    double pos = 0.0;
    if (guidePosition(e->globalPosition().toPoint(), &pos)) {
        m_canvas.editor().addGuide(makeGuide(pos));
        m_canvas.setGuidesVisible(true);
    }
}

void RulerCorner::mouseDoubleClickEvent(QMouseEvent*)
{
    // Doppelklick auf die Ecke öffnet die Lineal-Optionen
    emit optionsRequested();
}

void RulerCorner::mousePressEvent(QMouseEvent* e)
{
    m_dragging = e->button() == Qt::LeftButton;
}

void RulerCorner::mouseReleaseEvent(QMouseEvent* e)
{
    // Aus der Ecke auf die Zeichenfläche gezogen: dort liegt der neue Nullpunkt der Lineale
    if (!m_dragging || !m_canvas)
        return;
    m_dragging = false;
    const QPoint inCanvas = m_canvas->mapFromGlobal(e->globalPosition().toPoint());
    if (!m_canvas->rect().contains(inCanvas))
        return;
    const QPointF doc = m_canvas->toDoc(QPointF(inCanvas));
    Options::instance().apply({{QStringLiteral("doc/rulers/originX"), doc.x()}, {QStringLiteral("doc/rulers/originY"), doc.y()}});
}

void RulerCorner::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), rulerBackground(this));
    p.setPen(palette().color(QPalette::Mid));
    p.drawLine(width() - 1, 0, width() - 1, height());
    p.drawLine(0, height() - 1, width(), height() - 1);
}

} // namespace vek

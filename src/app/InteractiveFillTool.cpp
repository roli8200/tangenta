#include "InteractiveFillTool.h"

#include "Canvas.h"
#include "GradientSlider.h"
#include "Icons.h"
#include "Options.h"

#include <QtMath>

#include <cmath>

namespace vek {

namespace {

constexpr double kHandle = 5.0; // halbe Kantenlänge der Endpunkt-Quadrate, px

} // namespace

void InteractiveFillTool::activate()
{
    m_canvas.setCursor(QCursor(icon(QStringLiteral("tool-interactive-fill")).pixmap(24), 3, 20));
}

void InteractiveFillTool::deactivate()
{
    m_drag = Drag::None;
    m_preview.reset();
    m_selectedEnd = -1;
}

const DrawObject* InteractiveFillTool::target() const
{
    const QList<const DrawObject*> objects = m_canvas.editor().selectedObjects();
    return objects.isEmpty() ? nullptr : objects.first();
}

QPointF InteractiveFillTool::toUnit(const DrawObject& o, const QPointF& doc) const
{
    const QRectF b = o.localPath().boundingRect();
    const QPointF local = o.transform().inverted().map(doc);
    return QPointF(b.width() != 0 ? (local.x() - b.left()) / b.width() : 0.5,
                   b.height() != 0 ? (local.y() - b.top()) / b.height() : 0.5);
}

QPointF InteractiveFillTool::fromUnit(const DrawObject& o, const QPointF& unit) const
{
    const QRectF b = o.localPath().boundingRect();
    return o.transform().map(QPointF(b.left() + unit.x() * b.width(), b.top() + unit.y() * b.height()));
}

QPointF InteractiveFillTool::constrained(const QPointF& from, const QPointF& to, Qt::KeyboardModifiers mods) const
{
    if (!(mods & Qt::ControlModifier))
        return to;
    const QPointF v = to - from;
    const double len = std::hypot(v.x(), v.y());
    const double a = std::round(std::atan2(v.y(), v.x()) / qDegreesToRadians(Options::instance().number(QStringLiteral("ws/edit/angleStep")))) * qDegreesToRadians(Options::instance().number(QStringLiteral("ws/edit/angleStep")));
    return from + QPointF(std::cos(a), std::sin(a)) * len;
}

namespace {

Color mix(const Color& a, const Color& b, double t)
{
    auto m = [t](std::uint8_t x, std::uint8_t y) { return static_cast<std::uint8_t>(std::lround(x + (y - x) * t)); };
    return Color(m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a));
}

} // namespace

std::optional<double> InteractiveFillTool::midpoint(const Fill& f)
{
    if (!f.isGradient())
        return std::nullopt;
    if (f.stops.size() == 2)
        return 0.5;
    if (f.stops.size() == 3 && f.stops[1].color == mix(f.stops.first().color, f.stops.last().color, 0.5))
        return f.stops[1].position;
    return std::nullopt;
}

void InteractiveFillTool::setMidpoint(Fill& f, double position)
{
    if (!midpoint(f))
        return;
    position = std::clamp(position, 0.01, 0.99);
    if (f.stops.size() == 2)
        f.stops.insert(1, GradientStop{position, mix(f.stops.first().color, f.stops.last().color, 0.5)});
    else
        f.stops[1].position = position;
}

Fill InteractiveFillTool::baseFill(const DrawObject& o) const
{
    const Fill& current = o.style().fill;
    if (current.isGradient())
        return current;
    // Von der bisherigen Füllfarbe (sonst Schwarz) nach Weiss
    return Fill::gradient(Fill::Type::Linear, current.representativeColor().value_or(Color(0, 0, 0)),
                          Color(255, 255, 255));
}

void InteractiveFillTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    Editor& ed = m_canvas.editor();
    m_pressDoc = doc;

    if (const DrawObject* o = target(); o && o->style().fill.isGradient()) {
        const Fill& f = o->style().fill;
        const QPointF s = m_canvas.toScreen(fromUnit(*o, f.start));
        const QPointF e = m_canvas.toScreen(fromUnit(*o, f.end));
        const QPointF pos = event->position();
        auto near = [&pos](const QPointF& h) { return std::abs(pos.x() - h.x()) <= kHandle + 2 && std::abs(pos.y() - h.y()) <= kHandle + 2; };
        if (near(e) || near(s)) {
            m_drag = near(e) ? Drag::End : Drag::Start;
            m_selectedEnd = m_drag == Drag::End ? 1 : 0;
            m_target = o->id();
            m_preview = f;
            m_canvas.update();
            return;
        }
        // Mittelpunkt-Regler (nur bei zwei Farben)
        if (const std::optional<double> mid = midpoint(f); mid && slider::hit(pos, s, e, *mid)) {
            m_drag = Drag::Mid;
            m_target = o->id();
            m_preview = f;
            m_canvas.update();
            return;
        }
    }

    const DrawObject* hit = ed.objectAt(doc, m_canvas.pixelsToDoc(4.0));
    if (!hit) {
        m_selectedEnd = -1;
        m_canvas.update();
        return;
    }
    if (!ed.isSelected(hit->id()))
        ed.setSelection({hit->id()});
    m_target = hit->id();
    m_drag = Drag::Create;
    m_selectedEnd = -1;
    m_preview.reset();
}

void InteractiveFillTool::mouseMove(QMouseEvent* event, const QPointF& doc)
{
    if (m_drag == Drag::None)
        return;
    const DrawObject* o = m_canvas.editor().document().findAs<DrawObject>(m_target);
    if (!o)
        return;
    Fill f = m_preview ? *m_preview : baseFill(*o);
    switch (m_drag) {
    case Drag::Create: {
        const QPointF d = m_canvas.toScreen(doc) - m_canvas.toScreen(m_pressDoc);
        if (std::abs(d.x()) + std::abs(d.y()) < 4.0)
            return;
        f = baseFill(*o);
        f.start = toUnit(*o, m_pressDoc);
        f.end = toUnit(*o, constrained(m_pressDoc, doc, event->modifiers()));
        break;
    }
    case Drag::Start: {
        const QPointF other = fromUnit(*o, f.end);
        f.start = toUnit(*o, constrained(other, doc, event->modifiers()));
        break;
    }
    case Drag::End: {
        const QPointF other = fromUnit(*o, f.start);
        f.end = toUnit(*o, constrained(other, doc, event->modifiers()));
        break;
    }
    case Drag::Mid:
        setMidpoint(f, slider::fraction(m_canvas.toScreen(doc), m_canvas.toScreen(fromUnit(*o, f.start)),
                                        m_canvas.toScreen(fromUnit(*o, f.end)), 0.01, 0.99));
        break;
    case Drag::None:
        break;
    }
    m_preview = f;
    m_canvas.update();
}

void InteractiveFillTool::mouseRelease(QMouseEvent* event, const QPointF&)
{
    if (event->button() != Qt::LeftButton)
        return;
    if (m_drag != Drag::None && m_preview)
        m_canvas.editor().setObjectFill(m_target, *m_preview, QObject::tr("Interaktive Füllung"));
    m_drag = Drag::None;
    m_preview.reset();
    m_canvas.update();
}

bool InteractiveFillTool::keyPress(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        if (m_drag != Drag::None) {
            m_drag = Drag::None;
            m_preview.reset();
        } else {
            m_selectedEnd = -1;
        }
        m_canvas.update();
        return true;
    }
    return false;
}

bool InteractiveFillTool::paletteColor(const std::optional<Color>& color, bool fill)
{
    const DrawObject* o = target();
    if (!fill || !color || m_selectedEnd < 0 || !o || !o->style().fill.isGradient())
        return false;
    Fill f = o->style().fill;
    GradientStop& stop = m_selectedEnd == 0 ? f.stops.first() : f.stops.last();
    stop.color = *color;
    m_canvas.editor().setObjectFill(o->id(), f, QObject::tr("Verlaufsfarbe"));
    return true;
}

void InteractiveFillTool::paintOverlay(QPainter& p)
{
    const DrawObject* o = m_drag != Drag::None ? m_canvas.editor().document().findAs<DrawObject>(m_target) : target();
    if (!o)
        return;
    const Fill f = m_preview ? *m_preview : o->style().fill;
    p.save();
    if (m_preview) {
        // Vorschau des neuen Verlaufs direkt im Objekt
        QBrush brush = f.brush(o->localPath().boundingRect());
        brush.setTransform(brush.transform() * o->transform() * m_canvas.viewTransform());
        p.fillPath(m_canvas.viewTransform().map(o->path()), brush);
    }
    // Umriss der Auswahl
    p.setPen(QPen(Canvas::previewColor(), 1, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawPath(m_canvas.viewTransform().map(o->path()));

    if (f.isGradient()) {
        const QPointF s = m_canvas.toScreen(fromUnit(*o, f.start));
        const QPointF e = m_canvas.toScreen(fromUnit(*o, f.end));
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(Qt::white, 3));
        p.drawLine(s, e);
        p.setPen(QPen(Qt::black, 1, Qt::DashLine));
        p.drawLine(s, e);
        // Zwei Farben: Mittelpunkt-Regler; sonst Zwischenstopps als kleine Quadrate (nur linear)
        if (const std::optional<double> mid = midpoint(f)) {
            slider::paint(p, s, e, *mid, m_drag == Drag::Mid ? QObject::tr("Mittelpunkt %1 %").arg(std::lround(*mid * 100)) : QString());
        } else if (f.type == Fill::Type::Linear) {
            for (int i = 1; i + 1 < f.stops.size(); ++i) {
                const QPointF m = s + (e - s) * f.stops[i].position;
                p.setPen(QPen(Qt::black, 1));
                p.setBrush(f.stops[i].color.toQColor());
                p.drawRect(QRectF(m.x() - 3, m.y() - 3, 6, 6));
            }
        }
        // Endpunkte: Farbfeld mit weissem und dunklem Rand, damit sie auf jedem Hintergrund sichtbar sind
        const QPointF ends[2] = {s, e};
        p.setRenderHint(QPainter::Antialiasing, false);
        for (int i = 0; i < 2; ++i) {
            const QRectF r(std::round(ends[i].x()) - kHandle, std::round(ends[i].y()) - kHandle, 2 * kHandle, 2 * kHandle);
            p.setPen(Qt::NoPen);
            p.setBrush(i == m_selectedEnd ? QColor(0xe0, 0x20, 0x20) : QColor(Qt::black));
            p.drawRect(r.adjusted(-2, -2, 2, 2));
            p.setBrush(Qt::white);
            p.drawRect(r.adjusted(-1, -1, 1, 1));
            p.setBrush((i == 0 ? f.stops.first() : f.stops.last()).color.toQColor());
            p.drawRect(r);
        }
    }
    p.restore();
}

} // namespace vek

#include "EffectTool.h"

#include "Canvas.h"
#include "GradientSlider.h"
#include "Icons.h"
#include "Options.h"

#include <QLineF>
#include <QtMath>

#include <cmath>

namespace vek {

namespace {

constexpr double kHandle = 5.0; // halbe Kantenlänge der Endpunkt-Quadrate, px

QPointF constrained(const QPointF& from, const QPointF& to, Qt::KeyboardModifiers mods)
{
    if (!(mods & Qt::ControlModifier))
        return to;
    const QPointF v = to - from;
    const double len = std::hypot(v.x(), v.y());
    const double a = std::round(std::atan2(v.y(), v.x()) / qDegreesToRadians(Options::instance().number(QStringLiteral("ws/edit/angleStep")))) * qDegreesToRadians(Options::instance().number(QStringLiteral("ws/edit/angleStep")));
    return from + QPointF(std::cos(a), std::sin(a)) * len;
}

} // namespace

EffectTool::EffectTool(Canvas& canvas, Effect effect) : Tool(canvas), m_effect(effect) {}

void EffectTool::activate()
{
    const QString name = m_effect == Effect::Blur ? QStringLiteral("tool-blur") : QStringLiteral("tool-transparency");
    m_canvas.setCursor(QCursor(icon(name).pixmap(24), 3, 20));
}

void EffectTool::deactivate()
{
    m_drag = Drag::None;
    m_preview.reset();
    m_selectedEnd = -1;
    m_canvas.clearEffectPreview();
}

QString EffectTool::label() const
{
    return m_effect == Effect::Blur ? QObject::tr("Interaktive Unschärfe") : QObject::tr("Interaktive Transparenz");
}

Ramp EffectTool::defaultRamp(Effect effect, Ramp::Type type, const Ramp& current)
{
    const bool blur = effect == Effect::Blur;
    Ramp r = current;
    switch (type) {
    case Ramp::Type::None:
        return Ramp();
    case Ramp::Type::Uniform:
        return Ramp::uniform(current.isActive() ? current.maximum() : blur ? 1.0 : 50.0);
    case Ramp::Type::Pattern:
        // Mustertransparenz: Vordergrund deckend, Hintergrund durchsichtig
        if (current.type != Ramp::Type::Pattern) {
            r.from = 0.0;
            r.to = 100.0;
            r.pattern = PatternFill();
            r.pattern.tile = 5.0;
        }
        r.type = type;
        return r;
    default:
        if (!current.isGradient()) {
            // Am Anfang ohne Wirkung, am Ende voll (bzw. die bisherige Stärke)
            r.from = 0.0;
            r.to = blur ? (current.isActive() ? current.maximum() : 2.0) : 100.0;
            const bool centred = type != Ramp::Type::Linear;
            r.start = centred ? QPointF(0.5, 0.5) : QPointF(0.0, 0.5);
            r.end = QPointF(1.0, 0.5);
        }
        r.type = type;
        return r;
    }
}

const DrawObject* EffectTool::target() const
{
    const QList<const DrawObject*> objects = m_canvas.editor().selectedObjects();
    return objects.isEmpty() ? nullptr : objects.first();
}

QPointF EffectTool::toUnit(const DrawObject& o, const QPointF& doc) const
{
    const QRectF b = o.localPath().boundingRect();
    const QPointF local = o.transform().inverted().map(doc);
    return QPointF(b.width() != 0 ? (local.x() - b.left()) / b.width() : 0.5,
                   b.height() != 0 ? (local.y() - b.top()) / b.height() : 0.5);
}

QPointF EffectTool::fromUnit(const DrawObject& o, const QPointF& unit) const
{
    const QRectF b = o.localPath().boundingRect();
    return o.transform().map(QPointF(b.left() + unit.x() * b.width(), b.top() + unit.y() * b.height()));
}

void EffectTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    Editor& ed = m_canvas.editor();
    m_pressDoc = doc;
    if (const DrawObject* o = target(); o && o->effect(m_effect).isGradient()) {
        const Ramp& r = o->effect(m_effect);
        const QPointF s = m_canvas.toScreen(fromUnit(*o, r.start));
        const QPointF e = m_canvas.toScreen(fromUnit(*o, r.end));
        const QPointF pos = event->position();
        auto near = [&pos](const QPointF& h) { return std::abs(pos.x() - h.x()) <= kHandle + 2 && std::abs(pos.y() - h.y()) <= kHandle + 2; };
        if (near(e) || near(s)) {
            // Liegen beide Griffe unter der Maus, gilt der nähere (nicht immer das Ende)
            const double toEnd = QLineF(pos, e).length();
            const double toStart = QLineF(pos, s).length();
            m_drag = near(e) && (!near(s) || toEnd < toStart) ? Drag::End : Drag::Start;
            m_selectedEnd = m_drag == Drag::End ? 1 : 0;
            m_target = o->id();
            m_preview = r;
            m_canvas.update();
            return;
        }
        // Mittelpunkt-Regler auf der Linie
        if (slider::hit(pos, s, e, r.midpoint)) {
            m_drag = Drag::Mid;
            m_target = o->id();
            m_preview = r;
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
    if (ed.isLocked(hit->id()))
        return;
    m_target = hit->id();
    m_drag = Drag::Create;
    m_selectedEnd = -1;
    m_preview.reset();
}

void EffectTool::mouseMove(QMouseEvent* event, const QPointF& doc)
{
    if (m_drag == Drag::None)
        return;
    const DrawObject* o = m_canvas.editor().document().findAs<DrawObject>(m_target);
    if (!o)
        return;
    Ramp r = m_preview ? *m_preview : o->effect(m_effect);
    switch (m_drag) {
    case Drag::Create: {
        const QPointF d = m_canvas.toScreen(doc) - m_canvas.toScreen(m_pressDoc);
        if (std::abs(d.x()) + std::abs(d.y()) < 4.0)
            return;
        const Ramp& current = o->effect(m_effect);
        r = defaultRamp(m_effect, current.isGradient() ? current.type : Ramp::Type::Linear, current);
        r.start = toUnit(*o, m_pressDoc);
        r.end = toUnit(*o, constrained(m_pressDoc, doc, event->modifiers()));
        break;
    }
    case Drag::Start:
        r.start = toUnit(*o, constrained(fromUnit(*o, r.end), doc, event->modifiers()));
        break;
    case Drag::End:
        r.end = toUnit(*o, constrained(fromUnit(*o, r.start), doc, event->modifiers()));
        break;
    case Drag::Mid:
        r.midpoint = slider::fraction(m_canvas.toScreen(doc), m_canvas.toScreen(fromUnit(*o, r.start)),
                                      m_canvas.toScreen(fromUnit(*o, r.end)), 0.01, 0.99);
        break;
    case Drag::None:
        break;
    }
    m_preview = r;
    m_canvas.setEffectPreview(m_target, m_effect, r);
}

void EffectTool::mouseRelease(QMouseEvent* event, const QPointF&)
{
    if (event->button() != Qt::LeftButton)
        return;
    if (m_drag != Drag::None && m_preview)
        m_canvas.editor().setObjectEffect(m_target, m_effect, *m_preview, label());
    m_drag = Drag::None;
    m_preview.reset();
    m_canvas.clearEffectPreview();
}

bool EffectTool::keyPress(QKeyEvent* event)
{
    if (event->key() != Qt::Key_Escape)
        return false;
    if (m_drag != Drag::None) {
        m_drag = Drag::None;
        m_preview.reset();
        m_canvas.clearEffectPreview();
    } else {
        m_selectedEnd = -1;
        m_canvas.update();
    }
    return true;
}

void EffectTool::paintOverlay(QPainter& p)
{
    const DrawObject* o = m_drag != Drag::None ? m_canvas.editor().document().findAs<DrawObject>(m_target) : target();
    if (!o)
        return;
    const Ramp r = m_preview ? *m_preview : o->effect(m_effect);
    p.save();
    p.setPen(QPen(Canvas::previewColor(), 1, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawPath(m_canvas.viewTransform().map(o->snapPath()));
    if (r.isGradient()) {
        const QPointF s = m_canvas.toScreen(fromUnit(*o, r.start));
        const QPointF e = m_canvas.toScreen(fromUnit(*o, r.end));
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(Qt::white, 3));
        p.drawLine(s, e);
        p.setPen(QPen(Qt::black, 1, Qt::DashLine));
        p.drawLine(s, e);
        const QPointF ends[2] = {s, e};
        const double values[2] = {r.from, r.to};
        const bool blur = m_effect == Effect::Blur;
        p.setRenderHint(QPainter::Antialiasing, false);
        for (int i = 0; i < 2; ++i) {
            const QRectF box(std::round(ends[i].x()) - kHandle, std::round(ends[i].y()) - kHandle, 2 * kHandle, 2 * kHandle);
            p.setPen(Qt::NoPen);
            p.setBrush(i == m_selectedEnd ? QColor(0xe0, 0x20, 0x20) : QColor(Qt::black));
            p.drawRect(box.adjusted(-2, -2, 2, 2));
            p.setBrush(Qt::white);
            p.drawRect(box.adjusted(-1, -1, 1, 1));
            // Transparenz als Grauwert (schwarz = deckend), Unschärfe hell
            const int gray = blur ? 0xd8 : int(std::lround(255.0 * std::clamp(values[i], 0.0, 100.0) / 100.0));
            p.setBrush(QColor(gray, gray, gray));
            p.drawRect(box);
            const QString text = blur ? QStringLiteral("%1 mm").arg(values[i], 0, 'f', 1)
                                      : QStringLiteral("%1 %").arg(std::lround(values[i]));
            // Mit weissem Rand, damit der Wert auch auf dunklem Grund lesbar bleibt
            const QPointF at = box.bottomRight() + QPointF(5, 10);
            p.setPen(Qt::white);
            for (const QPointF& d : {QPointF(-1, 0), QPointF(1, 0), QPointF(0, -1), QPointF(0, 1)})
                p.drawText(at + d, text);
            p.setPen(Qt::black);
            p.drawText(at, text);
        }
        slider::paint(p, s, e, r.midpoint,
                      m_drag == Drag::Mid ? QObject::tr("Mittelpunkt %1 %").arg(std::lround(r.midpoint * 100)) : QString());
    }
    p.restore();
}

} // namespace vek

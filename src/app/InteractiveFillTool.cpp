#include "InteractiveFillTool.h"

#include "Canvas.h"
#include "GradientSlider.h"
#include "Icons.h"
#include "NodePopup.h"
#include "Options.h"

#include <QImage>
#include <QLineF>
#include <QtMath>

#include <cmath>

namespace vek {

namespace {

constexpr double kHandle = 5.0; // halbe Kantenlänge der Endpunkt-Quadrate, px

Color mix(const Color& a, const Color& b, double t)
{
    auto m = [t](std::uint8_t x, std::uint8_t y) { return static_cast<std::uint8_t>(std::lround(x + (y - x) * t)); };
    return Color(m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a));
}

} // namespace

void InteractiveFillTool::activate()
{
    m_canvas.setCursor(QCursor(icon(QStringLiteral("tool-interactive-fill")).pixmap(24), 3, 20));
}

void InteractiveFillTool::deactivate()
{
    m_drag = Drag::None;
    m_preview.reset();
    m_selectedStop = -1;
    m_eyedropper = false;
    hidePopup();
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

Color InteractiveFillTool::colorAt(const Fill& f, double t)
{
    if (f.stops.isEmpty())
        return Color();
    if (t <= f.stops.first().position)
        return f.stops.first().color;
    for (int i = 1; i < f.stops.size(); ++i) {
        const GradientStop& a = f.stops[i - 1];
        const GradientStop& b = f.stops[i];
        if (t <= b.position)
            return mix(a.color, b.color, b.position > a.position ? (t - a.position) / (b.position - a.position) : 0.0);
    }
    return f.stops.last().color;
}

bool InteractiveFillTool::showsStops(const Fill& f)
{
    return f.isGradient() && !midpoint(f);
}

QPointF InteractiveFillTool::stopScreen(const DrawObject& o, const Fill& f, int stop) const
{
    const QPointF s = m_canvas.toScreen(fromUnit(o, f.start));
    const QPointF e = m_canvas.toScreen(fromUnit(o, f.end));
    if (stop <= 0)
        return s;
    if (stop >= f.stops.size() - 1)
        return e;
    return s + (e - s) * f.stops[stop].position;
}

int InteractiveFillTool::stopAt(const DrawObject& o, const Fill& f, const QPointF& pos) const
{
    if (!f.isGradient() || f.stops.size() < 2)
        return -1;
    auto near = [&pos](const QPointF& h, double r) { return std::abs(pos.x() - h.x()) <= r && std::abs(pos.y() - h.y()) <= r; };
    // Endpunkte zuerst; liegen beide unter der Maus, gilt der nähere
    const int last = int(f.stops.size()) - 1;
    const QPointF s = stopScreen(o, f, 0);
    const QPointF e = stopScreen(o, f, last);
    const bool ns = near(s, kHandle + 2);
    const bool ne = near(e, kHandle + 2);
    if (ns || ne)
        return ne && (!ns || QLineF(pos, e).length() < QLineF(pos, s).length()) ? last : 0;
    if (!showsStops(f))
        return -1;
    for (int i = 1; i < last; ++i)
        if (near(stopScreen(o, f, i), 5.0))
            return i;
    return -1;
}

std::optional<double> InteractiveFillTool::lineAt(const DrawObject& o, const Fill& f, const QPointF& pos) const
{
    if (!f.isGradient())
        return std::nullopt;
    const QPointF s = m_canvas.toScreen(fromUnit(o, f.start));
    const QPointF e = m_canvas.toScreen(fromUnit(o, f.end));
    const double t = slider::fraction(pos, s, e, 0.0, 1.0);
    if (QLineF(pos, s + (e - s) * t).length() > 4.0 || t <= 0.0 || t >= 1.0)
        return std::nullopt;
    return t;
}

int InteractiveFillTool::insertStop(Fill& f, double position, std::optional<Color> color) const
{
    position = std::clamp(position, 0.001, 0.999);
    const Color c = color.value_or(colorAt(f, position));
    int index = 1;
    while (index < f.stops.size() && f.stops[index].position < position)
        ++index;
    index = std::min(index, int(f.stops.size()) - 1);
    f.stops.insert(index, GradientStop{position, c});
    return index;
}

void InteractiveFillTool::commit(const Fill& f, const QString& label)
{
    const DrawObject* o = target();
    if (!o || o->style().fill == f)
        return;
    m_canvas.editor().setObjectFill(o->id(), f, label);
}

void InteractiveFillTool::hidePopup()
{
    if (m_popup)
        m_popup->hide();
}

void InteractiveFillTool::showPopup()
{
    const DrawObject* o = target();
    if (!o || !o->style().fill.isGradient() || m_selectedStop < 0 || m_selectedStop >= o->style().fill.stops.size()) {
        hidePopup();
        return;
    }
    if (!m_popup) {
        m_popup = new NodePopup(&m_canvas);
        // Änderungen im Fenster: Vorschau beim Ziehen des Reglers, sonst ein Undo-Schritt
        QObject::connect(m_popup, &NodePopup::colorEdited, &m_canvas, [this](const Color& c, bool final) {
            const DrawObject* t = target();
            if (!t || !t->style().fill.isGradient() || m_selectedStop < 0 || m_selectedStop >= t->style().fill.stops.size())
                return;
            Fill f = t->style().fill;
            f.stops[m_selectedStop].color = c;
            m_target = t->id();
            if (final) {
                m_preview.reset();
                commit(f, QObject::tr("Verlaufsfarbe"));
            } else {
                m_preview = f;
            }
            m_canvas.update();
        });
        QObject::connect(m_popup, &NodePopup::positionEdited, &m_canvas, [this](double position) {
            const DrawObject* t = target();
            if (!t || !t->style().fill.isGradient() || m_selectedStop <= 0 || m_selectedStop >= t->style().fill.stops.size() - 1)
                return;
            Fill f = t->style().fill;
            const Color color = f.stops[m_selectedStop].color;
            f.stops.removeAt(m_selectedStop);
            m_selectedStop = insertStop(f, position, color);
            commit(f, QObject::tr("Verlaufsknoten verschieben"));
            showPopup();
        });
        QObject::connect(&m_canvas.editor(), &Editor::selectionChanged, m_popup, [this] {
            m_selectedStop = -1;
            hidePopup();
        });
        QObject::connect(m_popup, &NodePopup::eyedropperRequested, &m_canvas, [this] {
            m_eyedropper = true;
            m_canvas.setCursor(QCursor(icon(QStringLiteral("eyedropper")).pixmap(24), 2, 21));
        });
    }
    const Fill& f = o->style().fill;
    const bool inner = m_selectedStop > 0 && m_selectedStop < f.stops.size() - 1;
    m_popup->showColor(f.stops[m_selectedStop].color, inner ? std::optional(f.stops[m_selectedStop].position) : std::nullopt,
                       stopScreen(*o, f, m_selectedStop));
}

void InteractiveFillTool::pickColor(const QPointF& screen)
{
    // Pipette: Farbe der Zeichenfläche an dieser Stelle, wie gezeichnet (ohne das Knotenfenster)
    m_eyedropper = false;
    activate();
    const bool shown = m_popup && m_popup->isVisible();
    if (shown)
        m_popup->hide();
    const QImage image = m_canvas.grab(QRect(screen.toPoint(), QSize(1, 1))).toImage();
    const DrawObject* o = target();
    if (!image.isNull() && o && o->style().fill.isGradient() && m_selectedStop >= 0
        && m_selectedStop < o->style().fill.stops.size()) {
        Fill f = o->style().fill;
        const QColor picked = image.pixelColor(0, 0);
        const std::uint8_t alpha = f.stops[m_selectedStop].color.a;
        f.stops[m_selectedStop].color = Color(std::uint8_t(picked.red()), std::uint8_t(picked.green()),
                                              std::uint8_t(picked.blue()), alpha);
        commit(f, QObject::tr("Verlaufsfarbe"));
    }
    if (shown)
        showPopup();
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
    Editor& ed = m_canvas.editor();
    const QPointF pos = event->position();
    if (m_eyedropper && event->button() == Qt::LeftButton) {
        pickColor(pos);
        return;
    }
    if (event->button() == Qt::RightButton) {
        // Rechtsklick auf einen Zwischenknoten entfernt ihn
        const DrawObject* o = target();
        if (!o)
            return;
        Fill f = o->style().fill;
        const int stop = stopAt(*o, f, pos);
        if (stop > 0 && stop < f.stops.size() - 1) {
            f.stops.removeAt(stop);
            m_selectedStop = -1;
            hidePopup();
            commit(f, QObject::tr("Verlaufsknoten löschen"));
        }
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;
    m_pressDoc = doc;
    m_pressScreen = pos;
    m_moved = false;

    if (const DrawObject* o = target(); o && o->style().fill.isGradient()) {
        const Fill& f = o->style().fill;
        if (const int stop = stopAt(*o, f, pos); stop >= 0) {
            m_drag = stop == 0 ? Drag::Start : stop == f.stops.size() - 1 ? Drag::End : Drag::Stop;
            m_selectedStop = stop;
            m_target = o->id();
            m_preview = f;
            m_canvas.update();
            return;
        }
        // Mittelpunkt-Regler (nur bei zwei Farben)
        const QPointF s = m_canvas.toScreen(fromUnit(*o, f.start));
        const QPointF e = m_canvas.toScreen(fromUnit(*o, f.end));
        if (const std::optional<double> mid = midpoint(f); mid && slider::hit(pos, s, e, *mid)) {
            m_drag = Drag::Mid;
            m_target = o->id();
            m_preview = f;
            hidePopup();
            m_canvas.update();
            return;
        }
    }

    hidePopup();
    const DrawObject* hit = ed.objectAt(doc, m_canvas.pixelsToDoc(4.0));
    if (!hit) {
        m_selectedStop = -1;
        m_canvas.update();
        return;
    }
    if (!ed.isSelected(hit->id()))
        ed.setSelection({hit->id()});
    m_target = hit->id();
    m_drag = Drag::Create;
    m_selectedStop = -1;
    m_preview.reset();
}

void InteractiveFillTool::mouseMove(QMouseEvent* event, const QPointF& doc)
{
    if (m_drag == Drag::None)
        return;
    const DrawObject* o = m_canvas.editor().document().findAs<DrawObject>(m_target);
    if (!o)
        return;
    const QPointF d = event->position() - m_pressScreen;
    if (!m_moved && std::abs(d.x()) + std::abs(d.y()) < 3.0)
        return; // Klick ohne Ziehen: Knoten wählen, nichts verschieben
    m_moved = true;
    hidePopup();
    Fill f = m_preview ? *m_preview : baseFill(*o);
    switch (m_drag) {
    case Drag::Create: {
        const QPointF ds = m_canvas.toScreen(doc) - m_canvas.toScreen(m_pressDoc);
        if (std::abs(ds.x()) + std::abs(ds.y()) < 4.0)
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
    case Drag::Stop: {
        // Zwischenknoten gleitet auf der Linie, zwischen seinen Nachbarn
        const int i = m_selectedStop;
        if (i <= 0 || i >= f.stops.size() - 1)
            break;
        const double lo = f.stops[i - 1].position + 0.001;
        const double hi = f.stops[i + 1].position - 0.001;
        f.stops[i].position = slider::fraction(m_canvas.toScreen(doc), m_canvas.toScreen(fromUnit(*o, f.start)),
                                               m_canvas.toScreen(fromUnit(*o, f.end)), std::min(lo, hi), std::max(lo, hi));
        break;
    }
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
    const Drag drag = m_drag;
    if (m_drag != Drag::None && m_preview && m_moved) {
        const DrawObject* o = m_canvas.editor().document().findAs<DrawObject>(m_target);
        if (o && !(o->style().fill == *m_preview))
            m_canvas.editor().setObjectFill(m_target, *m_preview,
                                            drag == Drag::Stop ? QObject::tr("Verlaufsknoten verschieben")
                                                               : QObject::tr("Interaktive Füllung"));
    }
    m_drag = Drag::None;
    m_preview.reset();
    // Knoten angeklickt oder verschoben: Fenster daneben
    if (drag == Drag::Start || drag == Drag::End || drag == Drag::Stop)
        showPopup();
    m_canvas.update();
}

void InteractiveFillTool::mouseDoubleClick(QMouseEvent* event, const QPointF&)
{
    if (event->button() != Qt::LeftButton)
        return;
    // Doppelklick auf die Verlaufslinie: Knoten mit der Farbe an dieser Stelle einfügen
    const DrawObject* o = target();
    if (!o || !o->style().fill.isGradient() || stopAt(*o, o->style().fill, event->position()) >= 0)
        return;
    const std::optional<double> t = lineAt(*o, o->style().fill, event->position());
    if (!t)
        return;
    m_drag = Drag::None;
    m_preview.reset();
    Fill f = o->style().fill;
    m_selectedStop = insertStop(f, *t, std::nullopt);
    commit(f, QObject::tr("Verlaufsknoten einfügen"));
    showPopup();
    m_canvas.update();
}

bool InteractiveFillTool::keyPress(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        if (m_eyedropper) {
            m_eyedropper = false;
            activate();
        } else if (m_drag != Drag::None) {
            m_drag = Drag::None;
            m_preview.reset();
        } else {
            m_selectedStop = -1;
            hidePopup();
        }
        m_canvas.update();
        return true;
    }
    if (event->key() == Qt::Key_Delete && m_drag == Drag::None) {
        // Entf entfernt den gewählten Zwischenknoten
        const DrawObject* o = target();
        if (o && o->style().fill.isGradient() && m_selectedStop > 0 && m_selectedStop < o->style().fill.stops.size() - 1) {
            Fill f = o->style().fill;
            f.stops.removeAt(m_selectedStop);
            m_selectedStop = -1;
            hidePopup();
            commit(f, QObject::tr("Verlaufsknoten löschen"));
            return true;
        }
    }
    return false;
}

bool InteractiveFillTool::paletteColor(const std::optional<Color>& color, bool fill)
{
    const DrawObject* o = target();
    if (!fill || !color || m_selectedStop < 0 || !o || !o->style().fill.isGradient()
        || m_selectedStop >= o->style().fill.stops.size())
        return false;
    Fill f = o->style().fill;
    f.stops[m_selectedStop].color = *color;
    commit(f, QObject::tr("Verlaufsfarbe"));
    if (m_popup && m_popup->isVisible())
        showPopup();
    return true;
}

bool InteractiveFillTool::colorDropped(const Color& color, const QPointF& screen)
{
    // Palettenfarbe auf einen Knoten: färbt ihn; auf die Linie: neuer Knoten in dieser Farbe
    const DrawObject* o = target();
    if (!o || !o->style().fill.isGradient())
        return false;
    Fill f = o->style().fill;
    if (const int stop = stopAt(*o, f, screen); stop >= 0) {
        f.stops[stop].color = color;
        m_selectedStop = stop;
        commit(f, QObject::tr("Verlaufsfarbe"));
        return true;
    }
    const std::optional<double> t = lineAt(*o, f, screen);
    if (!t)
        return false;
    m_selectedStop = insertStop(f, *t, color);
    commit(f, QObject::tr("Verlaufsknoten einfügen"));
    return true;
}

void InteractiveFillTool::paintOverlay(QPainter& p)
{
    const DrawObject* o = m_drag != Drag::None || m_preview ? m_canvas.editor().document().findAs<DrawObject>(m_target) : target();
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
        // Zwei Farben: Mittelpunkt-Regler; sonst Zwischenknoten als kleine Quadrate
        if (const std::optional<double> mid = midpoint(f)) {
            slider::paint(p, s, e, *mid, m_drag == Drag::Mid ? QObject::tr("Mittelpunkt %1 %").arg(std::lround(*mid * 100)) : QString());
        } else {
            p.setRenderHint(QPainter::Antialiasing, false);
            for (int i = 1; i + 1 < f.stops.size(); ++i) {
                const QPointF m = s + (e - s) * f.stops[i].position;
                const QRectF r(std::round(m.x()) - 3.5, std::round(m.y()) - 3.5, 7, 7);
                p.setPen(Qt::NoPen);
                p.setBrush(i == m_selectedStop ? QColor(0xe0, 0x20, 0x20) : QColor(Qt::black));
                p.drawRect(r.adjusted(-1.5, -1.5, 1.5, 1.5));
                p.setBrush(Qt::white);
                p.drawRect(r.adjusted(-0.5, -0.5, 0.5, 0.5));
                p.setBrush(f.stops[i].color.toQColor());
                p.drawRect(r);
            }
        }
        // Endpunkte: Farbfeld mit weissem und dunklem Rand, damit sie auf jedem Hintergrund sichtbar sind
        const QPointF ends[2] = {s, e};
        p.setRenderHint(QPainter::Antialiasing, false);
        for (int i = 0; i < 2; ++i) {
            const int index = i == 0 ? 0 : int(f.stops.size()) - 1;
            const QRectF r(std::round(ends[i].x()) - kHandle, std::round(ends[i].y()) - kHandle, 2 * kHandle, 2 * kHandle);
            p.setPen(Qt::NoPen);
            p.setBrush(index == m_selectedStop ? QColor(0xe0, 0x20, 0x20) : QColor(Qt::black));
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

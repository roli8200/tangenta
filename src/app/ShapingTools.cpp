#include "ShapingTools.h"

#include "Canvas.h"
#include "Editor.h"
#include "Options.h"
#include "PathOps.h"

#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

#include <cmath>

namespace vek {

namespace {

// Weicher Abfall 1 (Mitte) .. 0 (Rand)
double falloff(double d, double r)
{
    if (d >= r)
        return 0.0;
    const double t = d / r;
    return (1.0 - t * t) * (1.0 - t * t);
}

QPainterPath toPath(const QPolygonF& poly, bool closed)
{
    QPainterPath p;
    if (poly.isEmpty())
        return p;
    p.moveTo(poly.first());
    for (int i = 1; i < poly.size(); ++i)
        p.lineTo(poly[i]);
    if (closed)
        p.closeSubpath();
    return p;
}

} // namespace

// --- Spitzen-Werkzeuge -------------------------------------------------------------------------

BrushShapingTool::BrushShapingTool(Canvas& canvas, Mode mode) : Tool(canvas), m_mode(mode)
{
    m_timer.setInterval(40);
    QObject::connect(&m_timer, &QTimer::timeout, [this] { tick(); });
}

double BrushShapingTool::radius()
{
    return std::max(0.1, Options::instance().number(QStringLiteral("ws/tools/shaping/radius")));
}

double BrushShapingTool::strength()
{
    return std::clamp(Options::instance().number(QStringLiteral("ws/tools/shaping/strength")), 1.0, 100.0);
}

void BrushShapingTool::activate()
{
    m_canvas.setCursor(Qt::CrossCursor);
    m_work.clear();
}

void BrushShapingTool::deactivate()
{
    m_timer.stop();
    m_pressed = false;
    m_work.clear();
    m_hasMouse = false;
}

bool BrushShapingTool::begin()
{
    m_work.clear();
    const Editor& ed = m_canvas.editor();
    const double step = std::max(0.03, radius() / 12.0);
    for (const DrawObject* o : ed.selectedObjects()) {
        if (!Editor::canEnvelope(*o) || ed.isLocked(o->id()))
            continue;
        Work w;
        w.id = o->id();
        for (QPolygonF poly : pathops::flattenFine(o->path(), step)) {
            const bool closed = poly.size() > 2 && poly.first() == poly.last();
            w.polys << poly;
            w.closed << closed;
        }
        if (!w.polys.isEmpty())
            m_work << w;
    }
    return !m_work.isEmpty();
}

void BrushShapingTool::step(const QPointF& last, const QPointF& doc)
{
    if (m_work.isEmpty() && !begin())
        return;
    const double r = radius();
    const double k = strength() / 100.0;
    const QPointF delta = doc - last;
    const double fine = std::max(0.03, r / 12.0);
    for (Work& w : m_work) {
        for (int pi = 0; pi < w.polys.size(); ++pi) {
            QPolygonF& poly = w.polys[pi];
            const bool closed = w.closed[pi];
            const int n = int(poly.size()) - (closed ? 1 : 0); // geschlossen: letzter = erster
            for (int i = 0; i < n; ++i) {
                QPointF& p = poly[i];
                const double d = QLineF(p, doc).length();
                if (d >= r)
                    continue;
                const double f = falloff(d, r);
                switch (m_mode) {
                // Stärke 50 % = der Umriss unter der Mitte folgt der Spitze ganz
                case Mode::Smear: p += delta * std::min(1.0, f * (0.5 + k)); break;
                case Mode::Smudge: {
                    // Harte Spitze: innen voll, am Rand weich
                    const double t = d / r;
                    const double hard = t < 0.6 ? 1.0 : 1.0 - (t - 0.6) / 0.4;
                    p += delta * std::min(1.0, hard * (0.5 + k));
                    break;
                }
                case Mode::Twirl: {
                    const double a = qDegreesToRadians(8.0 * k * (1.0 - d / r));
                    const QPointF v = p - doc;
                    p = doc + QPointF(v.x() * std::cos(a) - v.y() * std::sin(a), v.x() * std::sin(a) + v.y() * std::cos(a));
                    break;
                }
                case Mode::Attract: p += (doc - p) * 0.08 * k * (0.3 + 0.7 * f); break;
                case Mode::Repel:
                    if (d > 1e-6)
                        p += (p - doc) / d * (r * 0.06 * k * (0.3 + 0.7 * f));
                    break;
                case Mode::Roughen: {
                    const qint64 key = (qint64(pi) << 32) | i;
                    if (w.roughened.contains(key))
                        break;
                    w.roughened.insert(key);
                    const QPointF a = poly[(i + n - 1) % n], b = poly[(i + 1) % n];
                    QPointF t = b - a;
                    const double len = std::hypot(t.x(), t.y());
                    if (len < 1e-9)
                        break;
                    const QPointF normal(-t.y() / len, t.x() / len);
                    p += normal * (i % 2 ? 1.0 : -1.0) * r * 0.25 * k;
                    break;
                }
                }
            }
            if (closed && !poly.isEmpty())
                poly.last() = poly.first();
            // Gedehnte Stücke unterteilen, damit der Umriss glatt bleibt (nicht beim Aufrauen:
            // dort zählen die Punkte)
            if (m_mode != Mode::Roughen) {
                QPolygonF fineer;
                for (int i = 0; i < poly.size(); ++i) {
                    if (i > 0) {
                        const double len = QLineF(poly[i - 1], poly[i]).length();
                        const int parts = std::min(16, int(len / (fine * 1.5)));
                        for (int s = 1; s <= parts; ++s)
                            fineer << poly[i - 1] + (poly[i] - poly[i - 1]) * (double(s) / (parts + 1));
                    }
                    fineer << poly[i];
                }
                poly = fineer;
            }
        }
    }
    m_canvas.update();
}

void BrushShapingTool::tick()
{
    if (m_pressed && (m_mode == Mode::Twirl || m_mode == Mode::Attract || m_mode == Mode::Repel))
        step(m_mouse, m_mouse);
}

void BrushShapingTool::commit()
{
    if (m_work.isEmpty())
        return;
    const QList<Work> work = m_work;
    m_work.clear();
    QList<NodeId> ids;
    for (const Work& w : work)
        ids << w.id;
    static const char* names[] = {QT_TRANSLATE_NOOP("QObject", "Verschmieren"), QT_TRANSLATE_NOOP("QObject", "Verwischen"),
                                   QT_TRANSLATE_NOOP("QObject", "Aufrauen"), QT_TRANSLATE_NOOP("QObject", "Wirbel"),
                                   QT_TRANSLATE_NOOP("QObject", "Anziehen"), QT_TRANSLATE_NOOP("QObject", "Abstossen")};
    m_canvas.editor().replaceObjects(ids, [&work](const DrawObject& o) -> Editor::Replacement {
        for (const Work& w : work) {
            if (w.id != o.id())
                continue;
            QPainterPath path;
            for (int i = 0; i < w.polys.size(); ++i)
                path.addPath(toPath(w.polys[i], w.closed[i]));
            path.setFillRule(o.path().fillRule());
            QList<SubPath> paths = pathops::fitPath(path);
            std::vector<std::unique_ptr<DrawObject>> out;
            if (paths.isEmpty())
                return out;
            auto curve = std::make_unique<CurveObject>(paths);
            Style s = o.style();
            curve->setStyle(s);
            curve->setEffect(Effect::Blur, o.blurRamp());
            curve->setEffect(Effect::Transparency, o.transparency());
            curve->setName(o.name());
            out.push_back(std::move(curve));
            return out;
        }
        return std::nullopt;
    }, QObject::tr(names[int(m_mode)]));
}

void BrushShapingTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    Editor& ed = m_canvas.editor();
    if (ed.selection().isEmpty()) {
        // Wie gewohnt wirkt die Spitze auf die Auswahl; ohne Auswahl das Objekt darunter wählen
        if (const DrawObject* hit = ed.objectAt(doc, m_canvas.pixelsToDoc(4.0)))
            ed.setSelection({hit->id()});
    }
    if (!begin())
        return;
    m_pressed = true;
    m_last = m_mouse = doc;
    if (m_mode == Mode::Roughen)
        step(doc, doc);
    m_timer.start();
}

void BrushShapingTool::mouseMove(QMouseEvent*, const QPointF& doc)
{
    m_mouse = doc;
    m_hasMouse = true;
    if (m_pressed) {
        step(m_last, doc);
        m_last = doc;
    } else {
        m_canvas.update();
    }
}

void BrushShapingTool::mouseRelease(QMouseEvent* event, const QPointF&)
{
    if (event->button() != Qt::LeftButton || !m_pressed)
        return;
    m_pressed = false;
    m_timer.stop();
    commit();
    m_canvas.update();
}

void BrushShapingTool::paintOverlay(QPainter& p)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const QTransform view = m_canvas.viewTransform();
    for (const Work& w : std::as_const(m_work)) {
        const DrawObject* o = m_canvas.editor().document().findAs<DrawObject>(w.id);
        QPainterPath path;
        for (int i = 0; i < w.polys.size(); ++i)
            path.addPath(toPath(w.polys[i], w.closed[i]));
        if (o) {
            if (const std::optional<Color> c = o->style().fill.representativeColor(); c && !o->style().fill.isNone())
                p.fillPath(view.map(path), c->toQColor());
        }
        p.setPen(QPen(Canvas::previewColor(), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawPath(view.map(path));
    }
    if (m_hasMouse) {
        // Spitze als Kreis
        const double r = radius() * m_canvas.zoom();
        p.setPen(QPen(QColor(0x30, 0x30, 0x30), 1.0, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(view.map(m_mouse), r, r);
    }
    p.restore();
}

// --- Freie Transformation ------------------------------------------------------------------

FreeTransformTool::Mode FreeTransformTool::mode()
{
    return Mode(std::clamp(Options::instance().integer(QStringLiteral("ws/tools/freeTransform/mode")), 0, 3));
}

void FreeTransformTool::activate()
{
    m_canvas.setCursor(Qt::CrossCursor);
}

void FreeTransformTool::deactivate()
{
    m_pressed = false;
}

QTransform FreeTransformTool::transformFor(Mode mode, const QPointF& pivot, const QPointF& to, const QRectF& bounds)
{
    const QPointF v = to - pivot;
    // Bezugsstrecke: von pivot zur entferntesten Ecke der Auswahl
    QPointF ref(1, 1);
    double best = -1;
    for (const QPointF& c : {bounds.topLeft(), bounds.topRight(), bounds.bottomLeft(), bounds.bottomRight()})
        if (const double d = QLineF(pivot, c).length(); d > best) {
            best = d;
            ref = c - pivot;
        }
    switch (mode) {
    case Mode::Rotate: return xform::rotation(qRadiansToDegrees(std::atan2(v.y(), v.x())), pivot);
    case Mode::Mirror: {
        // An der Geraden durch pivot in Zugrichtung spiegeln
        const double a = std::atan2(v.y(), v.x());
        const QTransform r(std::cos(2 * a), std::sin(2 * a), std::sin(2 * a), -std::cos(2 * a), 0, 0);
        return QTransform::fromTranslate(-pivot.x(), -pivot.y()) * r * QTransform::fromTranslate(pivot.x(), pivot.y());
    }
    case Mode::Scale: {
        auto ratio = [](double num, double den) {
            const double f = std::abs(den) > 1e-6 ? num / den : 1.0;
            return std::abs(f) < 0.01 ? (f < 0 ? -0.01 : 0.01) : f;
        };
        return xform::scaling(ratio(v.x(), ref.x()), ratio(v.y(), ref.y()), pivot);
    }
    case Mode::Skew:
        if (std::abs(v.x()) >= std::abs(v.y()))
            return xform::skewing(qRadiansToDegrees(std::atan2(v.x(), std::max(1e-6, std::abs(ref.y())))), 0.0, pivot);
        return xform::skewing(0.0, qRadiansToDegrees(std::atan2(v.y(), std::max(1e-6, std::abs(ref.x())))), pivot);
    }
    return {};
}

void FreeTransformTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    Editor& ed = m_canvas.editor();
    if (ed.selection().isEmpty()) {
        if (const DrawObject* hit = ed.objectAt(doc, m_canvas.pixelsToDoc(4.0)))
            ed.setSelection({hit->id()});
        return; // erst auswählen, dann transformieren
    }
    m_pressed = true;
    m_pivot = m_current = doc;
}

void FreeTransformTool::mouseMove(QMouseEvent*, const QPointF& doc)
{
    if (!m_pressed)
        return;
    m_current = doc;
    m_canvas.update();
}

void FreeTransformTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton || !m_pressed)
        return;
    m_pressed = false;
    if (QLineF(m_canvas.toScreen(m_pivot), m_canvas.toScreen(doc)).length() < 3.0) {
        m_canvas.update();
        return;
    }
    Editor& ed = m_canvas.editor();
    static const char* names[] = {QT_TRANSLATE_NOOP("QObject", "Freies Drehen"), QT_TRANSLATE_NOOP("QObject", "Freies Spiegeln"),
                                   QT_TRANSLATE_NOOP("QObject", "Freies Skalieren"), QT_TRANSLATE_NOOP("QObject", "Freies Neigen")};
    ed.transformSelection(transformFor(mode(), m_pivot, doc, ed.selectionBounds()), QObject::tr(names[int(mode())]));
    m_canvas.update();
}

void FreeTransformTool::paintOverlay(QPainter& p)
{
    if (!m_pressed)
        return;
    const Editor& ed = m_canvas.editor();
    const QTransform m = transformFor(mode(), m_pivot, m_current, ed.selectionBounds()) * m_canvas.viewTransform();
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Canvas::previewColor(), 1.0));
    p.setBrush(Qt::NoBrush);
    for (const DrawObject* o : ed.selectedObjects())
        p.drawPath(m.map(o->path()));
    p.setPen(QPen(QColor(0x30, 0x30, 0x30), 1.0, Qt::DashLine));
    p.drawLine(m_canvas.toScreen(m_pivot), m_canvas.toScreen(m_current));
    p.restore();
}

} // namespace vek

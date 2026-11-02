#include "InteractiveTools.h"

#include "Canvas.h"
#include "Editor.h"
#include "Options.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QtMath>

#include <cmath>

namespace vek {

namespace {

constexpr double kHandle = 5.0;

void drawHandle(QPainter& p, const QPointF& s, const QColor& fill = Qt::white)
{
    p.setPen(QPen(Qt::black, 1.0));
    p.setBrush(fill);
    p.drawRect(QRectF(s.x() - kHandle, s.y() - kHandle, 2 * kHandle, 2 * kHandle));
}

bool nearHandle(const QPointF& a, const QPointF& b)
{
    return std::abs(a.x() - b.x()) <= kHandle + 2 && std::abs(a.y() - b.y()) <= kHandle + 2;
}

const DrawObject* pickTarget(Canvas& canvas, const QPointF& doc)
{
    Editor& ed = canvas.editor();
    const DrawObject* hit = ed.objectAt(doc, canvas.pixelsToDoc(4.0));
    if (hit && !ed.isSelected(hit->id()))
        ed.setSelection({hit->id()});
    return hit;
}

} // namespace

// --- Überblendung ----------------------------------------------------------------------------

void BlendTool::activate()
{
    m_canvas.setCursor(Qt::CrossCursor);
}

void BlendTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    const DrawObject* hit = pickTarget(m_canvas, doc);
    m_from = hit ? hit->id() : 0;
    m_press = m_current = doc;
    m_dragging = m_from != 0;
}

void BlendTool::mouseMove(QMouseEvent*, const QPointF& doc)
{
    if (!m_dragging)
        return;
    m_current = doc;
    m_canvas.update();
}

void BlendTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton || !m_dragging)
        return;
    m_dragging = false;
    Editor& ed = m_canvas.editor();
    const DrawObject* to = ed.objectAt(doc, m_canvas.pixelsToDoc(4.0));
    if (to && to->id() != m_from)
        ed.blendObjects(m_from, to->id(), Options::instance().integer(QStringLiteral("ws/tools/blend/steps")));
    m_canvas.update();
}

void BlendTool::paintOverlay(QPainter& p)
{
    if (!m_dragging)
        return;
    p.save();
    p.setPen(QPen(Canvas::previewColor(), 1.0, Qt::DashLine));
    p.drawLine(m_canvas.toScreen(m_press), m_canvas.toScreen(m_current));
    p.restore();
}

// --- Kontur ----------------------------------------------------------------------------------

void ContourTool::activate()
{
    m_canvas.setCursor(Qt::CrossCursor);
}

ContourEffect ContourTool::contourFor(const DrawObject& o, const QPointF& press, const QPointF& to, const ContourEffect& base)
{
    ContourEffect c = base;
    c.active = true;
    const QPointF center = o.boundingRect().center();
    const double d0 = QLineF(center, press).length(), d1 = QLineF(center, to).length();
    c.direction = d1 >= d0 ? ContourEffect::Direction::Outside : ContourEffect::Direction::Inside;
    c.steps = std::max(1, base.steps);
    c.offset = std::max(0.05, std::abs(d1 - d0) / c.steps);
    return c;
}

void ContourTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    const DrawObject* hit = pickTarget(m_canvas, doc);
    m_target = hit ? hit->id() : 0;
    m_press = doc;
    m_dragging = m_target != 0;
    m_preview.reset();
}

void ContourTool::mouseMove(QMouseEvent*, const QPointF& doc)
{
    if (!m_dragging)
        return;
    const auto* o = m_canvas.editor().document().findAs<DrawObject>(m_target);
    if (!o)
        return;
    ContourEffect base = o->contour();
    if (!base.active)
        base.steps = Options::instance().integer(QStringLiteral("ws/tools/contour/steps"));
    m_preview = contourFor(*o, m_press, doc, base);
    m_canvas.update();
}

void ContourTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton || !m_dragging)
        return;
    mouseMove(event, doc);
    m_dragging = false;
    if (m_preview && QLineF(m_press, doc).length() > m_canvas.pixelsToDoc(2.0))
        m_canvas.editor().setObjectContour(m_target, *m_preview, QObject::tr("Kontur"));
    m_preview.reset();
    m_canvas.update();
    m_canvas.notifyToolState();
}

void ContourTool::paintOverlay(QPainter& p)
{
    if (!m_preview)
        return;
    const auto* o = m_canvas.editor().document().findAs<DrawObject>(m_target);
    if (!o)
        return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Canvas::previewColor(), 1.0));
    p.setBrush(Qt::NoBrush);
    for (const Canvas::ContourStep& s : Canvas::contourSteps(*o, *m_preview))
        p.drawPath(m_canvas.viewTransform().map(s.path));
    p.restore();
}

// --- Extrusion -------------------------------------------------------------------------------

void ExtrudeTool::activate()
{
    m_canvas.setCursor(Qt::CrossCursor);
}

void ExtrudeTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    // Fluchtpunkt einer bestehenden Extrusion ziehen
    const QList<const DrawObject*> sel = m_canvas.editor().selectedObjects();
    if (sel.size() == 1 && sel.first()->extrusion().active) {
        const QPointF vp = sel.first()->boundingRect().center() + sel.first()->extrusion().vanish;
        if (nearHandle(m_canvas.toScreen(vp), m_canvas.toScreen(doc))) {
            m_target = sel.first()->id();
            m_dragging = true;
            return;
        }
    }
    const DrawObject* hit = pickTarget(m_canvas, doc);
    m_target = hit ? hit->id() : 0;
    m_dragging = m_target != 0;
    m_preview.reset();
}

void ExtrudeTool::mouseMove(QMouseEvent*, const QPointF& doc)
{
    if (!m_dragging)
        return;
    const auto* o = m_canvas.editor().document().findAs<DrawObject>(m_target);
    if (!o)
        return;
    Extrusion e = o->extrusion();
    e.active = true;
    // Die Seitenflächen reichen bis zur Maus: Fluchtpunkt so, dass depth % dort enden
    const QPointF c = o->boundingRect().center();
    const double f = std::clamp(e.depth, 1.0, 99.0) / 100.0;
    e.vanish = (doc - c) / f;
    m_preview = e;
    m_canvas.update();
}

void ExtrudeTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton || !m_dragging)
        return;
    mouseMove(event, doc);
    m_dragging = false;
    if (m_preview)
        m_canvas.editor().setObjectExtrusion(m_target, *m_preview, QObject::tr("Extrusion"));
    m_preview.reset();
    m_canvas.update();
    m_canvas.notifyToolState();
}

void ExtrudeTool::paintOverlay(QPainter& p)
{
    const QList<const DrawObject*> sel = m_canvas.editor().selectedObjects();
    const DrawObject* o = m_preview ? m_canvas.editor().document().findAs<DrawObject>(m_target) : (sel.size() == 1 ? sel.first() : nullptr);
    if (!o)
        return;
    const Extrusion e = m_preview ? *m_preview : o->extrusion();
    if (!e.active)
        return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    if (m_preview) {
        p.setPen(QPen(Canvas::previewColor(), 1.0));
        p.setBrush(Qt::NoBrush);
        for (const auto& face : Canvas::extrusionFaces(*o, e))
            p.drawPolygon(m_canvas.viewTransform().map(face.first));
    }
    const QPointF c = m_canvas.toScreen(o->boundingRect().center());
    const QPointF vp = m_canvas.toScreen(o->boundingRect().center() + e.vanish);
    p.setPen(QPen(QColor(0x30, 0x30, 0x30), 1.0, Qt::DashLine));
    p.drawLine(c, vp);
    // Fluchtpunkt als Kreuz
    p.setPen(QPen(Qt::black, 1.5));
    p.drawLine(vp + QPointF(-6, -6), vp + QPointF(6, 6));
    p.drawLine(vp + QPointF(-6, 6), vp + QPointF(6, -6));
    p.restore();
}

// --- Verzerren -------------------------------------------------------------------------------

DistortTool::Mode DistortTool::mode()
{
    return Mode(std::clamp(Options::instance().integer(QStringLiteral("ws/tools/distort/mode")), 0, 2));
}

void DistortTool::activate()
{
    m_canvas.setCursor(Qt::CrossCursor);
}

QList<SubPath> DistortTool::distort(const QList<SubPath>& paths, Mode mode, double amount, int frequency, const QPointF& c)
{
    QList<SubPath> out;
    if (mode == Mode::PushPull) {
        // Knoten von der Mitte weg (Ziehen, +) bzw. hin (Stossen, −), Steuerpunkte gegenläufig
        const double k = amount / 100.0;
        for (SubPath sp : paths) {
            for (CurveNode& n : sp.nodes) {
                n.point = c + (n.point - c) * (1.0 + k);
                if (!n.curveIn) {
                    n.curveIn = true;
                    n.c1 = n.c2 = n.point;
                }
                n.c1 = c + (n.c1 - c) * (1.0 - k);
                n.c2 = c + (n.c2 - c) * (1.0 - k);
            }
            out << sp;
        }
        return out;
    }
    // Zickzack und Wirbel: erst fein abtasten
    const QPainterPath path = CurveObject::toPainterPath(paths);
    double rmax = 1e-6;
    for (const QPolygonF& poly : path.toSubpathPolygons())
        for (const QPointF& p : poly)
            rmax = std::max(rmax, QLineF(c, p).length());
    for (const QPolygonF& poly : path.toSubpathPolygons()) {
        SubPath sp;
        sp.closed = poly.isClosed();
        QPolygonF dense;
        const int perSeg = mode == Mode::Zipper ? std::max(2, frequency) : 8;
        for (int i = 0; i + 1 < poly.size(); ++i)
            for (int k = 0; k < perSeg; ++k)
                dense << poly[i] + (poly[i + 1] - poly[i]) * (double(k) / perSeg);
        if (!sp.closed && !poly.isEmpty())
            dense << poly.last();
        for (int i = 0; i < dense.size(); ++i) {
            QPointF p = dense[i];
            if (mode == Mode::Zipper) {
                const QPointF next = dense[(i + 1) % dense.size()], prev = dense[(i - 1 + dense.size()) % dense.size()];
                QPointF t = next - prev;
                const double len = std::max(1e-9, std::hypot(t.x(), t.y()));
                const QPointF nrm(-t.y() / len, t.x() / len);
                p += nrm * amount * ((i % 2) ? 1.0 : -1.0);
            } else {
                const double r = QLineF(c, p).length();
                const double a = qDegreesToRadians(amount) * (1.0 - r / rmax);
                const QPointF d = p - c;
                p = c + QPointF(d.x() * std::cos(a) - d.y() * std::sin(a), d.x() * std::sin(a) + d.y() * std::cos(a));
            }
            sp.nodes << CurveNode{p};
        }
        if (sp.nodes.size() > 1)
            out << sp;
    }
    return out;
}

double DistortTool::amountFor(const QPointF& to) const
{
    const double d = QLineF(m_press, to).length();
    switch (mode()) {
    case Mode::PushPull: {
        // Nach aussen ziehen = Ziehen (+), nach innen = Stossen (−), relativ zur Objektgrösse
        const double sign = QLineF(m_center, to).length() >= QLineF(m_center, m_press).length() ? 1.0 : -1.0;
        return sign * std::min(200.0, d * 2.0);
    }
    case Mode::Zipper: return d / 5.0;
    case Mode::Twister: return d * 6.0;
    }
    return 0;
}

void DistortTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    const DrawObject* hit = pickTarget(m_canvas, doc);
    if (!hit)
        return;
    m_target = hit->id();
    m_original = CurveObject::fromPath(hit->path())->subPaths();
    m_center = hit->boundingRect().center();
    m_press = m_current = doc;
    m_dragging = true;
}

void DistortTool::mouseMove(QMouseEvent*, const QPointF& doc)
{
    if (!m_dragging)
        return;
    m_current = doc;
    m_canvas.update();
}

void DistortTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton || !m_dragging)
        return;
    m_dragging = false;
    if (QLineF(m_press, doc).length() < m_canvas.pixelsToDoc(2.0))
        return;
    const QList<SubPath> result =
        distort(m_original, mode(), amountFor(doc), Options::instance().integer(QStringLiteral("ws/tools/distort/frequency")), m_center);
    m_canvas.editor().replaceObjectWithCurve(m_target, result, QObject::tr("Verzerren"));
    m_canvas.update();
}

void DistortTool::paintOverlay(QPainter& p)
{
    if (!m_dragging)
        return;
    const QList<SubPath> result =
        distort(m_original, mode(), amountFor(m_current), Options::instance().integer(QStringLiteral("ws/tools/distort/frequency")), m_center);
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Canvas::previewColor(), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawPath(m_canvas.viewTransform().map(CurveObject::toPainterPath(result)));
    p.setPen(QPen(QColor(0x30, 0x30, 0x30), 1.0, Qt::DashLine));
    p.drawLine(m_canvas.toScreen(m_press), m_canvas.toScreen(m_current));
    p.restore();
}

// --- Hülle -----------------------------------------------------------------------------------

Envelope::Mode EnvelopeTool::mode()
{
    return Envelope::Mode(std::clamp(Options::instance().integer(QStringLiteral("ws/tools/envelope/mode")), 0, 3));
}

QList<std::pair<QString, QPainterPath>> EnvelopeTool::presets()
{
    QList<std::pair<QString, QPainterPath>> list;
    auto add = [&list](const QString& name, const std::function<void(QPainterPath&)>& draw) {
        QPainterPath p;
        draw(p);
        p.closeSubpath();
        list.append({name, p});
    };
    add(QObject::tr("Bogen nach oben"), [](QPainterPath& p) {
        p.moveTo(0, 0.35);
        p.quadTo(0.5, -0.3, 1, 0.35);
        p.lineTo(1, 1);
        p.quadTo(0.5, 0.45, 0, 1);
    });
    add(QObject::tr("Bogen nach unten"), [](QPainterPath& p) {
        p.moveTo(0, 0);
        p.quadTo(0.5, 0.55, 1, 0);
        p.lineTo(1, 0.65);
        p.quadTo(0.5, 1.3, 0, 0.65);
    });
    add(QObject::tr("Wölbung"), [](QPainterPath& p) {
        p.moveTo(0, 0.2);
        p.quadTo(0.5, -0.2, 1, 0.2);
        p.lineTo(1, 0.8);
        p.quadTo(0.5, 1.2, 0, 0.8);
    });
    add(QObject::tr("Taille"), [](QPainterPath& p) {
        p.moveTo(0, 0);
        p.quadTo(0.5, 0.4, 1, 0);
        p.lineTo(1, 1);
        p.quadTo(0.5, 0.6, 0, 1);
    });
    add(QObject::tr("Fahne"), [](QPainterPath& p) {
        p.moveTo(0, 0.1);
        p.cubicTo(0.33, -0.15, 0.66, 0.35, 1, 0.1);
        p.lineTo(1, 0.9);
        p.cubicTo(0.66, 1.15, 0.33, 0.65, 0, 0.9);
    });
    add(QObject::tr("Perspektive"), [](QPainterPath& p) {
        p.moveTo(0.25, 0);
        p.lineTo(0.75, 0);
        p.lineTo(1, 1);
        p.lineTo(0, 1);
    });
    add(QObject::tr("Perspektive umgekehrt"), [](QPainterPath& p) {
        p.moveTo(0, 0);
        p.lineTo(1, 0);
        p.lineTo(0.75, 1);
        p.lineTo(0.25, 1);
    });
    add(QObject::tr("Nach rechts kleiner"), [](QPainterPath& p) {
        p.moveTo(0, 0);
        p.lineTo(1, 0.25);
        p.lineTo(1, 0.75);
        p.lineTo(0, 1);
    });
    add(QObject::tr("Kreis"), [](QPainterPath& p) { p.addEllipse(QRectF(0, 0, 1, 1)); });
    add(QObject::tr("Raute"), [](QPainterPath& p) {
        p.moveTo(0.5, 0);
        p.lineTo(1, 0.5);
        p.lineTo(0.5, 1);
        p.lineTo(0, 0.5);
    });
    add(QObject::tr("Kissen"), [](QPainterPath& p) {
        p.moveTo(0, 0);
        p.quadTo(0.5, 0.2, 1, 0);
        p.quadTo(0.8, 0.5, 1, 1);
        p.quadTo(0.5, 0.8, 0, 1);
        p.quadTo(0.2, 0.5, 0, 0);
    });
    add(QObject::tr("Fass"), [](QPainterPath& p) {
        p.moveTo(0.1, 0);
        p.lineTo(0.9, 0);
        p.quadTo(1.25, 0.5, 0.9, 1);
        p.lineTo(0.1, 1);
        p.quadTo(-0.25, 0.5, 0.1, 0);
    });
    add(QObject::tr("Wappen"), [](QPainterPath& p) {
        p.moveTo(0, 0);
        p.lineTo(1, 0);
        p.lineTo(1, 0.55);
        p.quadTo(0.95, 0.9, 0.5, 1);
        p.quadTo(0.05, 0.9, 0, 0.55);
    });
    add(QObject::tr("Herz"), [](QPainterPath& p) {
        p.moveTo(0.5, 0.25);
        p.cubicTo(0.35, -0.05, 0, 0.0, 0.0, 0.3);
        p.cubicTo(0.0, 0.6, 0.35, 0.75, 0.5, 1.0);
        p.cubicTo(0.65, 0.75, 1.0, 0.6, 1.0, 0.3);
        p.cubicTo(1.0, 0.0, 0.65, -0.05, 0.5, 0.25);
    });
    add(QObject::tr("Welle"), [](QPainterPath& p) {
        p.moveTo(0, 0.15);
        p.cubicTo(0.25, -0.1, 0.25, 0.4, 0.5, 0.15);
        p.cubicTo(0.75, -0.1, 0.75, 0.4, 1, 0.15);
        p.lineTo(1, 0.85);
        p.cubicTo(0.75, 1.1, 0.75, 0.6, 0.5, 0.85);
        p.cubicTo(0.25, 1.1, 0.25, 0.6, 0, 0.85);
    });
    return list;
}

QIcon EnvelopeTool::presetIcon(const QPainterPath& shape)
{
    QPixmap pm(48, 36);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF b = shape.boundingRect();
    const QRectF to(4, 4, 40, 28);
    const QTransform fit = QTransform::fromTranslate(-b.left(), -b.top()) * QTransform::fromScale(to.width() / b.width(), to.height() / b.height())
                           * QTransform::fromTranslate(to.left(), to.top());
    p.setPen(QPen(QColor(0x1f, 0x5f, 0xae), 1.2));
    p.setBrush(QColor(0x9c, 0xc3, 0xef));
    p.drawPath(fit.map(shape));
    return QIcon(pm);
}

void EnvelopeTool::activate()
{
    m_drag = -1;
    refresh();
    m_canvas.setCursor(Qt::CrossCursor);
    m_canvas.update();
}

void EnvelopeTool::deactivate()
{
    m_target = 0;
    m_drag = -1;
}

void EnvelopeTool::refresh()
{
    m_target = 0;
    const QList<const DrawObject*> sel = m_canvas.editor().selectedObjects();
    if (sel.size() != 1 || !Editor::canEnvelope(*sel.first()))
        return;
    const DrawObject* o = sel.first();
    m_base = o->envelopes();
    if (!m_base.isEmpty()) {
        m_env = m_base.takeLast();
    }
    m_local = o->localPath();
    for (const Envelope& e : std::as_const(m_base))
        m_local = e.map(m_local);
    if (o->envelopes().isEmpty()) {
        const QRectF box = m_local.boundingRect();
        if (box.width() <= 1e-6 || box.height() <= 1e-6)
            return;
        m_env = Envelope::rectangle(box, mode());
    }
    m_toPage = o->transform();
    m_target = o->id();
}

QList<QPointF> EnvelopeTool::pageHandles() const
{
    QList<QPointF> list;
    for (const QPointF& h : m_env.handles())
        list << m_toPage.map(h);
    return list;
}

void EnvelopeTool::moveHandle(int handle, const QPointF& doc)
{
    refresh();
    if (!m_target || handle < 0 || handle > 7)
        return;
    m_env.mode = mode();
    m_env.moveHandle(handle, m_toPage.inverted().map(doc));
    m_canvas.editor().setObjectEnvelopes(m_target, m_base + QList<Envelope>{m_env}, QObject::tr("Hülle"));
    m_canvas.update();
}

void EnvelopeTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    refresh();
    const QList<QPointF> handles = pageHandles();
    for (int i = 0; m_target && i < handles.size(); ++i)
        if (nearHandle(m_canvas.toScreen(handles[i]), m_canvas.toScreen(doc))) {
            m_drag = i;
            m_start = m_env;
            m_start.mode = mode();
            return;
        }
    // Anderes Objekt anklicken: dessen Hülle zeigen
    pickTarget(m_canvas, doc);
    refresh();
    m_canvas.update();
}

void EnvelopeTool::mouseMove(QMouseEvent*, const QPointF& doc)
{
    if (m_drag < 0)
        return;
    m_env = m_start;
    m_env.moveHandle(m_drag, m_toPage.inverted().map(doc));
    m_canvas.update();
}

void EnvelopeTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton || m_drag < 0)
        return;
    const int h = m_drag;
    m_drag = -1;
    m_env = m_start;
    m_env.moveHandle(h, m_toPage.inverted().map(doc));
    m_canvas.editor().setObjectEnvelopes(m_target, m_base + QList<Envelope>{m_env}, QObject::tr("Hülle"));
    m_canvas.update();
}

bool EnvelopeTool::keyPress(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        m_canvas.setTool(ToolId::Pick);
        return true;
    }
    return false;
}

void EnvelopeTool::paintOverlay(QPainter& p)
{
    if (m_drag < 0)
        refresh(); // nach Rückgängig oder geänderter Auswahl
    if (!m_target)
        return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const QTransform toScreen = m_toPage * m_canvas.viewTransform();
    if (m_drag >= 0) {
        p.setPen(QPen(Canvas::previewColor(), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawPath(toScreen.map(m_env.map(m_local)));
    }
    // Hüllenumriss gestrichelt rot
    p.setPen(QPen(QColor(0xd0, 0x30, 0x30), 1.0, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawPath(toScreen.map(m_env.outline()));
    for (const QPointF& h : m_env.handles())
        drawHandle(p, toScreen.map(h));
    p.restore();
}

} // namespace vek

#include "InteractiveTools.h"

#include "Canvas.h"
#include "Editor.h"
#include "Options.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QRadialGradient>
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

ExtrusionTrackball::ExtrusionTrackball(QWidget* parent) : QWidget(parent)
{
    setFixedSize(30, 30);
    setCursor(Qt::SizeAllCursor);
}

void ExtrusionTrackball::setRotation(double x, double y, double z)
{
    if (m_dragging)
        return;
    m_x = x;
    m_y = y;
    m_z = z;
    update();
}

void ExtrusionTrackball::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
    QRadialGradient g(r.center() - QPointF(r.width() * 0.2, r.height() * 0.2), r.width() * 0.75);
    g.setColorAt(0, QColor(0xfa, 0xfa, 0xfa));
    g.setColorAt(1, QColor(0x9a, 0xa8, 0xb8));
    p.setPen(QPen(QColor(0x50, 0x58, 0x60), 1));
    p.setBrush(g);
    p.drawEllipse(r);
    // Vorderfläche als gedrehtes Quadrat (rot, wenn sie zum Betrachter zeigt)
    Extrusion e;
    e.rotX = m_x;
    e.rotY = m_y;
    e.rotZ = m_z;
    double m[3][3];
    e.rotation(m);
    const double k = r.width() * 0.3;
    QPolygonF face;
    for (const QPointF& c : {QPointF(-1, -1), QPointF(1, -1), QPointF(1, 1), QPointF(-1, 1)})
        face << r.center() + QPointF(m[0][0] * c.x() + m[0][1] * c.y(), m[1][0] * c.x() + m[1][1] * c.y()) * k;
    const bool facing = m[2][2] > 0; // Normale (0,0,-1) zeigt nach vorn
    p.setPen(QPen(facing ? QColor(0xc0, 0x20, 0x20) : QColor(0x60, 0x60, 0x60), 1.5));
    p.setBrush(facing ? QColor(0xe0, 0x50, 0x40, 160) : QColor(0x90, 0x90, 0x90, 120));
    p.drawPolygon(face);
}

void ExtrusionTrackball::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton)
        return;
    m_dragging = true;
    m_press = e->position().toPoint();
    m_startX = m_x;
    m_startY = m_y;
    m_startZ = m_z;
}

void ExtrusionTrackball::mouseMoveEvent(QMouseEvent* e)
{
    if (!m_dragging)
        return;
    const QPoint d = e->position().toPoint() - m_press;
    auto wrap = [](double a) { return std::remainder(a, 360.0); };
    if (e->modifiers() & Qt::ShiftModifier) {
        m_z = wrap(m_startZ + d.x() * kDegreesPerPixel);
    } else {
        // Nach oben ziehen kippt die Oberkante nach hinten, nach rechts die rechte Seite
        m_x = wrap(m_startX - d.y() * kDegreesPerPixel);
        m_y = wrap(m_startY + d.x() * kDegreesPerPixel);
    }
    update();
}

void ExtrusionTrackball::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton || !m_dragging)
        return;
    mouseMoveEvent(e);
    m_dragging = false;
    if (e->position().toPoint() != m_press)
        emit rotationChanged(m_x, m_y, m_z);
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

DistortTool::ZipperOptions DistortTool::zipperOptions()
{
    const Options& opt = Options::instance();
    ZipperOptions z;
    z.smooth = opt.flag(QStringLiteral("ws/tools/distort/smooth"));
    z.random = opt.flag(QStringLiteral("ws/tools/distort/random"));
    z.local = opt.flag(QStringLiteral("ws/tools/distort/local"));
    return z;
}

QList<SubPath> DistortTool::distort(const QList<SubPath>& paths, Mode mode, double amount, int frequency, const QPointF& c,
                                    const ZipperOptions& zipper)
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
    // Abtastung: Wirbel fein über die abgeflachte Linie; Zickzack mit "Frequenz" Zacken je
    // Segment der Kurve (Knoten zu Knoten), gleichmässig nach Bogenlänge. Vorher galt die Frequenz
    // je abgeflachtem Polygonstück: ein Kreis bekam hunderte Zacken.
    QList<std::pair<QPolygonF, bool>> samples;
    if (mode == Mode::Zipper) {
        const int perSeg = std::max(2, frequency);
        for (const SubPath& src : paths) {
            const int n = int(src.nodes.size());
            const int segs = src.closed ? n : n - 1;
            if (n < 2)
                continue;
            QPolygonF dense;
            for (int k = 0; k < segs; ++k) {
                const CurveNode& a = src.nodes[k];
                const CurveNode& b = src.nodes[(k + 1) % n];
                auto at = [&](double t) {
                    if (!b.curveIn)
                        return a.point + (b.point - a.point) * t;
                    const double u = 1.0 - t;
                    return u * u * u * a.point + 3 * u * u * t * b.c1 + 3 * u * t * t * b.c2 + t * t * t * b.point;
                };
                // Bogenlänge tabellieren, dann gleich lange Stücke
                constexpr int kFine = 64;
                QList<double> acc{0.0};
                QPointF last = at(0.0);
                for (int f = 1; f <= kFine; ++f) {
                    const QPointF q = at(double(f) / kFine);
                    acc << acc.last() + QLineF(last, q).length();
                    last = q;
                }
                for (int j = 0; j < perSeg; ++j) {
                    const double target = acc.last() * j / perSeg;
                    int f = 0;
                    while (f + 1 < acc.size() && acc[f + 1] < target)
                        ++f;
                    const double span = f + 1 < acc.size() ? acc[f + 1] - acc[f] : 0.0;
                    const double t = (f + (span > 1e-12 ? (target - acc[f]) / span : 0.0)) / kFine;
                    dense << at(t);
                }
            }
            if (!src.closed)
                dense << src.nodes.last().point;
            samples << std::make_pair(dense, src.closed);
        }
    } else {
        for (const QPolygonF& poly : path.toSubpathPolygons()) {
            QPolygonF dense;
            for (int i = 0; i + 1 < poly.size(); ++i)
                for (int k = 0; k < 8; ++k)
                    dense << poly[i] + (poly[i + 1] - poly[i]) * (double(k) / 8);
            if (!poly.isClosed() && !poly.isEmpty())
                dense << poly.last();
            samples << std::make_pair(dense, poly.isClosed());
        }
    }
    for (const auto& [dense, closed] : std::as_const(samples)) {
        SubPath sp;
        sp.closed = closed;
        QList<QPointF> tangents; // Richtung der unverzerrten Linie (für glatte Wellen)
        quint32 rng = zipper.seed * 2654435761u + quint32(out.size()) * 40503u + 1u;
        for (int i = 0; i < dense.size(); ++i) {
            QPointF p = dense[i];
            if (mode == Mode::Zipper) {
                const int n = int(dense.size());
                const QPointF next = dense[sp.closed ? (i + 1) % n : std::min(i + 1, n - 1)];
                const QPointF prev = dense[sp.closed ? (i - 1 + n) % n : std::max(i - 1, 0)];
                QPointF t = next - prev;
                const double len = std::max(1e-9, std::hypot(t.x(), t.y()));
                const QPointF nrm(-t.y() / len, t.x() / len);
                tangents << t / len;
                double a = amount;
                if (zipper.random) {
                    rng = rng * 1664525u + 1013904223u;
                    a *= 0.2 + 0.8 * double(rng >> 8) / double(1u << 24);
                }
                if (zipper.local)
                    a *= std::max(0.0, 1.0 - QLineF(c, p).length() / rmax);
                p += nrm * a * ((i % 2) ? 1.0 : -1.0);
            } else {
                const double r = QLineF(c, p).length();
                const double a = qDegreesToRadians(amount) * (1.0 - r / rmax);
                const QPointF d = p - c;
                p = c + QPointF(d.x() * std::cos(a) - d.y() * std::sin(a), d.x() * std::sin(a) + d.y() * std::cos(a));
            }
            sp.nodes << CurveNode{p};
        }
        if (mode == Mode::Zipper && zipper.smooth && sp.nodes.size() > 1) {
            // Wellen: Knoten auf den Spitzen, Tangenten parallel zur Linie; 0.3642 der Strecke
            // ergibt eine sinusähnliche Halbwelle
            const int n = int(sp.nodes.size());
            const int segs = sp.closed ? n : n - 1;
            for (int k = 0; k < segs; ++k) {
                const int j = (k + 1) % n;
                const double h = 0.3642 * QLineF(dense[k], dense[j]).length();
                CurveNode& to = sp.nodes[j];
                to.curveIn = true;
                to.c1 = sp.nodes[k].point + tangents[k] * h;
                to.c2 = to.point - tangents[j] * h;
                to.kind = NodeKind::Smooth;
            }
            if (!sp.closed)
                sp.nodes.first().kind = NodeKind::Smooth;
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
        distort(m_original, mode(), amountFor(doc), Options::instance().integer(QStringLiteral("ws/tools/distort/frequency")),
                mode() == Mode::Zipper ? m_press : m_center, zipperOptions());
    m_canvas.editor().replaceObjectWithCurve(m_target, result, QObject::tr("Verzerren"));
    m_canvas.update();
}

void DistortTool::paintOverlay(QPainter& p)
{
    if (!m_dragging)
        return;
    const QList<SubPath> result =
        distort(m_original, mode(), amountFor(m_current), Options::instance().integer(QStringLiteral("ws/tools/distort/frequency")),
                mode() == Mode::Zipper ? m_press : m_center, zipperOptions());
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
        m_env.mapping = Envelope::Mapping(std::clamp(Options::instance().integer(QStringLiteral("ws/tools/envelope/mapping")), 0,
                                                     int(Envelope::Mapping::Vertical)));
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

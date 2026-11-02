#include "MeshTool.h"

#include "Canvas.h"
#include "ConnectorObject.h"
#include "DimensionObject.h"
#include "GroupObject.h"
#include "ImageObject.h"
#include "Options.h"
#include "TableObject.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include <cmath>

namespace vek {

namespace {

constexpr double kHandle = 4.0; // halbe Kantenlänge der Knotenquadrate, px

// Farbe an einer Einheitsstelle (u, v) des alten Netzes, für das Neuaufteilen
Color sample(const MeshFill& m, double u, double v)
{
    const double x = std::clamp(u, 0.0, 1.0) * m.columns, y = std::clamp(v, 0.0, 1.0) * m.rows;
    const int c = std::min(m.columns - 1, int(x)), r = std::min(m.rows - 1, int(y));
    const double fu = x - c, fv = y - r;
    const Color a = m.colors[m.index(r, c)], b = m.colors[m.index(r, c + 1)];
    const Color d = m.colors[m.index(r + 1, c)], e = m.colors[m.index(r + 1, c + 1)];
    const auto mix = [&](auto ch) {
        return std::uint8_t(std::lround((1 - fu) * (1 - fv) * ch(a) + fu * (1 - fv) * ch(b) + (1 - fu) * fv * ch(d) + fu * fv * ch(e)));
    };
    return Color(mix([](const Color& k) { return double(k.r); }), mix([](const Color& k) { return double(k.g); }),
                 mix([](const Color& k) { return double(k.b); }), mix([](const Color& k) { return double(k.a); }));
}

} // namespace

bool MeshTool::canHaveMesh(const DrawObject& o)
{
    return !node_cast<GroupObject>(&o) && !node_cast<ImageObject>(&o) && !node_cast<TableObject>(&o)
           && !node_cast<DimensionObject>(&o) && !node_cast<ConnectorObject>(&o);
}

void MeshTool::activate()
{
    m_canvas.setCursor(Qt::CrossCursor);
}

void MeshTool::deactivate()
{
    m_drag = Drag::None;
    m_preview.reset();
    m_nodes.clear();
}

const DrawObject* MeshTool::target() const
{
    const QList<const DrawObject*> objects = m_canvas.editor().selectedObjects();
    return objects.size() == 1 && canHaveMesh(*objects.first()) ? objects.first() : nullptr;
}

QPointF MeshTool::toUnit(const DrawObject& o, const QPointF& doc) const
{
    const QRectF b = o.localPath().boundingRect();
    const QPointF local = o.transform().inverted().map(doc);
    return QPointF(b.width() != 0 ? (local.x() - b.left()) / b.width() : 0.5,
                   b.height() != 0 ? (local.y() - b.top()) / b.height() : 0.5);
}

QPointF MeshTool::fromUnit(const DrawObject& o, const QPointF& unit) const
{
    const QRectF b = o.localPath().boundingRect();
    return o.transform().map(QPointF(b.left() + unit.x() * b.width(), b.top() + unit.y() * b.height()));
}

int MeshTool::nodeAt(const DrawObject& o, const MeshFill& mesh, const QPointF& doc) const
{
    const QPointF screen = m_canvas.toScreen(doc);
    int best = -1;
    double bestDist = kHandle + 2.0;
    for (int i = 0; i < mesh.points.size(); ++i) {
        const QPointF s = m_canvas.toScreen(fromUnit(o, mesh.points[i]));
        const double d = std::max(std::abs(s.x() - screen.x()), std::abs(s.y() - screen.y()));
        if (d <= bestDist) {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

void MeshTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    Editor& ed = m_canvas.editor();
    m_pressDoc = m_currentDoc = doc;
    const DrawObject* o = target();
    const double tolerance = m_canvas.pixelsToDoc(3.0);
    if (o && o->style().fill.isMesh()) {
        const MeshFill& mesh = o->style().fill.mesh;
        const int node = nodeAt(*o, mesh, doc);
        if (node >= 0) {
            if (event->modifiers() & Qt::ShiftModifier) {
                if (m_nodes.contains(node))
                    m_nodes.removeAll(node);
                else
                    m_nodes << node;
            } else if (!m_nodes.contains(node)) {
                m_nodes = {node};
            }
            m_drag = m_nodes.contains(node) ? Drag::Nodes : Drag::None;
            m_canvas.update();
            return;
        }
        if (o->hitTest(doc, tolerance, true)) {
            if (!(event->modifiers() & Qt::ShiftModifier))
                m_nodes.clear();
            m_drag = Drag::Marquee;
            m_canvas.update();
            return;
        }
    } else if (o && o->hitTest(doc, tolerance, true) && !ed.isLocked(o->id())) {
        // Netz anlegen, Grundfarbe aus der bisherigen Füllung
        const Options& opt = Options::instance();
        const Color base = o->style().fill.representativeColor().value_or(Color(255, 255, 255));
        const MeshFill mesh = MeshFill::grid(opt.integer(QStringLiteral("ws/tools/mesh/cols")),
                                             opt.integer(QStringLiteral("ws/tools/mesh/rows")), base);
        ed.setObjectFill(o->id(), Fill::meshFill(mesh), QObject::tr("Maschenfüllung"));
        m_nodes.clear();
        m_canvas.update();
        return;
    }
    // Anderes Objekt wählen
    m_nodes.clear();
    const DrawObject* hit = ed.objectAt(doc, tolerance);
    ed.setSelection(hit ? QList<NodeId>{hit->id()} : QList<NodeId>{});
    m_canvas.update();
}

void MeshTool::mouseMove(QMouseEvent*, const QPointF& doc)
{
    if (m_drag == Drag::None)
        return;
    m_currentDoc = doc;
    const DrawObject* o = target();
    if (m_drag == Drag::Nodes && o && o->style().fill.isMesh()) {
        Fill f = o->style().fill;
        const QPointF delta = toUnit(*o, doc) - toUnit(*o, m_pressDoc);
        for (int i : std::as_const(m_nodes))
            f.mesh.points[i] += delta;
        m_preview = f;
    }
    m_canvas.update();
}

void MeshTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    if (m_drag == Drag::Nodes)
        mouseMove(event, doc); // Vorschau bis zur Loslass-Stelle
    const Drag drag = m_drag;
    m_drag = Drag::None;
    const DrawObject* o = target();
    if (drag == Drag::Nodes && m_preview && o) {
        const Fill f = *m_preview;
        m_preview.reset();
        if (std::hypot(doc.x() - m_pressDoc.x(), doc.y() - m_pressDoc.y()) > m_canvas.pixelsToDoc(1.0))
            m_canvas.editor().setObjectFill(o->id(), f, QObject::tr("Maschenknoten verschieben"));
    } else if (drag == Drag::Marquee && o && o->style().fill.isMesh()) {
        // Auswahlrahmen über Knoten
        const QRectF r = QRectF(m_canvas.toScreen(m_pressDoc), m_canvas.toScreen(doc)).normalized();
        const MeshFill& mesh = o->style().fill.mesh;
        for (int i = 0; i < mesh.points.size(); ++i)
            if (r.contains(m_canvas.toScreen(fromUnit(*o, mesh.points[i]))) && !m_nodes.contains(i))
                m_nodes << i;
    }
    m_preview.reset();
    m_canvas.update();
}

bool MeshTool::keyPress(QKeyEvent* e)
{
    const DrawObject* o = target();
    if (!o || !o->style().fill.isMesh())
        return false;
    if (e->key() == Qt::Key_Escape && !m_nodes.isEmpty()) {
        m_nodes.clear();
        m_canvas.update();
        return true;
    }
    if (e->matches(QKeySequence::SelectAll)) {
        m_nodes.clear();
        for (int i = 0; i < o->style().fill.mesh.nodeCount(); ++i)
            m_nodes << i;
        m_canvas.update();
        return true;
    }
    if (e->key() == Qt::Key_Delete) {
        // Netz löschen: zurück zur gleichmässigen Füllung in der ersten Knotenfarbe
        m_nodes.clear();
        m_canvas.editor().setObjectFill(o->id(), Fill::solid(o->style().fill.representativeColor().value_or(Color(255, 255, 255))),
                                        QObject::tr("Maschenfüllung löschen"));
        return true;
    }
    return false;
}

bool MeshTool::paletteColor(const std::optional<Color>& color, bool fill)
{
    const DrawObject* o = target();
    if (!fill || !color || !o || !o->style().fill.isMesh())
        return false;
    Fill f = o->style().fill;
    if (m_nodes.isEmpty()) {
        for (Color& c : f.mesh.colors)
            c = *color;
    } else {
        for (int i : std::as_const(m_nodes))
            if (i >= 0 && i < f.mesh.colors.size())
                f.mesh.colors[i] = *color;
    }
    m_canvas.editor().setObjectFill(o->id(), f, QObject::tr("Maschenfarbe"));
    return true;
}

void MeshTool::setGrid(int columns, int rows)
{
    const DrawObject* o = target();
    if (!o || !o->style().fill.isMesh())
        return;
    const MeshFill& old = o->style().fill.mesh;
    if (old.columns == columns && old.rows == rows)
        return;
    MeshFill mesh = MeshFill::grid(columns, rows, Color(255, 255, 255));
    for (int r = 0; r <= mesh.rows; ++r)
        for (int c = 0; c <= mesh.columns; ++c)
            mesh.colors[mesh.index(r, c)] = sample(old, double(c) / mesh.columns, double(r) / mesh.rows);
    m_nodes.clear();
    m_canvas.editor().setObjectFill(o->id(), Fill::meshFill(mesh), QObject::tr("Maschenfüllung"));
}

void MeshTool::paintOverlay(QPainter& p)
{
    const DrawObject* o = target();
    if (!o)
        return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const QTransform view = m_canvas.viewTransform();
    if (!o->style().fill.isMesh()) {
        // Vorschau des Netzes, das ein Klick anlegen würde
        const Options& opt = Options::instance();
        const MeshFill mesh = MeshFill::grid(opt.integer(QStringLiteral("ws/tools/mesh/cols")),
                                             opt.integer(QStringLiteral("ws/tools/mesh/rows")), Color());
        p.setPen(QPen(Canvas::previewColor(), 1.0, Qt::DashLine));
        for (int r = 0; r <= mesh.rows; ++r)
            p.drawLine(m_canvas.toScreen(fromUnit(*o, mesh.points[mesh.index(r, 0)])),
                       m_canvas.toScreen(fromUnit(*o, mesh.points[mesh.index(r, mesh.columns)])));
        for (int c = 0; c <= mesh.columns; ++c)
            p.drawLine(m_canvas.toScreen(fromUnit(*o, mesh.points[mesh.index(0, c)])),
                       m_canvas.toScreen(fromUnit(*o, mesh.points[mesh.index(mesh.rows, c)])));
        p.restore();
        return;
    }
    const Fill f = m_preview ? *m_preview : o->style().fill;
    if (m_preview) {
        QBrush brush = f.brush(o->localPath().boundingRect());
        brush.setTransform(brush.transform() * o->transform() * view);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.fillPath(view.map(o->path()), brush);
    }
    const MeshFill& mesh = f.mesh;
    auto at = [&](int r, int c) { return m_canvas.toScreen(fromUnit(*o, mesh.points[mesh.index(r, c)])); };
    p.setPen(QPen(QColor(0x30, 0x30, 0x30), 1.0, Qt::DashLine));
    for (int r = 0; r <= mesh.rows; ++r)
        for (int c = 0; c < mesh.columns; ++c)
            p.drawLine(at(r, c), at(r, c + 1));
    for (int c = 0; c <= mesh.columns; ++c)
        for (int r = 0; r < mesh.rows; ++r)
            p.drawLine(at(r, c), at(r + 1, c));
    p.setRenderHint(QPainter::Antialiasing, false);
    for (int i = 0; i < mesh.points.size(); ++i) {
        const QPointF s = m_canvas.toScreen(fromUnit(*o, mesh.points[i]));
        p.setPen(QPen(Qt::black, 1.0));
        p.setBrush(m_nodes.contains(i) ? QColor(Qt::black) : QColor(Qt::white));
        p.drawRect(QRectF(s.x() - kHandle, s.y() - kHandle, 2 * kHandle, 2 * kHandle));
    }
    if (m_drag == Drag::Marquee) {
        p.setPen(QPen(Canvas::previewColor(), 1.0, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(m_canvas.toScreen(m_pressDoc), m_canvas.toScreen(m_currentDoc)).normalized());
    }
    p.restore();
}

} // namespace vek

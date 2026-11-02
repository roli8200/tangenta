#include "ShapeTool.h"

#include "Canvas.h"
#include "ImageObject.h"

#include <QMenu>

#include <cmath>

namespace vek {

namespace {

constexpr double kNodeRadius = 5.0;    // px, Trefferbereich eines Knotens
constexpr double kControlRadius = 5.0; // px
constexpr double kSegmentRadius = 4.0; // px
constexpr double kDragThreshold = 3.0; // px

double distance(const QPointF& a, const QPointF& b)
{
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

} // namespace

void ShapeTool::activate()
{
    m_canvas.setCursor(Qt::ArrowCursor);
    validateSelection();
}

void ShapeTool::deactivate()
{
    m_drag = Drag::None;
    m_preview.reset();
    m_selected.clear();
}

const CurveObject* ShapeTool::curve() const
{
    // Gesperrte Kurven lassen sich nicht bearbeiten
    const Editor& ed = m_canvas.editor();
    const QList<const DrawObject*> objects = ed.selectedObjects();
    if (objects.size() != 1 || ed.isLocked(objects.first()->id()))
        return nullptr;
    if (const auto* image = node_cast<ImageObject>(objects.first())) {
        // Stellvertreter nur neu bauen, wenn sich das Bild geändert hat: Aufrufer halten den Zeiger
        if (!m_frameProxy || m_frameProxyOf != image->id() || m_frameProxy->subPaths() != image->frame()
            || m_frameProxy->transform() != image->transform()) {
            m_frameProxy = std::make_unique<CurveObject>(image->frame());
            m_frameProxy->setTransform(image->transform());
            m_frameProxyOf = image->id();
        }
        return m_frameProxy.get();
    }
    return node_cast<CurveObject>(objects.first());
}

void ShapeTool::modifyTarget(const std::function<void(CurveObject&)>& fn, const QString& label)
{
    Editor& ed = m_canvas.editor();
    if (ed.document().findAs<ImageObject>(m_object)) {
        const CurveObject* proxy = curve();
        if (!proxy)
            return;
        CurveObject frame(proxy->subPaths());
        fn(frame);
        ed.setImageFrame(m_object, frame.subPaths(), label);
        return;
    }
    ed.modifyCurve(m_object, fn, label);
}

void ShapeTool::validateSelection() const
{
    const CurveObject* c = curve();
    // Beim Bild ist es das Bild selbst, nicht der Stellvertreter für den Rahmen
    const NodeId id = !c ? InvalidNodeId : c == m_frameProxy.get() ? m_frameProxyOf : c->id();
    if (id != m_object) {
        m_object = id;
        m_selected.clear();
    }
    if (c)
        m_selected.removeIf([c](NodeRef r) { return !c->isValid(r); });
    else
        m_selected.clear();
}

void ShapeTool::setSelectedNodes(const QList<NodeRef>& nodes)
{
    validateSelection();
    m_selected = nodes;
    notify();
}

void ShapeTool::notify()
{
    m_canvas.notifyToolState();
    m_canvas.update();
}

QTransform ShapeTool::toScreen() const
{
    const CurveObject* c = curve();
    return c ? c->transform() * m_canvas.viewTransform() : QTransform();
}

std::optional<NodeRef> ShapeTool::nodeAt(const QPointF& screen) const
{
    const CurveObject* c = curve();
    if (!c)
        return std::nullopt;
    const QTransform t = toScreen();
    std::optional<NodeRef> best;
    double bestDistance = kNodeRadius;
    const QList<SubPath>& paths = c->subPaths();
    for (int p = 0; p < paths.size(); ++p) {
        for (int n = 0; n < paths[p].nodes.size(); ++n) {
            const double d = distance(t.map(paths[p].nodes[n].point), screen);
            if (d <= bestDistance) {
                bestDistance = d;
                best = NodeRef{p, n};
            }
        }
    }
    return best;
}

QList<ShapeTool::ControlRef> ShapeTool::visibleControls(const CurveObject& c) const
{
    // An jedem gewählten Knoten die Steuerpunkte der anliegenden Kurvensegmente
    QList<ControlRef> out;
    auto addSegment = [&](NodeRef end) {
        if (!c.hasSegmentInto(end) || !c.node(end).curveIn)
            return;
        const ControlRef in{end, true};
        const ControlRef outOfPrev{c.previous(end), false};
        for (const ControlRef& r : {in, outOfPrev}) {
            const bool known = std::any_of(out.begin(), out.end(), [&r](const ControlRef& o) {
                return o.owner == r.owner && o.incoming == r.incoming;
            });
            if (!known)
                out.append(r);
        }
    };
    for (NodeRef r : m_selected) {
        if (!c.isValid(r))
            continue;
        addSegment(r);
        if (const NodeRef nx = c.next(r); nx.isValid())
            addSegment(nx);
    }
    return out;
}

QPointF ShapeTool::controlPoint(const CurveObject& c, const ControlRef& ref) const
{
    if (ref.incoming)
        return c.node(ref.owner).c2;
    return c.node(c.next(ref.owner)).c1;
}

std::optional<ShapeTool::ControlRef> ShapeTool::controlAt(const QPointF& screen) const
{
    const CurveObject* c = curve();
    if (!c)
        return std::nullopt;
    const QTransform t = toScreen();
    for (const ControlRef& r : visibleControls(*c)) {
        if (distance(t.map(controlPoint(*c, r)), screen) <= kControlRadius)
            return r;
    }
    return std::nullopt;
}

std::optional<ShapeTool::SegmentHit> ShapeTool::segmentAt(const QPointF& screen) const
{
    const CurveObject* c = curve();
    if (!c)
        return std::nullopt;
    const QTransform t = toScreen();
    bool invertible = false;
    const QPointF local = t.inverted(&invertible).map(screen);
    if (!invertible)
        return std::nullopt;
    std::optional<SegmentHit> best;
    double bestDistance = kSegmentRadius;
    const QList<SubPath>& paths = c->subPaths();
    for (int p = 0; p < paths.size(); ++p) {
        for (int n = 0; n < paths[p].nodes.size(); ++n) {
            const NodeRef r{p, n};
            if (!c->hasSegmentInto(r))
                continue;
            const double tt = c->nearestOnSegment(r, local, nullptr);
            const double d = distance(t.map(c->pointOnSegment(r, tt)), screen);
            if (d <= bestDistance) {
                bestDistance = d;
                best = SegmentHit{r, tt};
            }
        }
    }
    return best;
}

void ShapeTool::beginPreview()
{
    const CurveObject* c = curve();
    m_preview = std::make_unique<CurveObject>(c->subPaths());
    m_preview->setTransform(c->transform());
    m_moved = false;
}

void ShapeTool::commitPreview(const QString& label)
{
    if (m_preview && m_moved) {
        const QList<SubPath> paths = m_preview->subPaths();
        modifyTarget([&paths](CurveObject& c) { c.setSubPaths(paths); }, label);
    }
    m_preview.reset();
}

void ShapeTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    validateSelection();
    Editor& ed = m_canvas.editor();
    const QPointF pos = event->position();
    m_pressDoc = doc;
    m_pressScreen = m_currentScreen = pos;
    m_drag = Drag::None;
    m_moved = false;
    const bool shift = event->modifiers() & Qt::ShiftModifier;

    if (event->button() == Qt::RightButton) {
        // Knoten oder Segment unter der Maus wählen, dann das Knotenmenü zeigen
        if (const auto n = nodeAt(pos)) {
            if (!m_selected.contains(*n))
                m_selected = {*n};
        } else if (const auto s = segmentAt(pos)) {
            m_selected = {s->end};
        } else {
            return;
        }
        notify();
        QMenu menu(&m_canvas);
        fillMenu(menu);
        menu.exec(event->globalPosition().toPoint());
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;

    if (const auto ctl = controlAt(pos)) {
        m_control = *ctl;
        m_drag = Drag::Control;
        beginPreview();
        return;
    }
    if (const auto n = nodeAt(pos)) {
        if (shift) {
            if (m_selected.contains(*n))
                m_selected.removeAll(*n);
            else
                m_selected.append(*n);
        } else if (!m_selected.contains(*n)) {
            m_selected = {*n};
        }
        notify();
        if (m_selected.contains(*n)) {
            m_drag = Drag::Nodes;
            beginPreview();
        }
        return;
    }
    if (const auto s = segmentAt(pos)) {
        // Segment anklicken wählt seinen Endknoten; Ziehen verbiegt Kurvensegmente
        m_segment = *s;
        m_selected = {s->end};
        notify();
        m_drag = Drag::Segment;
        beginPreview();
        return;
    }
    // Anderes Objekt anklicken: es wird zur bearbeiteten Kurve
    if (const DrawObject* hit = ed.objectAt(doc, m_canvas.pixelsToDoc(4.0))) {
        ed.setSelection({hit->id()});
        validateSelection();
        notify();
        return;
    }
    if (curve()) {
        if (!shift)
            m_selected.clear();
        m_drag = Drag::Marquee;
        notify();
    } else {
        ed.clearSelection();
    }
}

void ShapeTool::mouseMove(QMouseEvent* event, const QPointF& doc)
{
    m_currentScreen = event->position();
    if (m_drag == Drag::None) {
        const bool overHandle = controlAt(event->position()) || nodeAt(event->position());
        m_canvas.setCursor(overHandle ? Qt::SizeAllCursor : Qt::ArrowCursor);
        return;
    }
    if (!m_moved && distance(event->position(), m_pressScreen) < kDragThreshold)
        return;
    m_moved = true;
    const CurveObject* c = curve();
    if (m_drag == Drag::Marquee || !c || !m_preview) {
        m_canvas.update();
        return;
    }
    const QTransform toLocal = c->transform().inverted();
    QPointF docDelta = doc - m_pressDoc;
    if (m_drag == Drag::Nodes && (event->modifiers() & Qt::ControlModifier)) {
        // Strg: nur waagrecht oder senkrecht
        if (std::abs(docDelta.x()) > std::abs(docDelta.y()))
            docDelta.setY(0);
        else
            docDelta.setX(0);
    }
    const QPointF localDelta = toLocal.map(m_pressDoc + docDelta) - toLocal.map(m_pressDoc);
    m_preview->setSubPaths(c->subPaths());
    switch (m_drag) {
    case Drag::Nodes:
        m_preview->moveNodes(m_selected, localDelta);
        break;
    case Drag::Control:
        m_preview->moveControl(m_control.owner, m_control.incoming, toLocal.map(doc));
        break;
    case Drag::Segment:
        m_preview->bendSegment(m_segment.end, m_segment.t, localDelta);
        break;
    default:
        break;
    }
    m_canvas.update();
}

void ShapeTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    Q_UNUSED(doc);
    if (event->button() != Qt::LeftButton)
        return;
    const Drag drag = m_drag;
    m_drag = Drag::None;
    switch (drag) {
    case Drag::Nodes:
        commitPreview(QObject::tr("Knoten verschieben"));
        break;
    case Drag::Control:
        commitPreview(QObject::tr("Steuerpunkt verschieben"));
        break;
    case Drag::Segment:
        commitPreview(QObject::tr("Kurve verbiegen"));
        break;
    case Drag::Marquee: {
        const CurveObject* c = curve();
        if (c && m_moved) {
            const QRectF area = QRectF(m_pressScreen, event->position()).normalized();
            const QTransform t = toScreen();
            const QList<SubPath>& paths = c->subPaths();
            for (int p = 0; p < paths.size(); ++p) {
                for (int n = 0; n < paths[p].nodes.size(); ++n) {
                    if (area.contains(t.map(paths[p].nodes[n].point)) && !m_selected.contains(NodeRef{p, n}))
                        m_selected.append(NodeRef{p, n});
                }
            }
        }
        m_moved = false;
        notify();
        break;
    }
    case Drag::None:
        break;
    }
    m_canvas.update();
}

void ShapeTool::mouseDoubleClick(QMouseEvent* event, const QPointF& doc)
{
    Q_UNUSED(doc);
    if (event->button() != Qt::LeftButton || !curve())
        return;
    m_drag = Drag::None;
    m_preview.reset();
    if (const auto n = nodeAt(event->position())) {
        // Doppelklick auf einen Knoten löscht ihn
        const NodeRef r = *n;
        modifyTarget([r](CurveObject& c) { c.deleteNodes({r}); },
                                      QObject::tr("Knoten löschen"));
        m_selected.clear();
    } else if (const auto s = segmentAt(event->position())) {
        // Doppelklick auf die Linie fügt dort einen Knoten ein
        NodeRef added;
        const SegmentHit hit = *s;
        modifyTarget([&](CurveObject& c) { added = c.insertNode(hit.end, hit.t); },
                                      QObject::tr("Knoten hinzufügen"));
        m_selected = {added};
    }
    validateSelection();
    notify();
}

bool ShapeTool::keyPress(QKeyEvent* event)
{
    validateSelection();
    if (event->key() == Qt::Key_Delete && !m_selected.isEmpty()) {
        run(Command::Delete);
        return true;
    }
    if (event->key() == Qt::Key_Escape && m_drag != Drag::None) {
        m_drag = Drag::None;
        m_preview.reset();
        m_canvas.update();
        return true;
    }
    return false;
}

// --- Befehle --------------------------------------------------------------------------------

QString ShapeTool::commandText(Command c)
{
    switch (c) {
    case Command::Add: return QObject::tr("Knoten hinzufügen");
    case Command::Delete: return QObject::tr("Knoten löschen");
    case Command::ToLine: return QObject::tr("In Linie umwandeln");
    case Command::ToCurve: return QObject::tr("In Kurve umwandeln");
    case Command::Cusp: return QObject::tr("Spitzer Knoten");
    case Command::Smooth: return QObject::tr("Glatter Knoten");
    case Command::Symmetric: return QObject::tr("Symmetrischer Knoten");
    case Command::Close: return QObject::tr("Kurve schliessen/öffnen");
    case Command::Reverse: return QObject::tr("Richtung umkehren");
    }
    return {};
}

QString ShapeTool::commandIcon(Command c)
{
    switch (c) {
    case Command::Add: return QStringLiteral("node-add");
    case Command::Delete: return QStringLiteral("node-delete");
    case Command::ToLine: return QStringLiteral("node-to-line");
    case Command::ToCurve: return QStringLiteral("node-to-curve");
    case Command::Cusp: return QStringLiteral("node-cusp");
    case Command::Smooth: return QStringLiteral("node-smooth");
    case Command::Symmetric: return QStringLiteral("node-symmetric");
    case Command::Close: return QStringLiteral("path-close");
    case Command::Reverse: return QStringLiteral("path-reverse");
    }
    return {};
}

bool ShapeTool::canRun(Command cmd) const
{
    validateSelection();
    const CurveObject* c = curve();
    if (!c || m_selected.isEmpty())
        return false;
    auto any = [&](auto&& pred) { return std::any_of(m_selected.begin(), m_selected.end(), pred); };
    switch (cmd) {
    case Command::Add:
        return any([c](NodeRef r) { return c->hasSegmentInto(r); });
    case Command::Delete:
        return true;
    case Command::ToLine:
        return any([c](NodeRef r) { return c->hasSegmentInto(r) && c->node(r).curveIn; });
    case Command::ToCurve:
        return any([c](NodeRef r) { return c->hasSegmentInto(r) && !c->node(r).curveIn; });
    case Command::Cusp:
    case Command::Smooth:
    case Command::Symmetric:
        // Nur Knoten mit Segmenten auf beiden Seiten haben eine Art
        return any([c](NodeRef r) { return c->hasSegmentInto(r) && c->next(r).isValid(); });
    case Command::Close:
        return any([c](NodeRef r) { return c->subPaths().at(r.path).nodes.size() > 2; });
    case Command::Reverse:
        return true;
    }
    return false;
}

void ShapeTool::run(Command cmd)
{
    if (!canRun(cmd))
        return;
    const QList<NodeRef> nodes = m_selected;
    QList<int> paths;
    for (NodeRef r : nodes) {
        if (!paths.contains(r.path))
            paths.append(r.path);
    }
    QList<NodeRef> after = nodes;
    auto edit = [&](const std::function<void(CurveObject&)>& fn) {
        modifyTarget(fn, commandText(cmd));
    };
    switch (cmd) {
    case Command::Add:
        // In der Mitte des Segments vor jedem gewählten Knoten
        edit([&](CurveObject& c) {
            QList<NodeRef> sorted = nodes;
            std::sort(sorted.begin(), sorted.end(), [](NodeRef a, NodeRef b) {
                return a.path != b.path ? a.path > b.path : a.node > b.node;
            });
            after.clear();
            for (NodeRef r : sorted) {
                if (c.hasSegmentInto(r))
                    after.append(c.insertNode(r, 0.5));
            }
        });
        break;
    case Command::Delete:
        edit([&](CurveObject& c) { c.deleteNodes(nodes); });
        after.clear();
        break;
    case Command::ToLine:
    case Command::ToCurve:
        edit([&](CurveObject& c) {
            for (NodeRef r : nodes)
                c.setSegmentCurve(r, cmd == Command::ToCurve);
        });
        break;
    case Command::Cusp:
    case Command::Smooth:
    case Command::Symmetric: {
        const NodeKind kind = cmd == Command::Cusp     ? NodeKind::Cusp
                              : cmd == Command::Smooth ? NodeKind::Smooth
                                                       : NodeKind::Symmetric;
        edit([&](CurveObject& c) {
            for (NodeRef r : nodes)
                c.setKind(r, kind);
        });
        break;
    }
    case Command::Close:
        edit([&](CurveObject& c) {
            for (int p : paths)
                c.setClosed(p, !c.subPaths().at(p).closed);
        });
        break;
    case Command::Reverse:
        edit([&](CurveObject& c) {
            for (int p : paths)
                c.reverse(p);
        });
        after.clear();
        break;
    }
    m_selected = after;
    validateSelection();
    notify();
}

void ShapeTool::fillMenu(QMenu& menu)
{
    const QList<QList<Command>> groups = {
        {Command::Add, Command::Delete},
        {Command::ToLine, Command::ToCurve},
        {Command::Cusp, Command::Smooth, Command::Symmetric},
        {Command::Close, Command::Reverse},
    };
    for (const QList<Command>& group : groups) {
        if (!menu.isEmpty())
            menu.addSeparator();
        for (Command c : group) {
            QAction* a = menu.addAction(commandText(c), [this, c] { run(c); });
            a->setEnabled(canRun(c));
        }
    }
}

// --- Darstellung ------------------------------------------------------------------------------

void ShapeTool::paintOverlay(QPainter& p)
{
    validateSelection();
    const CurveObject* original = curve();
    if (!original)
        return;
    const CurveObject& c = m_preview && m_moved ? *m_preview : *original;
    const QTransform t = toScreen();
    const QColor blue = Canvas::previewColor();
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    if (m_preview && m_moved) {
        p.setPen(QPen(blue, 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawPath(t.map(c.spinePath()));
    }

    // Steuerpunkte: gestrichelte Linie vom Knoten, Pfeilspitze am Ende
    for (const ControlRef& r : visibleControls(c)) {
        const QPointF node = t.map(c.node(r.owner).point);
        const QPointF ctl = t.map(controlPoint(c, r));
        p.setPen(QPen(blue, 1.0, Qt::DashLine));
        p.drawLine(node, ctl);
        QPointF dir = ctl - node;
        const double len = std::hypot(dir.x(), dir.y());
        dir = len > 1e-9 ? dir / len : QPointF(1, 0);
        const QPointF n(-dir.y(), dir.x());
        p.setPen(Qt::NoPen);
        p.setBrush(blue);
        p.drawPolygon(QPolygonF{ctl + dir * 3.5, ctl - dir * 3.5 + n * 3.5, ctl - dir * 3.5 - n * 3.5});
    }

    // Knoten: hohl, gewählt gefüllt; der Anfang eines Linienzugs etwas grösser
    p.setRenderHint(QPainter::Antialiasing, false);
    const QList<SubPath>& paths = c.subPaths();
    for (int pi = 0; pi < paths.size(); ++pi) {
        for (int n = 0; n < paths[pi].nodes.size(); ++n) {
            const QPointF at = t.map(paths[pi].nodes[n].point);
            const double half = n == 0 ? 4.0 : 3.0;
            const bool selected = m_selected.contains(NodeRef{pi, n});
            p.setPen(QPen(selected ? blue : QColor(40, 40, 40), 1.0));
            p.setBrush(selected ? QBrush(blue) : QBrush(Qt::white));
            p.drawRect(QRectF(at.x() - half, at.y() - half, 2 * half, 2 * half));
        }
    }

    if (m_drag == Drag::Marquee && m_moved) {
        p.setPen(QPen(QColor(80, 80, 80), 1.0, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(m_pressScreen, m_currentScreen).normalized());
    }
    p.restore();
}

} // namespace vek

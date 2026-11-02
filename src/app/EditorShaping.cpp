// Formen (Objekt > Formen und Andockfenster "Formen")
#include "Editor.h"

#include "GroupObject.h"
#include "PathOps.h"
#include "Transaction.h"

#include <algorithm>

namespace vek {

namespace {

constexpr double kShapeTolerance = 0.02; // mm, Kurvenanpassung der Ergebnisse

QPainterPath unionOf(const QList<QPainterPath>& areas)
{
    QPainterPath u;
    for (const QPainterPath& a : areas) {
        if (a.isEmpty())
            continue;
        u = u.isEmpty() ? a : pathops::united(u, a);
    }
    return u;
}

// Stil des Ergebnisses: der des Ziels; bei einer Gruppe der des ersten Objekts darin
Style shapeStyle(const DrawObject& o)
{
    if (node_cast<GroupObject>(&o)) {
        for (int i = 0; i < o.childCount(); ++i) {
            if (const auto* child = node_cast<DrawObject>(o.child(i)))
                return shapeStyle(*child);
        }
    }
    return o.style();
}

std::unique_ptr<CurveObject> result(const DrawObject& styleFrom, const pathops::Geometry& g)
{
    std::unique_ptr<CurveObject> curve = pathops::rebuild(styleFrom, g, kShapeTolerance);
    if (curve)
        curve->setStyle(shapeStyle(styleFrom));
    return curve;
}

} // namespace

QString Editor::shapeOpName(ShapeOp op)
{
    switch (op) {
    case ShapeOp::Weld: return tr("Verschmelzen");
    case ShapeOp::Trim: return tr("Zuschneiden");
    case ShapeOp::Intersect: return tr("Schnittmenge");
    case ShapeOp::Simplify: return tr("Vereinfachen");
    case ShapeOp::FrontMinusBack: return tr("Vorn minus hinten");
    case ShapeOp::BackMinusFront: return tr("Hinten minus vorn");
    case ShapeOp::Boundary: return tr("Begrenzung");
    }
    return {};
}

bool Editor::shapeWithTarget(ShapeOp op, const QList<NodeId>& sourceIds, NodeId targetId, bool keepSources,
                             bool keepTarget)
{
    const auto* target = m_doc->findAs<DrawObject>(targetId);
    if (!target || isLocked(targetId) || op == ShapeOp::Simplify || op == ShapeOp::FrontMinusBack
        || op == ShapeOp::BackMinusFront || op == ShapeOp::Boundary)
        return false;
    QList<const DrawObject*> sources;
    for (NodeId id : sourceIds) {
        const auto* o = m_doc->findAs<DrawObject>(id);
        // Ziel, Teile des Ziels und Gruppen, die das Ziel enthalten, zählen nicht als Quelle
        if (o && id != targetId && !o->isAncestorOf(target) && !target->isAncestorOf(o) && !isLocked(id))
            sources << o;
    }
    if (sources.isEmpty())
        return false;

    const pathops::Geometry t = pathops::geometryOf(*target);
    QList<QPainterPath> areas;
    QList<QPolygonF> sourceLines;
    for (const DrawObject* s : sources) {
        const pathops::Geometry g = pathops::geometryOf(*s);
        areas << g.area;
        sourceLines << g.lines;
    }
    const QPainterPath sourceArea = unionOf(areas);
    pathops::Geometry g;
    switch (op) {
    case ShapeOp::Weld:
        g.area = unionOf({t.area, sourceArea});
        g.lines = t.lines + sourceLines;
        break;
    case ShapeOp::Trim:
        g.area = pathops::subtracted(t.area, sourceArea);
        g.lines = pathops::clipLines(t.lines, sourceArea, false);
        break;
    case ShapeOp::Intersect:
        g.area = pathops::intersected(t.area, sourceArea);
        g.lines = pathops::clipLines(t.lines, sourceArea, true);
        break;
    default:
        return false;
    }
    std::unique_ptr<CurveObject> shaped = result(*target, g);
    if (!shaped && op == ShapeOp::Intersect)
        return false; // keine Überschneidung: nichts tun

    Transaction tx(*m_doc, shapeOpName(op));
    NodeId created = 0;
    // Ergebnis direkt über dem Ziel, erst danach entfernen (sonst verschieben sich die Plätze)
    if (shaped)
        created = tx.insert(target->parent()->id(), target->indexInParent() + 1, std::move(shaped));
    if (!keepTarget)
        tx.remove(targetId);
    if (!keepSources) {
        for (const DrawObject* s : sources)
            tx.remove(s->id());
    }
    tx.commit();
    setSelection(created ? QList<NodeId>{created} : QList<NodeId>());
    return true;
}

bool Editor::canShapeSelection(ShapeOp op) const
{
    const int count = int(topLevelSelection().size());
    return op == ShapeOp::Boundary ? count >= 1 : count >= 2;
}

bool Editor::shapeSelection(ShapeOp op)
{
    if (!canShapeSelection(op))
        return false;
    // Unten nach oben in der Stapelreihenfolge
    const QList<NodeId> ids = topLevelSelection();
    QList<const DrawObject*> stack;
    for (NodeId id : ids)
        stack << m_doc->findAs<DrawObject>(id);

    switch (op) {
    case ShapeOp::Weld:
    case ShapeOp::Trim:
    case ShapeOp::Intersect: {
        // Ziel ist das zuletzt ausgewählte Objekt
        NodeId target = ids.last();
        for (auto it = m_selection.crbegin(); it != m_selection.crend(); ++it) {
            if (ids.contains(*it)) {
                target = *it;
                break;
            }
        }
        QList<NodeId> sources = ids;
        sources.removeAll(target);
        // Verschmelzen ersetzt alles; Zuschneiden behält die Werkzeuge; die Schnittmenge kommt dazu
        return shapeWithTarget(op, sources, target, op != ShapeOp::Weld, op == ShapeOp::Intersect);
    }
    case ShapeOp::Simplify: {
        // Jedes Objekt verliert, was von den Objekten darüber verdeckt wird
        Transaction tx(*m_doc, shapeOpName(op));
        QList<NodeId> kept;
        for (int i = 0; i < stack.size(); ++i) {
            QList<QPainterPath> above;
            for (int j = i + 1; j < stack.size(); ++j)
                above << pathops::geometryOf(*stack[j]).area;
            const QPainterPath cover = unionOf(above);
            if (cover.isEmpty() || !cover.intersects(stack[i]->path())) {
                kept << ids[i];
                continue;
            }
            const pathops::Geometry own = pathops::geometryOf(*stack[i]);
            pathops::Geometry g;
            g.area = pathops::subtracted(own.area, cover);
            g.lines = pathops::clipLines(own.lines, cover, false);
            if (std::unique_ptr<CurveObject> shaped = result(*stack[i], g))
                kept << tx.insert(stack[i]->parent()->id(), stack[i]->indexInParent() + 1, std::move(shaped));
            tx.remove(ids[i]);
        }
        tx.commit();
        setSelection(kept);
        return true;
    }
    case ShapeOp::FrontMinusBack:
    case ShapeOp::BackMinusFront: {
        // Das vorderste bzw. hinterste Objekt minus alle anderen; die anderen verschwinden
        const bool front = op == ShapeOp::FrontMinusBack;
        const DrawObject* keep = front ? stack.last() : stack.first();
        QList<QPainterPath> others;
        for (const DrawObject* o : stack) {
            if (o != keep)
                others << pathops::geometryOf(*o).area;
        }
        const QPainterPath cut = unionOf(others);
        const pathops::Geometry own = pathops::geometryOf(*keep);
        pathops::Geometry g;
        g.area = pathops::subtracted(own.area, cut);
        g.lines = pathops::clipLines(own.lines, cut, false);
        std::unique_ptr<CurveObject> shaped = result(*keep, g);
        Transaction tx(*m_doc, shapeOpName(op));
        NodeId created = 0;
        if (shaped)
            created = tx.insert(keep->parent()->id(), keep->indexInParent() + 1, std::move(shaped));
        for (NodeId id : ids)
            tx.remove(id);
        tx.commit();
        setSelection(created ? QList<NodeId>{created} : QList<NodeId>());
        return true;
    }
    case ShapeOp::Boundary: {
        // Umriss um alles, ohne Füllung, über den Objekten; die Originale bleiben
        QList<QPainterPath> areas;
        for (const DrawObject* o : stack)
            areas << pathops::geometryOf(*o).area;
        pathops::Geometry g;
        g.area = unionOf(areas);
        std::unique_ptr<CurveObject> outline = pathops::rebuild(*stack.last(), g, kShapeTolerance);
        if (!outline)
            return false;
        Style s;
        s.fill = Fill::none();
        s.stroke = Color(0, 0, 0);
        s.strokeWidth = 0.2;
        outline->setStyle(s);
        outline->setEffect(Effect::Blur, Ramp());
        outline->setEffect(Effect::Transparency, Ramp());
        outline->setName(QString());
        Transaction tx(*m_doc, shapeOpName(op));
        const NodeId created = tx.insert(stack.last()->parent()->id(), stack.last()->indexInParent() + 1, std::move(outline));
        tx.commit();
        setSelection({created});
        return true;
    }
    }
    return false;
}

} // namespace vek

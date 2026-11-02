// Editor-Befehle für Hülle, fixierte Linse und PowerClip-Bearbeitung
#include "Editor.h"

#include "BlendObject.h"
#include "CloneObject.h"
#include "GroupObject.h"
#include "ImageObject.h"
#include "PathOps.h"
#include "PerfectShape.h"
#include "PowerClipObject.h"
#include "RectangleObject.h"
#include "Serialization.h"
#include "Transaction.h"

#include <algorithm>
#include <vector>

namespace vek {

bool Editor::canEnvelope(const DrawObject& o)
{
    return node_cast<RectangleObject>(&o) || node_cast<EllipseObject>(&o) || node_cast<PolygonObject>(&o) || node_cast<PerfectShapeObject>(&o)
           || node_cast<CurveObject>(&o) || node_cast<TextObject>(&o);
}

void Editor::setObjectEnvelopes(NodeId id, const QList<Envelope>& envelopes, const QString& label)
{
    const DrawObject* o = m_doc->findAs<DrawObject>(id);
    if (!o || isLocked(id) || !canEnvelope(*o) || o->envelopes() == envelopes)
        return;
    Transaction tx(*m_doc, label);
    tx.modify<DrawObject>(id, [&envelopes](DrawObject& d) { d.setEnvelopes(envelopes); });
    tx.commit();
}

bool Editor::applyEnvelopeShape(const QPainterPath& shape, bool pageCoordinates, Envelope::Mode mode, bool addNew)
{
    Transaction tx(*m_doc, tr("Hülle"));
    for (NodeId id : topLevelSelection()) {
        const DrawObject* o = m_doc->findAs<DrawObject>(id);
        if (!o || !canEnvelope(*o))
            continue;
        QList<Envelope> list = o->envelopes();
        QRectF box;
        if (!addNew && !list.isEmpty()) {
            box = list.last().box;
            list.removeLast();
        } else {
            // Neue Hülle um die Form, wie sie mit den bisherigen Hüllen aussieht
            QPainterPath g = o->localPath();
            for (const Envelope& e : std::as_const(list))
                g = e.map(g);
            box = g.boundingRect();
        }
        if (box.width() <= 1e-6 || box.height() <= 1e-6)
            continue;
        const QPainterPath local = pageCoordinates ? o->transform().inverted().map(shape) : shape;
        list << (local.isEmpty() ? Envelope::rectangle(box, mode) : Envelope::fromOutline(box, local, mode));
        tx.modify<DrawObject>(id, [&list](DrawObject& d) { d.setEnvelopes(list); });
    }
    if (tx.isEmpty())
        return false;
    tx.commit();
    return true;
}

void Editor::setSelectionEnvelopeMode(Envelope::Mode mode)
{
    Transaction tx(*m_doc, tr("Hüllenmodus"));
    for (NodeId id : topLevelSelection()) {
        const DrawObject* o = m_doc->findAs<DrawObject>(id);
        if (!o || o->envelopes().isEmpty() || o->envelopes().last().mode == mode)
            continue;
        tx.modify<DrawObject>(id, [mode](DrawObject& d) {
            QList<Envelope> list = d.envelopes();
            list.last().mode = mode;
            d.setEnvelopes(list);
        });
    }
    if (!tx.isEmpty())
        tx.commit();
}

void Editor::clearSelectionEnvelopes()
{
    Transaction tx(*m_doc, tr("Hülle löschen"));
    for (NodeId id : topLevelSelection())
        if (const DrawObject* o = m_doc->findAs<DrawObject>(id); o && o->hasEnvelope())
            tx.modify<DrawObject>(id, [](DrawObject& d) { d.setEnvelopes({}); });
    if (!tx.isEmpty())
        tx.commit();
}

QList<const DrawObject*> Editor::objectsBelow(const DrawObject& stop) const
{
    // Oberstes Objekt im Ebenenbaum, das stop enthält
    const Node* top = &stop;
    while (top->parent() && !node_cast<Layer>(top->parent()))
        top = top->parent();
    QList<const DrawObject*> out;
    auto collect = [&](const Page& page) {
        for (int l = 0; l < page.childCount(); ++l) {
            const auto* layer = node_cast<Layer>(page.child(l));
            if (!layer || !layer->visible())
                continue;
            for (int i = 0; i < layer->childCount(); ++i) {
                if (layer->child(i) == top)
                    return true;
                if (const auto* d = node_cast<DrawObject>(layer->child(i)))
                    out << d;
            }
        }
        return false;
    };
    const Page* page = m_doc->pageOf(stop.id());
    if (page != &m_doc->masterPage() && collect(m_doc->masterPage()))
        return out;
    if (page)
        collect(*page);
    return out;
}

namespace {

// Wie die Linse beim Fixieren auf die Objekte darunter wirkt
struct Freeze {
    Lens lens;
    QTransform move; // Blickpunkt und Vergrösserung
    std::function<QPointF(const QPointF&)> warp; // Fischauge (sonst leer)
    double step = 0.1;
};

Color frozenColor(const Freeze& fz, const Color& c)
{
    return fz.lens.changesColor() ? fz.lens.mapColor(c) : c;
}

Style frozenStyle(const Freeze& fz, Style s)
{
    if (fz.lens.type == Lens::Type::Wireframe) {
        if (!s.fill.isNone())
            s.fill = Fill::solid(fz.lens.color2);
        s.stroke = fz.lens.color;
        return s;
    }
    if (!fz.lens.changesColor())
        return s;
    if (s.fill.type == Fill::Type::Solid)
        s.fill.color = frozenColor(fz, s.fill.color);
    for (GradientStop& stop : s.fill.stops)
        stop.color = frozenColor(fz, stop.color);
    if (s.stroke)
        s.stroke = frozenColor(fz, *s.stroke);
    return s;
}

std::unique_ptr<DrawObject> copyOf(const DrawObject& o)
{
    std::unique_ptr<Node> n = serial::fromJson(serial::toJson(o, serial::Ids::Drop), serial::Ids::Drop);
    auto* d = node_cast<DrawObject>(n.get());
    if (!d)
        return nullptr;
    n.release();
    return std::unique_ptr<DrawObject>(d);
}

std::unique_ptr<DrawObject> frozenCopy(const DrawObject& o, const Freeze& fz);

void attach(DrawObject& parent, std::unique_ptr<DrawObject> child)
{
    if (child)
        attachDetachedChild(parent, std::move(child));
}

std::unique_ptr<DrawObject> frozenCopy(const DrawObject& o, const Freeze& fz)
{
    if (o.lens().isActive())
        return nullptr; // Linsen unter Linsen zeigen nichts Eigenes
    if (const auto* clone = node_cast<CloneObject>(&o)) {
        const std::shared_ptr<const DrawObject> shown = clone->rendered();
        return shown ? frozenCopy(*shown, fz) : nullptr;
    }
    if (const auto* clip = node_cast<PowerClipObject>(&o)) {
        if (!clip->frame())
            return nullptr;
        auto out = std::make_unique<PowerClipObject>();
        out->setContentsLocked(clip->contentsLocked());
        std::unique_ptr<DrawObject> frame = frozenCopy(*clip->frame(), fz);
        if (!frame)
            return nullptr;
        attach(*out, std::move(frame));
        for (const DrawObject* c : clip->contents())
            attach(*out, frozenCopy(*c, fz));
        return out;
    }
    if (node_cast<GroupObject>(&o) || node_cast<BlendObject>(&o)) {
        auto group = std::make_unique<GroupObject>();
        group->setName(o.name());
        group->setEffect(Effect::Transparency, o.transparency());
        group->setEffect(Effect::Blur, o.blurRamp());
        if (const auto* blend = node_cast<BlendObject>(&o)) {
            if (blend->start())
                attach(*group, frozenCopy(*blend->start(), fz));
            for (const auto& step : blend->intermediates())
                attach(*group, frozenCopy(*step, fz));
            if (blend->end())
                attach(*group, frozenCopy(*blend->end(), fz));
        } else {
            for (int i = 0; i < o.childCount(); ++i)
                if (const auto* c = node_cast<DrawObject>(o.child(i)))
                    attach(*group, frozenCopy(*c, fz));
        }
        return group->childCount() > 0 ? std::move(group) : nullptr;
    }
    if (node_cast<ImageObject>(&o)) {
        // Bilder: nur verschoben/vergrössert (Farbe und Fischauge wirken auf Vektoren)
        std::unique_ptr<DrawObject> copy = copyOf(o);
        if (copy)
            copy->applyTransform(fz.move);
        return copy;
    }
    const auto* text = node_cast<TextObject>(&o);
    if (!fz.warp && !text && !fz.lens.changesColor() && fz.lens.type != Lens::Type::Wireframe) {
        // Nur Lage (Vergrössern, Blickpunkt): das Objekt selbst bleibt erhalten
        std::unique_ptr<DrawObject> copy = copyOf(o);
        if (copy)
            copy->applyTransform(fz.move);
        return copy;
    }
    // Sonst wird das Objekt zu Kurven (Text: je Farbe) mit veränderten Farben und Formen
    QList<std::pair<QPainterPath, Style>> parts;
    if (text) {
        for (const TextObject::Piece& piece : text->shownPieces()) {
            Style s = text->style();
            if (piece.color)
                s.fill = Fill::solid(*piece.color);
            parts << std::pair{text->transform().map(piece.path), s};
        }
    } else {
        parts << std::pair{o.path(), o.style()};
    }
    std::vector<std::unique_ptr<DrawObject>> curves;
    for (const auto& [page, style] : std::as_const(parts)) {
        std::unique_ptr<CurveObject> curve;
        if (fz.warp)
            curve = std::make_unique<CurveObject>(pathops::fitPath(warpPath(page, fz.warp, fz.step)));
        else
            curve = CurveObject::fromPath(fz.move.map(page));
        if (curve->subPaths().isEmpty())
            continue;
        curve->setStyle(frozenStyle(fz, style));
        curve->setEffect(Effect::Transparency, o.transparency());
        curve->setEffect(Effect::Blur, o.blurRamp());
        curves.push_back(std::move(curve));
    }
    if (curves.empty())
        return nullptr;
    if (curves.size() == 1) {
        curves.front()->setName(o.name());
        return std::move(curves.front());
    }
    auto group = std::make_unique<GroupObject>();
    group->setName(o.name());
    for (auto& c : curves)
        attach(*group, std::move(c));
    return group;
}

} // namespace

NodeId Editor::freezeLens(NodeId lensId)
{
    const DrawObject* lensObj = m_doc->findAs<DrawObject>(lensId);
    if (!lensObj || !lensObj->lens().isActive() || isLocked(lensId))
        return InvalidNodeId;
    Freeze fz;
    fz.lens = lensObj->lens();
    const QRectF b = lensObj->boundingRect();
    const QPointF c = b.center();
    const QPointF v = fz.lens.viewpoint ? c + fz.lens.viewOffset : c;
    const double f = fz.lens.type == Lens::Type::Magnify ? std::max(1.0, fz.lens.amount / 10.0) : 1.0;
    fz.move = QTransform::fromTranslate(-v.x(), -v.y()) * QTransform::fromScale(f, f) * QTransform::fromTranslate(c.x(), c.y());
    fz.step = std::max(1e-3, std::max(b.width(), b.height()) / 200.0);
    if (fz.lens.type == Lens::Type::FishEye) {
        const QSizeF radius(b.width() / 2.0, b.height() / 2.0);
        const Lens lens = fz.lens;
        const QTransform move = fz.move;
        fz.warp = [lens, move, c, radius](const QPointF& p) { return lens.fishEye(move.map(p), c, radius, true); };
    }

    auto clip = std::make_unique<PowerClipObject>();
    clip->setName(lensObj->name());
    // Rahmen: die Linsenform ohne Linse; Farblinsen färben auch das (weisse) Papier
    std::unique_ptr<DrawObject> frame = copyOf(*lensObj);
    if (!frame)
        return InvalidNodeId;
    frame->setLens(Lens());
    Style fs = frame->style();
    fs.fill = fz.lens.changesColor() ? Fill::solid(fz.lens.mapColor(Color(255, 255, 255))) : Fill::none();
    frame->setStyle(fs);
    attach(*clip, std::move(frame));
    for (const DrawObject* below : objectsBelow(*lensObj)) {
        const QRectF shown = fz.move.mapRect(below->boundingRect());
        if (!shown.intersects(b))
            continue;
        attach(*clip, frozenCopy(*below, fz));
    }
    const NodeId parent = lensObj->parent()->id();
    const int index = lensObj->indexInParent();
    Transaction tx(*m_doc, tr("Linse fixieren"));
    tx.remove(lensId); // danach gibt es lensObj nicht mehr
    const NodeId id = tx.insert(parent, index, std::move(clip));
    tx.commit();
    setSelection({id});
    return id;
}

bool Editor::fitTextToPath(NodeId text, NodeId pathObject)
{
    const auto* t = m_doc->findAs<TextObject>(text);
    const DrawObject* p = m_doc->findAs<DrawObject>(pathObject);
    if (!t || !p || text == pathObject || t->kind() != TextObject::Kind::Artistic || isLocked(text) || node_cast<TextObject>(p))
        return false;
    // Geschlossene Form: Text mittig oben (Anfang des Pfads oft rechts)
    double offset = 0.0;
    const QList<QPolygonF> polys = pathops::flattenFine(p->path(), 0.2);
    if (!polys.isEmpty() && polys.first().size() > 2 && polys.first().first() == polys.first().last()) {
        const QPolygonF& poly = polys.first();
        double s = 0.0, topS = 0.0, topY = poly.first().y();
        for (int i = 1; i < poly.size(); ++i) {
            s += QLineF(poly[i - 1], poly[i]).length();
            if (poly[i].y() < topY) {
                topY = poly[i].y();
                topS = s;
            }
        }
        offset = topS - t->frameRect().width() / 2.0;
    }
    Transaction tx(*m_doc, tr("Text an Pfad anpassen"));
    tx.modify<TextObject>(text, [pathObject, offset](TextObject& o) {
        o.setPathFit(0, 0, 0, false); // Transformation zurücksetzen dürfen
        o.setTransform(QTransform());
        o.setPathFit(pathObject, offset, 0.0, false);
    });
    tx.commit();
    setSelection({text});
    return true;
}

bool Editor::canFitSelectedTextToPath() const
{
    const QList<const DrawObject*> objects = selectedObjects();
    if (objects.size() != 2)
        return false;
    const int texts = int(node_cast<TextObject>(objects[0]) != nullptr) + int(node_cast<TextObject>(objects[1]) != nullptr);
    return texts == 1;
}

bool Editor::fitSelectedTextToPath()
{
    if (!canFitSelectedTextToPath())
        return false;
    const QList<const DrawObject*> objects = selectedObjects();
    const bool firstIsText = node_cast<TextObject>(objects[0]) != nullptr;
    return fitTextToPath((firstIsText ? objects[0] : objects[1])->id(), (firstIsText ? objects[1] : objects[0])->id());
}

void Editor::straightenSelectedText()
{
    Transaction tx(*m_doc, tr("Text begradigen"));
    for (NodeId id : topLevelSelection()) {
        const auto* t = m_doc->findAs<TextObject>(id);
        if (!t || !t->onPath())
            continue;
        // Wieder waagrecht, dort wo der Text auf dem Pfad anfing
        const QPointF at = t->path().boundingRect().topLeft();
        tx.modify<TextObject>(id, [at](TextObject& o) {
            o.setPathFit(0, 0, 0, false);
            o.setTransform(QTransform::fromTranslate(at.x(), at.y()));
        });
    }
    if (!tx.isEmpty())
        tx.commit();
}

void Editor::setTextPathParams(std::optional<double> offset, std::optional<double> distance, std::optional<bool> flip)
{
    Transaction tx(*m_doc, tr("Text an Pfad"));
    for (NodeId id : topLevelSelection()) {
        const auto* t = m_doc->findAs<TextObject>(id);
        if (!t || !t->onPath() || isLocked(id))
            continue;
        tx.modify<TextObject>(id, [&](TextObject& o) {
            o.setPathFit(o.pathObject(), offset.value_or(o.pathOffset()), distance.value_or(o.pathDistance()), flip.value_or(o.pathFlip()));
        });
    }
    if (!tx.isEmpty())
        tx.commit();
}

void Editor::setTextBullets(bool on)
{
    Transaction tx(*m_doc, on ? tr("Aufzählungszeichen") : tr("Aufzählungszeichen entfernen"));
    for (const TextObject* t : selectedTexts()) {
        if (isLocked(t->id()))
            continue;
        tx.modify<TextObject>(t->id(), [on](TextObject& o) {
            QList<TextParagraph> list = o.paragraphs();
            for (TextParagraph& p : list)
                p.bullet = on;
            o.setParagraphs(list);
        });
    }
    if (!tx.isEmpty())
        tx.commit();
}

void Editor::setTextDropCap(int lines)
{
    Transaction tx(*m_doc, lines > 0 ? tr("Initiale") : tr("Initiale entfernen"));
    for (const TextObject* t : selectedTexts()) {
        if (isLocked(t->id()))
            continue;
        tx.modify<TextObject>(t->id(), [lines](TextObject& o) {
            QList<TextParagraph> list = o.paragraphs();
            for (TextParagraph& p : list)
                p.dropCap = lines;
            o.setParagraphs(list);
        });
    }
    if (!tx.isEmpty())
        tx.commit();
}

NodeId Editor::editedPowerClip() const
{
    return m_doc->findAs<PowerClipObject>(m_clipEdit) ? m_clipEdit : InvalidNodeId;
}

bool Editor::beginPowerClipEdit(NodeId clip)
{
    const auto* c = m_doc->findAs<PowerClipObject>(clip);
    if (!c || isLocked(clip))
        return false;
    m_clipEdit = clip;
    emit powerClipEditChanged(clip);
    QList<NodeId> ids;
    for (const DrawObject* o : c->contents())
        ids << o->id();
    setSelection(ids);
    return true;
}

void Editor::endPowerClipEdit()
{
    const NodeId clip = editedPowerClip();
    m_clipEdit = InvalidNodeId;
    emit powerClipEditChanged(InvalidNodeId);
    setSelection(clip != InvalidNodeId ? QList<NodeId>{clip} : QList<NodeId>());
}

bool Editor::selectPowerClipContents()
{
    const QList<NodeId> ids = topLevelSelection();
    return ids.size() == 1 && beginPowerClipEdit(ids.first());
}

} // namespace vek

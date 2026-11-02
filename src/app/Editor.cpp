#include "Editor.h"

#include "BlurBake.h"
#include "ConnectorObject.h"
#include "ImageObject.h"
#include "ImagePool.h"
#include "Options.h"
#include <QTextDocument>
#include <QTextCursor>
#include "CloneObject.h"
#include "SuperClipObject.h"
#include "BlendObject.h"
#include "Canvas.h"
#include <functional>
#include "Outline.h"
#include "TableObject.h"

#include "GroupObject.h"
#include "PathOps.h"
#include "Serialization.h"
#include "Transaction.h"

#include <QClipboard>
#include <QCursor>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeData>
#include <QPainterPathStroker>
#include <QSet>

#include <algorithm>

namespace vek {

namespace {

// Pfad aus Kindindizes von der Wurzel: ergibt die Zeichenreihenfolge im ganzen Dokument
QList<int> treePath(const Node* n)
{
    QList<int> path;
    for (; n && n->parent(); n = n->parent())
        path.prepend(n->indexInParent());
    return path;
}

// Oberste sichtbare, nicht gesperrte Ebene einer Seite
NodeId topEditableLayer(const Page& page)
{
    for (int i = page.childCount() - 1; i >= 0; --i) {
        auto* layer = node_cast<Layer>(page.child(i));
        if (layer && layer->visible() && !layer->locked())
            return layer->id();
    }
    return InvalidNodeId;
}

} // namespace

Editor::Editor(QObject* parent)
    : QObject(parent)
{
    setDocument(std::make_unique<Document>(), QString());
}

Editor::~Editor() = default;

void Editor::setDocument(std::unique_ptr<Document> doc, const QString& filePath)
{
    if (m_doc)
        m_doc->disconnect(this);
    m_doc = std::move(doc);
    m_filePath = filePath;
    m_currentPage = 0;
    m_selection.clear();
    m_selectedGuide = -1;
    m_clipEdit = InvalidNodeId;
    connect(m_doc.get(), &Document::changed, this, &Editor::onDocumentChanged);
    connect(m_doc.get(), &Document::modifiedChanged, this, &Editor::modifiedChanged);
    emit documentReplaced();
    emit currentPageChanged(m_currentPage);
    emit selectionChanged();
}

void Editor::onDocumentChanged()
{
    // Nach Undo/Redo können ausgewählte Objekte verschwunden oder auf eine andere Seite
    // gewandert sein.
    const int pageCount = m_doc->pageCount();
    if (m_currentPage >= pageCount) {
        m_currentPage = pageCount - 1;
        emit currentPageChanged(m_currentPage);
    }
    QList<NodeId> kept;
    for (NodeId id : std::as_const(m_selection)) {
        const Page* p = m_doc->pageOf(id);
        if (node_cast<DrawObject>(m_doc->find(id)) && (p == &m_doc->masterPage() || p == &currentPage()))
            kept.append(id);
    }
    if (m_selectedGuide >= guides().size()) {
        m_selectedGuide = -1;
        emit guideSelectionChanged(-1);
    }
    const bool selectionChangedNow = kept != m_selection;
    m_selection = kept;
    if (m_clipEdit != InvalidNodeId && !m_doc->findAs<SuperClipObject>(m_clipEdit)) {
        m_clipEdit = InvalidNodeId;
        emit superClipEditChanged(InvalidNodeId);
    }
    emit documentChanged();
    if (selectionChangedNow)
        emit selectionChanged();
}

void Editor::setCurrentPageIndex(int index)
{
    index = std::clamp(index, 0, m_doc->pageCount() - 1);
    if (index == m_currentPage)
        return;
    m_currentPage = index;
    // Die Liste der sichtbaren Hilfslinien wechselt mit der Seite
    if (m_selectedGuide >= 0) {
        m_selectedGuide = -1;
        emit guideSelectionChanged(-1);
    }
    // Objekte der Masterseite bleiben ausgewählt, die der alten Seite nicht
    QList<NodeId> kept;
    for (NodeId id : std::as_const(m_selection)) {
        if (m_doc->pageOf(id) == &m_doc->masterPage())
            kept.append(id);
    }
    m_selection = kept;
    emit currentPageChanged(index);
    emit selectionChanged();
}

NodeId Editor::activeLayer() const
{
    return topEditableLayer(currentPage());
}

QList<const DrawObject*> Editor::selectedObjects() const
{
    QList<const DrawObject*> result;
    for (NodeId id : m_selection) {
        if (auto* o = m_doc->findAs<DrawObject>(id))
            result.append(o);
    }
    return result;
}

QRectF Editor::selectionBounds() const
{
    QRectF r;
    for (const DrawObject* o : selectedObjects())
        r = r.isNull() ? o->boundingRect() : r.united(o->boundingRect());
    return r;
}

void Editor::setSelection(const QList<NodeId>& ids, bool marquee)
{
    if (!ids.isEmpty() && m_selectedGuide >= 0) {
        m_selectedGuide = -1;
        emit guideSelectionChanged(-1);
    }
    m_marquee = marquee;
    if (ids == m_selection)
        return;
    m_selection = ids;
    emit selectionChanged();
}

NodeId Editor::alignReference() const
{
    const QList<NodeId> ids = topLevelSelection();
    if (ids.size() < 2)
        return InvalidNodeId;
    if (!m_marquee) {
        // Zuletzt gewählt: nach Auswahlreihenfolge, nicht nach Lage im Dokument
        for (auto it = m_selection.crbegin(); it != m_selection.crend(); ++it)
            if (ids.contains(*it))
                return *it;
        return ids.last();
    }
    // Auswahlrahmen: das Objekt oben links (kleinster Abstand der linken oberen Ecke zum Ursprung
    // der Auswahl)
    const QRectF all = selectionBounds();
    NodeId best = InvalidNodeId;
    double bestDist = 0.0;
    for (NodeId id : ids) {
        const QRectF r = m_doc->findAs<DrawObject>(id)->boundingRect();
        const double d = std::hypot(r.left() - all.left(), r.top() - all.top());
        if (best == InvalidNodeId || d < bestDist - 1e-9) {
            best = id;
            bestDist = d;
        }
    }
    return best;
}

void Editor::alignSelection(align::H h, align::V v, AlignTo to, const QPointF& point)
{
    const QList<NodeId> ids = topLevelSelection();
    if (ids.isEmpty() || (h == align::H::None && v == align::V::None))
        return;
    const QRectF page(QPointF(0, 0), currentPage().size());
    NodeId reference = InvalidNodeId;
    QRectF ref;
    switch (to) {
    case AlignTo::ActiveObjects:
        reference = alignReference();
        if (reference == InvalidNodeId)
            ref = page; // ein einzelnes Objekt richtet sich an der Seite aus
        else
            ref = m_doc->findAs<DrawObject>(reference)->boundingRect();
        break;
    case AlignTo::PageEdge: ref = page; break;
    case AlignTo::PageCenter: ref = QRectF(page.center(), QSizeF(0, 0)); break;
    case AlignTo::Point: ref = QRectF(point, QSizeF(0, 0)); break;
    case AlignTo::Grid: break;
    }
    QSizeF grid;
    if (to == AlignTo::Grid) {
        const Options& o = Options::instance();
        const double gh = o.number(QStringLiteral("doc/grid/h")), gv = o.number(QStringLiteral("doc/grid/v"));
        if (gh <= 0 || gv <= 0)
            return;
        grid = o.integer(QStringLiteral("doc/grid/mode")) == 1 ? QSizeF(gh, gv) : QSizeF(1.0 / gh, 1.0 / gv);
    }
    Transaction tx(*m_doc, tr("Ausrichten"));
    for (NodeId id : ids) {
        if (id == reference)
            continue;
        const QRectF r = m_doc->findAs<DrawObject>(id)->boundingRect();
        const QPointF d = to == AlignTo::Grid ? align::toGrid(r, grid, h, v) : align::offset(r, ref, h, v);
        if (std::abs(d.x()) < 1e-9 && std::abs(d.y()) < 1e-9)
            continue;
        const QTransform m = QTransform::fromTranslate(d.x(), d.y());
        tx.modifySubtree<DrawObject>(id, [&m](DrawObject& o) { o.applyTransform(m); });
    }
    if (!tx.isEmpty())
        tx.commit();
}

void Editor::distributeSelection(align::Spread h, align::Spread v, bool toPage)
{
    const QList<NodeId> ids = topLevelSelection();
    if (ids.size() < 2 || (h == align::Spread::None && v == align::Spread::None))
        return;
    QList<QRectF> rects;
    for (NodeId id : ids)
        rects << m_doc->findAs<DrawObject>(id)->boundingRect();
    const QRectF extent = toPage ? QRectF(QPointF(0, 0), currentPage().size()) : selectionBounds();
    const QList<QPointF> moves = align::distribute(rects, extent, h, v);
    Transaction tx(*m_doc, tr("Verteilen"));
    for (int i = 0; i < ids.size(); ++i) {
        const QPointF d = moves[i];
        if (std::abs(d.x()) < 1e-9 && std::abs(d.y()) < 1e-9)
            continue;
        const QTransform m = QTransform::fromTranslate(d.x(), d.y());
        tx.modifySubtree<DrawObject>(ids[i], [&m](DrawObject& o) { o.applyTransform(m); });
    }
    if (!tx.isEmpty())
        tx.commit();
}

void Editor::selectAll()
{
    QList<NodeId> ids;
    for (const DrawObject* o : pickableObjects()) {
        if (m_doc->pageOf(o->id()) == &currentPage())
            ids.append(o->id());
    }
    setSelection(ids);
}

QList<const DrawObject*> Editor::pickableObjects() const
{
    QList<const DrawObject*> result;
    // SuperClip bearbeiten: nur seine Inhalte
    if (const auto* clip = m_doc->findAs<SuperClipObject>(m_clipEdit))
        return clip->contents();
    for (const Page* page : {&m_doc->masterPage(), &currentPage()}) {
        for (int l = 0; l < page->childCount(); ++l) {
            auto* layer = node_cast<Layer>(page->child(l));
            if (!layer || !layer->visible() || layer->locked())
                continue;
            for (int i = 0; i < layer->childCount(); ++i) {
                if (auto* o = node_cast<DrawObject>(layer->child(i)))
                    result.append(o);
            }
        }
    }
    return result;
}

const DrawObject* Editor::objectAt(const QPointF& point, double tolerance) const
{
    const QList<const DrawObject*> objects = pickableObjects();
    for (auto it = objects.rbegin(); it != objects.rend(); ++it) {
        if ((*it)->hitTest(point, tolerance, m_treatAllAsFilled))
            return *it;
    }
    return nullptr;
}

QList<NodeId> Editor::topLevelSelection(bool includeLocked) const
{
    QList<const DrawObject*> objects = selectedObjects();
    QList<NodeId> result;
    for (const DrawObject* o : objects) {
        const bool coveredByAncestor = std::any_of(objects.begin(), objects.end(),
                                                   [o](const DrawObject* other) { return other->isAncestorOf(o); });
        if (!coveredByAncestor && (includeLocked || !isLocked(o->id())))
            result.append(o->id());
    }
    std::sort(result.begin(), result.end(),
              [this](NodeId a, NodeId b) { return treePath(m_doc->find(a)) < treePath(m_doc->find(b)); });
    return result;
}

NodeId Editor::addObject(std::unique_ptr<DrawObject> object, const QString& label)
{
    const NodeId layer = activeLayer();
    if (layer == InvalidNodeId)
        return InvalidNodeId;
    Transaction tx(*m_doc, label);
    const NodeId id = tx.insert(layer, -1, std::move(object));
    tx.commit();
    setSelection({id});
    return id;
}

NodeId Editor::importImage(const QByteArray& encoded, std::optional<QPointF> center, const QString& name)
{
    // Rohdateien zu entwickeln dauert einige Sekunden
    QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    const QString key = imagepool::add(encoded);
    QGuiApplication::restoreOverrideCursor();
    if (key.isEmpty())
        return InvalidNodeId;
    const QImage image = imagepool::image(key);
    const double dpm = image.dotsPerMeterX() > 0 ? image.dotsPerMeterX() : 96.0 / 0.0254;
    const QSizeF size(image.width() / dpm * 1000.0, image.height() / dpm * 1000.0);
    const QPointF c = center.value_or(QRectF(QPointF(0, 0), currentPage().size()).center());
    auto object = std::make_unique<ImageObject>(key, size);
    object->setTransform(QTransform::fromTranslate(c.x() - size.width() / 2.0, c.y() - size.height() / 2.0));
    object->setName(name);
    return addObject(std::move(object), tr("Importieren"));
}

NodeId Editor::importGroup(std::unique_ptr<GroupObject> group, bool centerOnPage, const QString& label)
{
    if (!group || group->childCount() == 0)
        return InvalidNodeId;
    if (centerOnPage) {
        const QRectF b = group->boundingRect();
        const QPointF d = QRectF(QPointF(0, 0), currentPage().size()).center() - b.center();
        group->applyTransform(QTransform::fromTranslate(d.x(), d.y()));
    }
    // Ein einzelnes Objekt ohne Gruppe einfügen
    if (group->childCount() == 1) {
        std::unique_ptr<Node> only = serial::fromJson(serial::toJson(*group->child(0), serial::Ids::Drop), serial::Ids::Drop);
        if (auto* o = node_cast<DrawObject>(only.get())) {
            only.release();
            return addObject(std::unique_ptr<DrawObject>(o), label);
        }
    }
    return addObject(std::move(group), label);
}

NodeId Editor::importImage(const QImage& image, std::optional<QPointF> center)
{
    return image.isNull() ? InvalidNodeId : importImage(imagepool::encoded(imagepool::addImage(image)), center);
}

void Editor::transformSelection(const QTransform& m, const QString& label)
{
    const QList<NodeId> ids = topLevelSelection();
    if (ids.isEmpty() || m.isIdentity())
        return;
    remember(label, [this, m, label] { transformSelection(m, label); });
    Transaction tx(*m_doc, label);
    for (NodeId id : ids)
        tx.modifySubtree<DrawObject>(id, [&m](DrawObject& o) { o.applyTransform(m); });
    tx.commit();
}

QList<Guide> Editor::guides() const
{
    return m_doc->masterPage().guides() + currentPage().guides();
}

std::pair<NodeId, int> Editor::guideSlot(int index) const
{
    const int global = int(m_doc->masterPage().guides().size());
    if (index < global)
        return {m_doc->masterPage().id(), index};
    return {currentPage().id(), index - global};
}

void Editor::replaceGuides(const QList<Guide>& global, const QList<Guide>& currentPageOnly)
{
    if (global == m_doc->masterPage().guides() && currentPageOnly == currentPage().guides())
        return;
    if (m_selectedGuide >= 0) {
        m_selectedGuide = -1;
        emit guideSelectionChanged(-1);
    }
    Transaction tx(*m_doc, tr("Hilfslinien"));
    tx.modify<Page>(m_doc->masterPage().id(), [&global](Page& p) { p.guides() = global; });
    tx.modify<Page>(currentPage().id(), [&currentPageOnly](Page& p) { p.guides() = currentPageOnly; });
    tx.commit();
}

bool Editor::isGuidePageSpecific(int index) const
{
    return index >= int(m_doc->masterPage().guides().size()) && index < int(guides().size());
}

void Editor::setGuidePageSpecific(int index, bool pageSpecific)
{
    if (index < 0 || index >= int(guides().size()) || isGuidePageSpecific(index) == pageSpecific)
        return;
    const Guide guide = guides().at(index);
    const auto [page, local] = guideSlot(index);
    Transaction tx(*m_doc, pageSpecific ? tr("Hilfslinie nur auf dieser Seite") : tr("Hilfslinie auf allen Seiten"));
    tx.modify<Page>(page, [local](Page& p) { p.guides().removeAt(local); });
    const NodeId target = pageSpecific ? currentPage().id() : m_doc->masterPage().id();
    tx.modify<Page>(target, [&guide](Page& p) { p.guides().append(guide); });
    tx.commit();
    // Neue Lage in der Liste: am Ende der globalen bzw. ganz am Ende
    const int newIndex = pageSpecific ? int(guides().size()) - 1 : int(m_doc->masterPage().guides().size()) - 1;
    m_selectedGuide = newIndex;
    emit guideSelectionChanged(newIndex);
}

void Editor::addGuide(const Guide& guide)
{
    Transaction tx(*m_doc, tr("Hilfslinie erstellen"));
    tx.modify<Page>(m_doc->masterPage().id(), [&guide](Page& p) { p.guides().append(guide); });
    tx.commit();
}

void Editor::setGuide(int index, const Guide& guide, const QString& label)
{
    if (index < 0 || index >= guides().size() || guides().at(index) == guide)
        return;
    const auto [page, local] = guideSlot(index);
    Transaction tx(*m_doc, label);
    tx.modify<Page>(page, [&, local](Page& p) { p.guides()[local] = guide; });
    tx.commit();
}

void Editor::setSelectedGuide(int index)
{
    if (index >= guides().size())
        index = -1;
    if (index >= 0 && !m_selection.isEmpty()) {
        m_selection.clear();
        emit selectionChanged();
    }
    if (index == m_selectedGuide)
        return;
    m_selectedGuide = index;
    emit guideSelectionChanged(index);
}

void Editor::removeGuide(int index)
{
    if (index < 0 || index >= guides().size())
        return;
    if (index == m_selectedGuide) {
        m_selectedGuide = -1;
        emit guideSelectionChanged(-1);
    }
    const auto [page, local] = guideSlot(index);
    Transaction tx(*m_doc, tr("Hilfslinie löschen"));
    tx.modify<Page>(page, [local](Page& p) { p.guides().removeAt(local); });
    tx.commit();
}

void Editor::setSelectionBounds(const QRectF& target)
{
    const QRectF current = selectionBounds();
    if (current.isNull() || target.width() <= 0.0 || target.height() <= 0.0)
        return;
    // Linien ohne Ausdehnung in einer Richtung nur verschieben, nicht skalieren
    QRectF from = current;
    QRectF to = target;
    if (from.width() == 0.0)
        to.setWidth(0.0);
    if (from.height() == 0.0)
        to.setHeight(0.0);
    transformSelection(xform::rectToRect(from, to), tr("Grösse und Lage"));
}

void Editor::scaleSelection(double sx, double sy)
{
    const QRectF b = selectionBounds();
    if (b.isNull() || sx == 0.0 || sy == 0.0)
        return;
    transformSelection(xform::scaling(sx, sy, b.center()), tr("Skalieren"));
}

void Editor::rotateSelectionTo(double degreesCcw)
{
    const QList<const DrawObject*> objects = selectedObjects();
    if (objects.size() != 1)
        return;
    // intern im Uhrzeigersinn (y nach unten), angezeigt gegen den Uhrzeigersinn
    const double delta = -degreesCcw - objects.first()->rotationAngle();
    transformSelection(xform::rotation(delta, selectionBounds().center()), tr("Drehen"));
}

void Editor::mirrorSelection(Qt::Orientation orientation)
{
    const QRectF b = selectionBounds();
    if (b.isNull())
        return;
    const bool horizontal = orientation == Qt::Horizontal;
    transformSelection(xform::scaling(horizontal ? -1.0 : 1.0, horizontal ? 1.0 : -1.0, b.center()),
                       tr("Spiegeln"));
}

void Editor::setSelectionOutlinePen(const Style& pen)
{
    auto apply = [&pen](Style& s) {
        const Fill fill = s.fill;
        s = pen;
        s.fill = fill;
    };
    const QList<NodeId> ids = topLevelSelection();
    if (ids.isEmpty()) {
        apply(m_defaultStyle);
        return;
    }
    Transaction tx(*m_doc, tr("Konturstift"));
    for (NodeId id : ids)
        tx.modifySubtree<DrawObject>(id, [&](DrawObject& o) {
            Style s = o.style();
            apply(s);
            o.setStyle(s);
        });
    tx.commit();
}

void Editor::setSelectionOutlineShape(std::optional<int> dash, std::optional<int> startArrow,
                                      std::optional<int> endArrow)
{
    auto apply = [&](Style& s) {
        if (dash)
            s.dash = *dash;
        if (startArrow)
            s.startArrow = *startArrow;
        if (endArrow)
            s.endArrow = *endArrow;
    };
    const QList<NodeId> ids = topLevelSelection();
    if (ids.isEmpty()) {
        apply(m_defaultStyle); // ohne Auswahl: Vorgabe für neue Objekte
        return;
    }
    Transaction tx(*m_doc, startArrow || endArrow ? tr("Pfeilspitze") : tr("Linienstil"));
    for (NodeId id : ids) {
        tx.modifySubtree<DrawObject>(id, [&](DrawObject& o) {
            Style s = o.style();
            apply(s);
            o.setStyle(s);
        });
    }
    tx.commit();
}

void Editor::setSelectionStrokeWidth(double width)
{
    const QList<NodeId> ids = topLevelSelection();
    if (ids.isEmpty()) {
        m_defaultStyle.strokeWidth = width;
        return;
    }
    remember(tr("Konturbreite"), [this, width] { setSelectionStrokeWidth(width); });
    Transaction tx(*m_doc, tr("Konturbreite"));
    for (NodeId id : ids) {
        tx.modifySubtree<DrawObject>(id, [width](DrawObject& o) {
            Style s = o.style();
            s.strokeWidth = width;
            if (!s.stroke) // Breite wählen schaltet die Kontur ein
                s.stroke = Color(0, 0, 0);
            o.setStyle(s);
        });
    }
    tx.commit();
}

void Editor::setObjectProperty(const QString& id, const QVariant& value, const QString& label)
{
    const QList<NodeId> ids = topLevelSelection();
    if (ids.isEmpty())
        return;
    Transaction tx(*m_doc, label);
    for (NodeId objectId : ids)
        tx.modify<DrawObject>(objectId, [&](DrawObject& o) { o.setPropertyValue(id, value); });
    tx.commit();
}

namespace {

const QString kClipboardMime = QStringLiteral("application/x-vektoreditor-objects");

} // namespace

void Editor::copySelection()
{
    const QList<NodeId> ids = topLevelSelection(true);
    if (ids.isEmpty())
        return;
    QJsonArray objects;
    for (NodeId id : ids)
        objects.append(serial::toJson(*m_doc->find(id), serial::Ids::Drop));
    auto* mime = new QMimeData;
    mime->setData(kClipboardMime, QJsonDocument(objects).toJson(QJsonDocument::Compact));
    QGuiApplication::clipboard()->setMimeData(mime);
}

void Editor::cutSelection()
{
    copySelection();
    deleteSelection();
}

bool Editor::canPaste()
{
    const QMimeData* mime = QGuiApplication::clipboard()->mimeData();
    return mime && mime->hasFormat(kClipboardMime);
}

void Editor::paste()
{
    const QMimeData* mime = QGuiApplication::clipboard()->mimeData();
    const NodeId layer = activeLayer();
    if (!mime || !mime->hasFormat(kClipboardMime) || layer == InvalidNodeId)
        return;
    const QJsonArray objects = QJsonDocument::fromJson(mime->data(kClipboardMime)).array();
    QList<NodeId> pasted;
    Transaction tx(*m_doc, tr("Einfügen"));
    for (const QJsonValue& v : objects) {
        std::unique_ptr<Node> node = serial::fromJson(v.toObject(), serial::Ids::Drop);
        if (node && node_cast<DrawObject>(node.get()))
            pasted.append(tx.insert(layer, -1, std::move(node)));
    }
    tx.commit();
    setSelection(pasted);
}

void Editor::deleteSelection()
{
    const QList<NodeId> ids = topLevelSelection();
    if (ids.isEmpty())
        return;
    Transaction tx(*m_doc, tr("Löschen"));
    closeTextChains(tx, ids);
    detachConnectors(tx, ids);
    for (NodeId id : ids)
        tx.remove(id);
    tx.commit();
    clearSelection();
}

void Editor::setNudge(double mm)
{
    if (mm <= 0.0 || mm == nudge())
        return;
    Transaction tx(*m_doc, tr("Verschiebungsabstand"));
    tx.modify<Page>(m_doc->masterPage().id(), [mm](Page& p) { p.setNudge(mm); });
    tx.commit();
}

void Editor::setDuplicateOffset(const QPointF& offset)
{
    if (offset == duplicateOffset())
        return;
    Transaction tx(*m_doc, tr("Duplikatabstand"));
    tx.modify<Page>(m_doc->masterPage().id(), [offset](Page& p) { p.setDuplicateOffset(offset); });
    tx.commit();
}

QList<NodeId> Editor::cutTargets(bool wholePageIfNoSelection) const
{
    QList<NodeId> result;
    // Gruppen werden aufgelöst: geschnitten wird jedes einzelne Objekt darin
    std::function<void(const Node*)> collect = [&](const Node* n) {
        if (node_cast<GroupObject>(n)) {
            for (int i = 0; i < n->childCount(); ++i)
                collect(n->child(i));
        } else if (node_cast<DrawObject>(n) && !isLocked(n->id())) {
            result.append(n->id());
        }
    };
    if (!m_selection.isEmpty()) {
        for (NodeId id : topLevelSelection())
            collect(m_doc->find(id));
        return result;
    }
    if (!wholePageIfNoSelection)
        return result;
    const Page& page = currentPage();
    for (int l = 0; l < page.childCount(); ++l) {
        auto* layer = node_cast<Layer>(page.child(l));
        if (!layer || !layer->visible() || layer->locked())
            continue;
        for (int i = 0; i < layer->childCount(); ++i)
            collect(layer->child(i));
    }
    return result;
}

bool Editor::replaceObjects(const QList<NodeId>& targets, const std::function<Replacement(const DrawObject&)>& fn,
                            const QString& label)
{
    Transaction tx(*m_doc, label);
    QList<NodeId> created;
    QList<NodeId> untouched;
    for (NodeId id : targets) {
        const DrawObject* o = m_doc->findAs<DrawObject>(id);
        if (!o)
            continue;
        Replacement replacement = fn(*o);
        if (!replacement) {
            untouched.append(id);
            continue;
        }
        const NodeId parent = o->parent()->id();
        int index = o->indexInParent();
        tx.remove(id);
        for (std::unique_ptr<DrawObject>& piece : *replacement)
            created.append(tx.insert(parent, index++, std::move(piece)));
    }
    if (tx.isEmpty())
        return false;
    tx.commit();
    // Bisher ausgewählte, unveränderte Objekte bleiben gewählt
    QList<NodeId> selection = created;
    for (NodeId id : m_selection) {
        if (untouched.contains(id))
            selection.append(id);
    }
    setSelection(selection);
    return true;
}

bool Editor::isLocked(NodeId id) const
{
    for (const Node* n = m_doc->find(id); n; n = n->parent()) {
        if (const auto* o = node_cast<DrawObject>(n); o && o->locked())
            return true;
    }
    return false;
}

void Editor::setSelectionLocked(bool locked)
{
    Transaction tx(*m_doc, locked ? tr("Objekt sperren") : tr("Objekt entsperren"));
    for (NodeId id : topLevelSelection(true)) {
        if (m_doc->findAs<DrawObject>(id)->locked() != locked)
            tx.modify<DrawObject>(id, [locked](DrawObject& o) { o.setLocked(locked); });
    }
    if (tx.isEmpty())
        return;
    tx.commit();
    emit selectionChanged(); // Griffe, Menüs und Eigenschaftsleiste zeigen den neuen Zustand
}

bool Editor::selectionHasLocked() const
{
    const QList<NodeId> ids = topLevelSelection(true);
    return std::any_of(ids.begin(), ids.end(), [this](NodeId id) { return isLocked(id); });
}

bool Editor::selectionHasUnlocked() const
{
    return !topLevelSelection().isEmpty();
}

void Editor::reorder(Transaction& tx, NodeId parent, const QList<NodeId>& order)
{
    // Von vorn nach hinten: Platz i bekommt sein Objekt, davor steht schon alles richtig
    for (int i = 0; i < order.size(); ++i) {
        if (m_doc->find(order[i])->indexInParent() != i)
            tx.move(order[i], parent, i);
    }
}

bool Editor::canArrange(Arrange how) const
{
    const QList<NodeId> ids = topLevelSelection();
    if (ids.isEmpty())
        return false;
    if (how == Arrange::Reverse)
        return ids.size() >= 2;
    // Nur anbieten, wenn sich etwas ändern würde (wie die grauen Einträge)
    for (NodeId id : ids) {
        const Node* n = m_doc->find(id);
        const Node* parent = n->parent();
        const int index = n->indexInParent();
        auto unselectedAt = [&](int i) {
            return i >= 0 && i < parent->childCount() && !ids.contains(parent->child(i)->id());
        };
        bool otherLayer = false;
        if (node_cast<Layer>(parent) && (how == Arrange::ToPageFront || how == Arrange::ToPageBack)) {
            const Node* page = parent->parent();
            otherLayer = parent != page->child(how == Arrange::ToPageFront ? page->childCount() - 1 : 0);
        }
        switch (how) {
        case Arrange::ToPageFront:
        case Arrange::ToLayerFront:
            for (int i = index + 1; i < parent->childCount(); ++i)
                otherLayer = otherLayer || unselectedAt(i);
            break;
        case Arrange::ToPageBack:
        case Arrange::ToLayerBack:
            for (int i = 0; i < index; ++i)
                otherLayer = otherLayer || unselectedAt(i);
            break;
        case Arrange::ForwardOne:
            otherLayer = unselectedAt(index + 1);
            break;
        case Arrange::BackOne:
            otherLayer = unselectedAt(index - 1);
            break;
        case Arrange::Reverse:
            break;
        }
        if (otherLayer)
            return true;
    }
    return false;
}

void Editor::arrangeSelection(Arrange how)
{
    const QList<NodeId> ids = topLevelSelection();
    if (ids.isEmpty())
        return;
    QString label;
    switch (how) {
    case Arrange::ToPageFront: label = tr("Nach vorn auf der Seite"); break;
    case Arrange::ToPageBack: label = tr("Nach hinten auf der Seite"); break;
    case Arrange::ToLayerFront: label = tr("Nach vorn auf der Ebene"); break;
    case Arrange::ToLayerBack: label = tr("Nach hinten auf der Ebene"); break;
    case Arrange::ForwardOne: label = tr("Eins nach vorn"); break;
    case Arrange::BackOne: label = tr("Eins nach hinten"); break;
    case Arrange::Reverse: label = tr("Reihenfolge umkehren"); break;
    }
    Transaction tx(*m_doc, label);
    // Auf der Seite: zuerst in die oberste bzw. unterste Ebene der Seite. In Gruppen bleibt
    // ein Objekt in seiner Gruppe.
    if (how == Arrange::ToPageFront || how == Arrange::ToPageBack) {
        for (NodeId id : ids) {
            const Node* parent = m_doc->find(id)->parent();
            if (!node_cast<Layer>(parent))
                continue;
            const Node* page = parent->parent();
            const Node* target = page->child(how == Arrange::ToPageFront ? page->childCount() - 1 : 0);
            if (target != parent)
                tx.move(id, target->id(), target->childCount());
        }
    }
    QList<NodeId> parents;
    for (NodeId id : ids) {
        const NodeId p = m_doc->find(id)->parent()->id();
        if (!parents.contains(p))
            parents.append(p);
    }
    for (NodeId pid : parents) {
        const Node* parent = m_doc->find(pid);
        QList<NodeId> order;
        QList<NodeId> chosen;
        QList<NodeId> rest;
        for (int i = 0; i < parent->childCount(); ++i) {
            const NodeId id = parent->child(i)->id();
            order.append(id);
            (ids.contains(id) ? chosen : rest).append(id);
        }
        auto selected = [&ids](NodeId id) { return ids.contains(id); };
        switch (how) {
        case Arrange::ToPageFront:
        case Arrange::ToLayerFront:
            order = rest + chosen;
            break;
        case Arrange::ToPageBack:
        case Arrange::ToLayerBack:
            order = chosen + rest;
            break;
        case Arrange::ForwardOne:
            for (int i = int(order.size()) - 2; i >= 0; --i) {
                if (selected(order[i]) && !selected(order[i + 1]))
                    order.swapItemsAt(i, i + 1);
            }
            break;
        case Arrange::BackOne:
            for (int i = 1; i < order.size(); ++i) {
                if (selected(order[i]) && !selected(order[i - 1]))
                    order.swapItemsAt(i, i - 1);
            }
            break;
        case Arrange::Reverse: {
            qsizetype k = chosen.size();
            for (NodeId& id : order) {
                if (selected(id))
                    id = chosen[--k];
            }
            break;
        }
        }
        reorder(tx, pid, order);
    }
    if (tx.isEmpty())
        return;
    tx.commit();
}

void Editor::placeSelectionRelativeTo(NodeId target, bool inFront)
{
    const Node* t = m_doc->find(target);
    if (!t)
        return;
    QList<NodeId> ids;
    for (NodeId id : topLevelSelection()) {
        // Eine Gruppe lässt sich nicht vor ihr eigenes Kind legen
        if (id != target && !m_doc->find(id)->isAncestorOf(t))
            ids.append(id);
    }
    if (ids.isEmpty())
        return;
    const NodeId pid = t->parent()->id();
    Transaction tx(*m_doc, inFront ? tr("Vor Objekt") : tr("Hinter Objekt"));
    for (NodeId id : ids) {
        if (m_doc->find(id)->parent()->id() != pid)
            tx.move(id, pid, m_doc->find(pid)->childCount());
    }
    const Node* parent = m_doc->find(pid);
    QList<NodeId> order;
    for (int i = 0; i < parent->childCount(); ++i) {
        const NodeId id = parent->child(i)->id();
        if (!ids.contains(id))
            order.append(id);
    }
    qsizetype at = order.indexOf(target) + (inFront ? 1 : 0);
    for (NodeId id : ids)
        order.insert(at++, id);
    reorder(tx, pid, order);
    if (tx.isEmpty())
        return;
    tx.commit();
}

bool Editor::canCombineSelection() const
{
    const QList<NodeId> ids = topLevelSelection();
    return std::count_if(ids.begin(), ids.end(), [this](NodeId id) { return !m_doc->findAs<GroupObject>(id); }) >= 2;
}

void Editor::combineSelection()
{
    QList<NodeId> ids;
    for (NodeId id : topLevelSelection()) {
        if (!m_doc->findAs<GroupObject>(id))
            ids.append(id);
    }
    if (ids.size() < 2)
        return;
    // Alle Linienzüge in einem Kurvenobjekt (Seitenkoordinaten); Füllung und Kontur vom
    // obersten Objekt
    QList<SubPath> paths;
    for (NodeId id : ids)
        paths += CurveObject::fromPath(m_doc->findAs<DrawObject>(id)->path())->subPaths();
    const DrawObject* top = m_doc->findAs<DrawObject>(ids.last());
    auto combined = std::make_unique<CurveObject>(paths);
    combined->setStyle(top->style());
    combined->copyEffects(*top);
    combined->setName(top->name());
    Transaction tx(*m_doc, tr("Kombinieren"));
    for (int i = 0; i + 1 < ids.size(); ++i)
        tx.remove(ids[i]);
    const NodeId parent = top->parent()->id();
    const int index = top->indexInParent();
    tx.remove(ids.last());
    const NodeId id = tx.insert(parent, index, std::move(combined));
    tx.commit();
    setSelection({id});
}

bool Editor::canBreakApartSelection() const
{
    for (NodeId id : topLevelSelection()) {
        const auto* c = m_doc->findAs<CurveObject>(id);
        if (c && c->subPaths().size() > 1)
            return true;
    }
    return false;
}

void Editor::breakApartSelection()
{
    QList<NodeId> targets;
    for (NodeId id : topLevelSelection()) {
        const auto* c = m_doc->findAs<CurveObject>(id);
        if (c && c->subPaths().size() > 1)
            targets.append(id);
    }
    // Jeder Linienzug wird ein eigenes Objekt mit Lage, Stil und Medien des Originals
    replaceObjects(targets, [](const DrawObject& o) -> Replacement {
        const auto& c = static_cast<const CurveObject&>(o);
        std::vector<std::unique_ptr<DrawObject>> out;
        for (const SubPath& s : c.subPaths()) {
            auto piece = std::make_unique<CurveObject>(QList<SubPath>{s});
            piece->setTransform(c.transform());
            piece->setStyle(c.style());
            piece->copyEffects(c);
            piece->setMedia(c.media());
            out.push_back(std::move(piece));
        }
        return out;
    }, tr("Kombination aufheben"));
}

void Editor::duplicateSelection()
{
    const QList<NodeId> ids = topLevelSelection(true);
    if (ids.isEmpty())
        return;
    remember(tr("Duplizieren"), [this] { duplicateSelection(); });
    QList<NodeId> copies;
    Transaction tx(*m_doc, tr("Duplizieren"));
    for (NodeId id : ids) {
        const Node* original = m_doc->find(id);
        std::unique_ptr<Node> copy = serial::fromJson(serial::toJson(*original, serial::Ids::Drop), serial::Ids::Drop);
        if (auto* obj = node_cast<DrawObject>(copy.get()))
            obj->applyTransform(xform::translation(duplicateOffset()));
        copies.append(tx.insert(original->parent()->id(), -1, std::move(copy)));
    }
    tx.commit();
    setSelection(copies);
}

void Editor::duplicateSelectionWith(const QTransform& m, const QString& label)
{
    const QList<NodeId> ids = topLevelSelection(true);
    if (ids.isEmpty())
        return;
    QList<NodeId> copies;
    Transaction tx(*m_doc, label);
    for (NodeId id : ids) {
        const Node* original = m_doc->find(id);
        std::unique_ptr<Node> copy = serial::fromJson(serial::toJson(*original, serial::Ids::Drop), serial::Ids::Drop);
        if (auto* obj = node_cast<DrawObject>(copy.get()))
            obj->applyTransform(m);
        copies.append(tx.insert(original->parent()->id(), -1, std::move(copy)));
    }
    tx.commit();
    setSelection(copies);
}

void Editor::repeatLast()
{
    if (m_repeat) {
        const std::function<void()> fn = m_repeat; // fn trägt sich selbst wieder ein
        fn();
    }
}

void Editor::cloneSelection()
{
    const QList<NodeId> ids = topLevelSelection();
    if (ids.isEmpty())
        return;
    QList<NodeId> clones;
    Transaction tx(*m_doc, tr("Klonen"));
    for (NodeId id : ids) {
        if (node_cast<CloneObject>(m_doc->find(id)))
            continue; // Klon eines Klons gibt es nicht
        const Node* original = m_doc->find(id);
        auto clone = std::make_unique<CloneObject>(id, false);
        clone->setTransform(xform::translation(duplicateOffset()));
        clones << tx.insert(original->parent()->id(), -1, std::move(clone));
    }
    if (clones.isEmpty())
        return;
    tx.commit();
    setSelection(clones);
}

void Editor::convertClonesToObjects()
{
    QList<NodeId> result;
    Transaction tx(*m_doc, tr("In Objekte zurückwandeln"));
    for (NodeId id : topLevelSelection()) {
        const auto* clone = m_doc->findAs<CloneObject>(id);
        if (!clone) {
            result << id;
            continue;
        }
        std::unique_ptr<DrawObject> obj = clone->detach();
        if (!obj)
            continue;
        const NodeId parent = clone->parent()->id();
        const int index = clone->indexInParent();
        tx.remove(id);
        result << tx.insert(parent, index, std::move(obj));
    }
    if (tx.isEmpty())
        return;
    tx.commit();
    setSelection(result);
}

NodeId Editor::symbolLayer(Transaction* tx)
{
    const Page& master = m_doc->masterPage();
    for (int i = 0; i < master.childCount(); ++i)
        if (const auto* l = node_cast<Layer>(master.child(i)); l && l->name() == QLatin1String("Symbole") && !l->visible())
            return l->id();
    if (!tx)
        return InvalidNodeId;
    auto layer = std::make_unique<Layer>();
    layer->setName(QStringLiteral("Symbole"));
    layer->setVisible(false);
    layer->setPrintable(false);
    layer->setLocked(true);
    return tx->insert(master.id(), -1, std::move(layer));
}

QList<NodeId> Editor::symbols() const
{
    QList<NodeId> ids;
    const NodeId layer = const_cast<Editor*>(this)->symbolLayer(nullptr);
    if (const Node* l = m_doc->find(layer))
        for (int i = 0; i < l->childCount(); ++i)
            ids << l->child(i)->id();
    return ids;
}

int Editor::symbolUses(NodeId symbol) const
{
    int n = 0;
    std::function<void(const Node&)> visit = [&](const Node& node) {
        if (const auto* c = node_cast<CloneObject>(&node); c && c->master() == symbol)
            ++n;
        for (int i = 0; i < node.childCount(); ++i)
            visit(*node.child(i));
    };
    visit(m_doc->root());
    return n;
}

NodeId Editor::createSymbol(const QString& name)
{
    const QList<NodeId> ids = topLevelSelection();
    if (ids.isEmpty())
        return InvalidNodeId;
    Transaction tx(*m_doc, tr("Neues Symbol"));
    const NodeId layer = symbolLayer(&tx);
    const Node* front = m_doc->find(ids.last());
    const NodeId parent = front->parent()->id();
    const int index = front->indexInParent();
    NodeId master = ids.first();
    if (ids.size() > 1) {
        auto group = std::make_unique<GroupObject>();
        master = tx.insert(layer, -1, std::move(group));
        for (NodeId id : ids)
            tx.move(id, master, -1);
    } else {
        tx.move(master, layer, -1);
    }
    tx.modify<DrawObject>(master, [&name](DrawObject& o) { o.setName(name); });
    // Instanz an der alten Stelle; Symbolkoordinaten = Seitenkoordinaten der Vorlage
    const NodeId instance = tx.insert(parent, std::min(index, m_doc->find(parent)->childCount()),
                                      std::make_unique<CloneObject>(master, true));
    tx.commit();
    setSelection({instance});
    return master;
}

NodeId Editor::placeSymbol(NodeId symbol, const QPointF& center)
{
    const DrawObject* master = m_doc->findAs<DrawObject>(symbol);
    const NodeId layer = activeLayer();
    if (!master || layer == InvalidNodeId)
        return InvalidNodeId;
    const QPointF c = master->boundingRect().center();
    auto instance = std::make_unique<CloneObject>(symbol, true);
    instance->setTransform(QTransform::fromTranslate(center.x() - c.x(), center.y() - c.y()));
    Transaction tx(*m_doc, tr("Symbol einfügen"));
    const NodeId id = tx.insert(layer, -1, std::move(instance));
    tx.commit();
    setSelection({id});
    return id;
}

bool Editor::deleteSymbol(NodeId symbol)
{
    if (!symbols().contains(symbol) || symbolUses(symbol) > 0)
        return false;
    Transaction tx(*m_doc, tr("Symbol löschen"));
    tx.remove(symbol);
    tx.commit();
    return true;
}

void Editor::renameSymbol(NodeId symbol, const QString& name)
{
    if (!symbols().contains(symbol))
        return;
    Transaction tx(*m_doc, tr("Symbol umbenennen"));
    tx.modify<Node>(symbol, [&name](Node& n) { n.setName(name); });
    tx.commit();
}

NodeId Editor::placeInsideFrame(NodeId frameId)
{
    const DrawObject* frameObj = m_doc->findAs<DrawObject>(frameId);
    QList<NodeId> contents;
    for (NodeId id : topLevelSelection())
        if (id != frameId)
            contents << id;
    if (!frameObj || contents.isEmpty() || isLocked(frameId))
        return InvalidNodeId;
    Transaction tx(*m_doc, tr("In Rahmen platzieren"));
    NodeId clip = frameId;
    if (!node_cast<SuperClipObject>(frameObj)) {
        const NodeId parent = frameObj->parent()->id();
        const int index = frameObj->indexInParent();
        clip = tx.insert(parent, index, std::make_unique<SuperClipObject>());
        tx.move(frameId, clip, 0);
    }
    // Inhalt automatisch zentrieren (Optionen > Bearbeiten > SuperClip-Rahmen): 0 = wenn ganz
    // ausserhalb, 1 = immer, 2 = nie
    const int center = Options::instance().integer(QStringLiteral("ws/edit/superclipCenter"));
    const QRectF f = m_doc->findAs<DrawObject>(clip)->boundingRect();
    for (NodeId id : contents) {
        const QRectF r = m_doc->findAs<DrawObject>(id)->boundingRect();
        const bool outside = !r.intersects(f);
        tx.move(id, clip, -1);
        if (center == 1 || (center == 0 && outside)) {
            const QPointF d = f.center() - r.center();
            const QTransform m = QTransform::fromTranslate(d.x(), d.y());
            tx.modifySubtree<DrawObject>(id, [&m](DrawObject& o) { o.applyTransform(m); });
        }
    }
    tx.commit();
    setSelection({clip});
    return clip;
}

void Editor::extractContents()
{
    QList<NodeId> result;
    Transaction tx(*m_doc, tr("Inhalt extrahieren"));
    for (NodeId id : topLevelSelection()) {
        const auto* clip = m_doc->findAs<SuperClipObject>(id);
        if (!clip)
            continue;
        const NodeId parent = clip->parent()->id();
        int index = clip->indexInParent();
        // Rahmen an die Stelle des SuperClips, Inhalte darüber
        const NodeId frameId = clip->frame() ? clip->frame()->id() : InvalidNodeId;
        while (clip->childCount() > 0) {
            const NodeId c = clip->child(0)->id();
            tx.move(c, parent, ++index);
            if (c != frameId)
                result << c;
        }
        tx.remove(id);
    }
    if (tx.isEmpty())
        return;
    tx.commit();
    setSelection(result);
}

void Editor::createEmptySuperClipFrame()
{
    QList<NodeId> result;
    Transaction tx(*m_doc, tr("Leeren SuperClip-Rahmen erstellen"));
    for (NodeId id : topLevelSelection()) {
        const DrawObject* o = m_doc->findAs<DrawObject>(id);
        if (!o || node_cast<SuperClipObject>(o))
            continue;
        const NodeId clip = tx.insert(o->parent()->id(), o->indexInParent(), std::make_unique<SuperClipObject>());
        tx.move(id, clip, 0);
        result << clip;
    }
    if (result.isEmpty())
        return;
    tx.commit();
    setSelection(result);
}

void Editor::fitSuperClipContents(ClipFit fit)
{
    Transaction tx(*m_doc, tr("Inhalt anpassen"));
    for (NodeId id : topLevelSelection()) {
        const auto* clip = m_doc->findAs<SuperClipObject>(id);
        if (!clip || !clip->frame() || clip->contents().isEmpty())
            continue;
        const QRectF f = clip->frame()->boundingRect();
        QRectF c;
        for (const DrawObject* o : clip->contents())
            c = c.isNull() ? o->boundingRect() : c.united(o->boundingRect());
        if (c.width() <= 0 || c.height() <= 0)
            continue;
        double sx = 1.0, sy = 1.0;
        switch (fit) {
        case ClipFit::Center: break;
        case ClipFit::FitProportional: sx = sy = std::min(f.width() / c.width(), f.height() / c.height()); break;
        case ClipFit::FillProportional: sx = sy = std::max(f.width() / c.width(), f.height() / c.height()); break;
        case ClipFit::Stretch:
            sx = f.width() / c.width();
            sy = f.height() / c.height();
            break;
        }
        const QTransform m = QTransform::fromTranslate(-c.center().x(), -c.center().y()) * QTransform::fromScale(sx, sy)
                             * QTransform::fromTranslate(f.center().x(), f.center().y());
        for (const DrawObject* o : clip->contents())
            tx.modifySubtree<DrawObject>(o->id(), [&m](DrawObject& d) { d.applyTransform(m); });
    }
    if (!tx.isEmpty())
        tx.commit();
}

void Editor::setSuperClipContentsLocked(bool locked)
{
    Transaction tx(*m_doc, locked ? tr("Inhalt in SuperClip sperren") : tr("Inhalt in SuperClip freigeben"));
    for (NodeId id : topLevelSelection())
        if (const auto* clip = m_doc->findAs<SuperClipObject>(id); clip && clip->contentsLocked() != locked)
            tx.modify<SuperClipObject>(id, [locked](SuperClipObject& c) { c.setContentsLocked(locked); });
    if (!tx.isEmpty())
        tx.commit();
}

void Editor::setSelectionLens(const Lens& lens)
{
    Transaction tx(*m_doc, tr("Linse"));
    for (NodeId id : topLevelSelection())
        tx.modify<DrawObject>(id, [&lens](DrawObject& o) { o.setLens(lens); });
    if (!tx.isEmpty())
        tx.commit();
}

void Editor::setSelectionBevel(const Bevel& bevel)
{
    Transaction tx(*m_doc, tr("Abschrägung"));
    for (NodeId id : topLevelSelection())
        tx.modify<DrawObject>(id, [&bevel](DrawObject& o) { o.setBevel(bevel); });
    if (!tx.isEmpty())
        tx.commit();
}

NodeId Editor::blendObjects(NodeId from, NodeId to, int steps)
{
    const DrawObject* a = m_doc->findAs<DrawObject>(from);
    const DrawObject* b = m_doc->findAs<DrawObject>(to);
    if (!a || !b || from == to || isLocked(from) || isLocked(to))
        return InvalidNodeId;
    Transaction tx(*m_doc, tr("Überblendung"));
    const NodeId parent = b->parent()->id();
    const int index = b->indexInParent();
    auto blend = std::make_unique<BlendObject>();
    blend->setSteps(steps);
    const NodeId id = tx.insert(parent, index, std::move(blend));
    tx.move(from, id, 0);
    tx.move(to, id, 1);
    tx.commit();
    setSelection({id});
    return id;
}

void Editor::setObjectContour(NodeId id, const ContourEffect& c, const QString& label)
{
    const DrawObject* o = m_doc->findAs<DrawObject>(id);
    if (!o || isLocked(id) || o->contour() == c)
        return;
    Transaction tx(*m_doc, label);
    tx.modify<DrawObject>(id, [&c](DrawObject& d) { d.setContour(c); });
    tx.commit();
}

void Editor::setSelectionContour(const ContourEffect& c, const QString& label)
{
    Transaction tx(*m_doc, label);
    for (NodeId id : topLevelSelection())
        if (m_doc->findAs<DrawObject>(id)->contour() != c)
            tx.modify<DrawObject>(id, [&c](DrawObject& d) { d.setContour(c); });
    if (!tx.isEmpty())
        tx.commit();
}

void Editor::setObjectExtrusion(NodeId id, const Extrusion& e, const QString& label)
{
    const DrawObject* o = m_doc->findAs<DrawObject>(id);
    if (!o || isLocked(id) || o->extrusion() == e)
        return;
    Transaction tx(*m_doc, label);
    tx.modify<DrawObject>(id, [&e](DrawObject& d) { d.setExtrusion(e); });
    tx.commit();
}

void Editor::setSelectionExtrusion(const Extrusion& e, const QString& label)
{
    Transaction tx(*m_doc, label);
    for (NodeId id : topLevelSelection())
        if (m_doc->findAs<DrawObject>(id)->extrusion() != e)
            tx.modify<DrawObject>(id, [&e](DrawObject& d) { d.setExtrusion(e); });
    if (!tx.isEmpty())
        tx.commit();
}

NodeId Editor::replaceObjectWithCurve(NodeId id, const QList<SubPath>& paths, const QString& label)
{
    const DrawObject* o = m_doc->findAs<DrawObject>(id);
    if (!o || isLocked(id) || paths.isEmpty())
        return InvalidNodeId;
    if (node_cast<CurveObject>(o)) {
        Transaction tx(*m_doc, label);
        tx.modify<CurveObject>(id, [&paths](CurveObject& c) {
            c.setTransform(QTransform());
            c.setSubPaths(paths);
        });
        tx.commit();
        return id;
    }
    auto curve = std::make_unique<CurveObject>(paths);
    curve->setStyle(o->style());
    curve->copyEffects(*o);
    curve->setName(o->name());
    const NodeId parent = o->parent()->id();
    const int index = o->indexInParent();
    Transaction tx(*m_doc, label);
    tx.remove(id);
    const NodeId nid = tx.insert(parent, index, std::move(curve));
    tx.commit();
    setSelection({nid});
    return nid;
}

bool Editor::separateEffects()
{
    QList<NodeId> result;
    bool any = false;
    Transaction tx(*m_doc, tr("Trennen"));
    auto curveFrom = [](const QPainterPath& path, const Color& fill, std::optional<Color> stroke, double width) {
        auto c = CurveObject::fromPath(path);
        Style s;
        s.fill = Fill::solid(fill);
        s.stroke = stroke;
        s.strokeWidth = width;
        c->setStyle(s);
        return c;
    };
    for (NodeId id : topLevelSelection()) {
        const DrawObject* o = m_doc->findAs<DrawObject>(id);
        const NodeId parent = o->parent()->id();
        const int index = o->indexInParent();
        if (const auto* blend = node_cast<BlendObject>(o)) {
            // Gruppe aus Start, Zwischenstufen (Kurven), Ende
            const NodeId group = tx.insert(parent, index, std::make_unique<GroupObject>());
            const NodeId startId = blend->start() ? blend->start()->id() : InvalidNodeId;
            const NodeId endId = blend->end() ? blend->end()->id() : InvalidNodeId;
            const auto steps = blend->intermediates();
            if (startId)
                tx.move(startId, group, -1);
            for (const auto& s : steps) {
                auto c = std::make_unique<CurveObject>(s->subPaths());
                c->setStyle(s->style());
                tx.insert(group, -1, std::move(c));
            }
            if (endId)
                tx.move(endId, group, -1);
            tx.remove(id);
            result << group;
            any = true;
            continue;
        }
        if (o->contour().active || o->extrusion().active) {
            const NodeId group = tx.insert(parent, index, std::make_unique<GroupObject>());
            if (o->extrusion().active)
                for (const auto& [poly, color] : Canvas::extrusionFaces(*o, o->extrusion())) {
                    QPainterPath p;
                    p.addPolygon(poly);
                    p.closeSubpath();
                    tx.insert(group, -1, curveFrom(p, Color::fromQColor(color), Color::fromQColor(color.darker(130)), 0.1));
                }
            QList<Canvas::ContourStep> steps;
            if (o->contour().active)
                steps = Canvas::contourSteps(*o, o->contour());
            for (const Canvas::ContourStep& s : std::as_const(steps))
                if (!s.front)
                    tx.insert(group, -1, curveFrom(s.path, s.fill, s.stroke, o->style().strokeWidth));
            tx.move(id, group, -1);
            tx.modify<DrawObject>(id, [](DrawObject& d) {
                d.setContour(ContourEffect());
                d.setExtrusion(Extrusion());
            });
            for (const Canvas::ContourStep& s : std::as_const(steps))
                if (s.front)
                    tx.insert(group, -1, curveFrom(s.path, s.fill, s.stroke, o->style().strokeWidth));
            result << group;
            any = true;
            continue;
        }
        result << id;
    }
    if (!any)
        return false;
    tx.commit();
    setSelection(result);
    return true;
}

void Editor::setSelectionShadow(const DropShadow& shadow, const QString& label)
{
    Transaction tx(*m_doc, label);
    for (NodeId id : topLevelSelection())
        if (m_doc->findAs<DrawObject>(id)->shadow() != shadow)
            tx.modify<DrawObject>(id, [&shadow](DrawObject& o) { o.setShadow(shadow); });
    if (!tx.isEmpty())
        tx.commit();
}

void Editor::setObjectShadow(NodeId id, const DropShadow& shadow, const QString& label)
{
    const DrawObject* o = m_doc->findAs<DrawObject>(id);
    if (!o || isLocked(id) || o->shadow() == shadow)
        return;
    Transaction tx(*m_doc, label);
    tx.modify<DrawObject>(id, [&shadow](DrawObject& d) { d.setShadow(shadow); });
    tx.commit();
}

void Editor::clearSelectionEffects()
{
    Transaction tx(*m_doc, tr("Effekt löschen"));
    for (NodeId id : topLevelSelection())
        if (m_doc->findAs<DrawObject>(id)->hasEffects())
            tx.modify<DrawObject>(id, [](DrawObject& o) { o.clearEffects(); });
    if (!tx.isEmpty())
        tx.commit();
}

void Editor::copyEffectsFrom(NodeId source, const QString& label)
{
    const DrawObject* src = m_doc->findAs<DrawObject>(source);
    if (!src)
        return;
    Transaction tx(*m_doc, label);
    for (NodeId id : topLevelSelection())
        if (id != source)
            tx.modify<DrawObject>(id, [src](DrawObject& o) { o.copyEffects(*src); });
    if (!tx.isEmpty())
        tx.commit();
}

NodeId Editor::replaceSelectionWithImage(const QImage& pixels, const QRectF& bounds, const QString& label)
{
    const QList<NodeId> ids = topLevelSelection();
    if (ids.isEmpty() || pixels.isNull())
        return InvalidNodeId;
    const QString key = imagepool::addImage(pixels);
    if (key.isEmpty())
        return InvalidNodeId;
    auto image = std::make_unique<ImageObject>(key, bounds.size());
    image->setTransform(QTransform::fromTranslate(bounds.left(), bounds.top()));
    const Node* front = m_doc->find(ids.last());
    const NodeId parent = front->parent()->id();
    const int index = front->indexInParent();
    Transaction tx(*m_doc, label);
    for (NodeId id : ids)
        tx.remove(id);
    const NodeId id = tx.insert(parent, std::min(index, m_doc->find(parent)->childCount()), std::move(image));
    tx.commit();
    setSelection({id});
    return id;
}

bool Editor::cropBitmapToFrame(NodeId id)
{
    const auto* image = m_doc->findAs<ImageObject>(id);
    if (!image || !image->isCropped() || isLocked(id))
        return false;
    const QImage pixels = image->image();
    const QSizeF size = image->size();
    if (pixels.isNull() || size.width() <= 0 || size.height() <= 0)
        return false;
    // Rahmen (lokal, mm) auf das Bild begrenzen und in Pixel umrechnen
    const QRectF box = CurveObject::toPainterPath(image->frame()).boundingRect().intersected(QRectF(QPointF(0, 0), size));
    if (box.isEmpty())
        return false;
    const double sx = pixels.width() / size.width(), sy = pixels.height() / size.height();
    const QRect px = QRectF(box.left() * sx, box.top() * sy, box.width() * sx, box.height() * sy).toAlignedRect().intersected(pixels.rect());
    const QImage cropped = pixels.copy(px);
    const QSizeF newSize(px.width() / sx, px.height() / sy);
    const QPointF shift(px.left() / sx, px.top() / sy);
    const QString key = imagepool::addImage(cropped);
    QList<SubPath> frame = image->frame();
    for (SubPath& sp : frame)
        for (CurveNode& n : sp.nodes) {
            n.point -= shift;
            n.c1 -= shift;
            n.c2 -= shift;
        }
    Transaction tx(*m_doc, tr("Bitmap zuschneiden"));
    tx.modify<ImageObject>(id, [&](ImageObject& o) {
        o.setKey(key);
        o.setSize(newSize);
        o.setFrame(frame);
        o.setTransform(QTransform::fromTranslate(shift.x(), shift.y()) * o.transform());
    });
    tx.commit();
    return true;
}

NodeId Editor::textToTable(const QString& separator)
{
    const QList<const DrawObject*> objects = selectedObjects();
    const auto* text = objects.size() == 1 ? node_cast<TextObject>(objects.first()) : nullptr;
    if (!text)
        return InvalidNodeId;
    QList<QStringList> rowsText;
    int cols = 1;
    for (const QString& line : text->plainText().split(QLatin1Char('\n'))) {
        const QStringList cells = separator.isEmpty() ? QStringList{line} : line.split(separator);
        rowsText << cells;
        cols = std::max(cols, int(cells.size()));
    }
    const QRectF box = text->boundingRect();
    const int rowCount = std::max(1, int(rowsText.size()));
    auto table = std::make_unique<TableObject>(QSizeF(std::max(box.width(), cols * 25.0), rowCount * 8.0), rowCount, cols);
    if (!text->paragraphs().isEmpty() && !text->paragraphs().first().runs.isEmpty()) {
        CharFormat f = text->paragraphs().first().runs.first().format;
        f.sizePt = std::min(f.sizePt, 12.0);
        table->setTextFormat(f);
    }
    for (int r = 0; r < rowsText.size(); ++r)
        for (int c = 0; c < rowsText[r].size(); ++c)
            table->setCellText(r, c, rowsText[r][c].trimmed());
    table->setTransform(QTransform::fromTranslate(box.left(), box.top()));
    Transaction tx(*m_doc, tr("Text in Tabelle konvertieren"));
    const NodeId parent = text->parent()->id();
    const int index = text->indexInParent();
    const NodeId oldId = text->id();
    tx.remove(oldId);
    const NodeId id = tx.insert(parent, index, std::move(table));
    tx.commit();
    setSelection({id});
    return id;
}

NodeId Editor::tableToText(const QString& separator)
{
    const QList<const DrawObject*> objects = selectedObjects();
    const auto* table = objects.size() == 1 ? node_cast<TableObject>(objects.first()) : nullptr;
    if (!table)
        return InvalidNodeId;
    QStringList lines;
    for (int r = 0; r < table->rows(); ++r) {
        QStringList cells;
        for (int c = 0; c < table->columns(); ++c)
            if (!table->isCovered(r, c))
                cells << table->cell(r, c).text;
        lines << cells.join(separator.isEmpty() ? QStringLiteral("\t") : separator);
    }
    auto text = std::make_unique<TextObject>(TextObject::Kind::Paragraph, table->size());
    text->setPlainText(lines.join(QLatin1Char('\n')), table->textFormat());
    text->setTransform(table->transform());
    Style s;
    s.fill = Fill::solid(Color(0, 0, 0));
    s.stroke.reset();
    text->setStyle(s);
    Transaction tx(*m_doc, tr("Tabelle in Text konvertieren"));
    const NodeId parent = table->parent()->id();
    const int index = table->indexInParent();
    tx.remove(table->id());
    const NodeId id = tx.insert(parent, index, std::move(text));
    tx.commit();
    setSelection({id});
    return id;
}

void Editor::setObjectStroke(NodeId id, const std::optional<Color>& color, const QString& label)
{
    if (!m_doc->findAs<DrawObject>(id) || isLocked(id))
        return;
    Transaction tx(*m_doc, label);
    tx.modifySubtree<DrawObject>(id, [&color](DrawObject& o) {
        Style s = o.style();
        s.stroke = color;
        if (color && s.strokeWidth <= 0)
            s.strokeWidth = 0.2;
        o.setStyle(s);
    });
    tx.commit();
}

void Editor::applyAttributes(const DrawObject& source, NodeId target, int what)
{
    const DrawObject* t = m_doc->findAs<DrawObject>(target);
    if (!t || isLocked(target) || what == 0)
        return;
    Transaction tx(*m_doc, tr("Attribute übertragen"));
    if (what & 1) {
        std::optional<CharFormat> format;
        if (const auto* text = node_cast<TextObject>(&source); text && !text->paragraphs().isEmpty() && !text->paragraphs().first().runs.isEmpty())
            format = text->paragraphs().first().runs.first().format;
        const Style style = source.style();
        tx.modifySubtree<DrawObject>(target, [&](DrawObject& o) {
            o.setStyle(style);
            if (auto* text = node_cast<TextObject>(&o); text && format) {
                QList<TextParagraph> paragraphs = text->paragraphs();
                for (TextParagraph& p : paragraphs)
                    for (TextRun& r : p.runs)
                        r.format = *format;
                text->setParagraphs(paragraphs);
            }
        });
    }
    if (what & 2) {
        // Grösse und Drehung des Spenders, Mitte bleibt
        const QRectF r = t->boundingRect();
        const QPointF c = r.center();
        const double turn = source.rotationAngle() - t->rotationAngle();
        QTransform m = QTransform::fromTranslate(-c.x(), -c.y()) * QTransform().rotate(turn) * QTransform::fromTranslate(c.x(), c.y());
        tx.modifySubtree<DrawObject>(target, [&m](DrawObject& o) { o.applyTransform(m); });
        const QRectF now = m_doc->findAs<DrawObject>(target)->boundingRect();
        const QRectF want = source.boundingRect();
        if (now.width() > 0 && now.height() > 0 && want.width() > 0 && want.height() > 0) {
            const QPointF nc = now.center();
            const QTransform s = QTransform::fromTranslate(-nc.x(), -nc.y())
                                 * QTransform::fromScale(want.width() / now.width(), want.height() / now.height())
                                 * QTransform::fromTranslate(nc.x(), nc.y());
            tx.modifySubtree<DrawObject>(target, [&s](DrawObject& o) { o.applyTransform(s); });
        }
    }
    if (what & 4)
        tx.modify<DrawObject>(target, [&source](DrawObject& o) { o.copyEffects(source); });
    if (!tx.isEmpty())
        tx.commit();
}

void Editor::renameObject(NodeId id, const QString& name)
{
    const Node* n = m_doc->find(id);
    if (!n || n->name() == name)
        return;
    Transaction tx(*m_doc, tr("Umbenennen"));
    tx.modify<Node>(id, [&name](Node& node) { node.setName(name); });
    tx.commit();
}

void Editor::copyPropertiesFrom(NodeId source, int what)
{
    const DrawObject* src = m_doc->findAs<DrawObject>(source);
    if (!src || what == 0)
        return;
    QList<NodeId> targets = topLevelSelection();
    targets.removeAll(source);
    copyProperties(*src, targets, what);
}

void Editor::copyPropertiesTo(NodeId source, NodeId target, int what)
{
    const DrawObject* src = m_doc->findAs<DrawObject>(source);
    if (!src || what == 0 || source == target || !m_doc->findAs<DrawObject>(target) || isLocked(target))
        return;
    copyProperties(*src, {target}, what);
}

void Editor::copyProperties(const DrawObject& src, const QList<NodeId>& targets, int what)
{
    const Style from = src.style();
    std::optional<CharFormat> textFormat;
    if (const auto* t = node_cast<TextObject>(&src); t && !t->paragraphs().isEmpty() && !t->paragraphs().first().runs.isEmpty())
        textFormat = t->paragraphs().first().runs.first().format;
    Transaction tx(*m_doc, tr("Eigenschaften kopieren"));
    for (NodeId id : targets) {
        tx.modifySubtree<DrawObject>(id, [&](DrawObject& o) {
            Style s = o.style();
            if (what & CopyOutlinePen) {
                s.strokeWidth = from.strokeWidth;
                s.dash = from.dash;
                s.startArrow = from.startArrow;
                s.endArrow = from.endArrow;
            }
            if (what & CopyOutlineColor)
                s.stroke = from.stroke;
            if (what & CopyFill)
                s.fill = from.fill;
            o.setStyle(s);
            if ((what & CopyText) && textFormat) {
                if (auto* t = node_cast<TextObject>(&o)) {
                    QList<TextParagraph> paragraphs = t->paragraphs();
                    for (TextParagraph& p : paragraphs)
                        for (TextRun& r : p.runs)
                            r.format = *textFormat;
                    t->setParagraphs(paragraphs);
                }
            }
        });
    }
    if (!tx.isEmpty())
        tx.commit();
}

void Editor::selectAllText()
{
    selectWhere([](const DrawObject& o) { return node_cast<TextObject>(&o) != nullptr; });
}

int Editor::selectWhere(const std::function<bool(const DrawObject&)>& pred)
{
    QList<NodeId> ids;
    for (const DrawObject* o : pickableObjects())
        if (pred(*o))
            ids << o->id();
    setSelection(ids);
    return int(ids.size());
}

int Editor::replaceText(const QString& find, const QString& with, bool caseSensitive, bool wholeWords)
{
    if (find.isEmpty())
        return 0;
    QTextDocument::FindFlags flags;
    if (caseSensitive)
        flags |= QTextDocument::FindCaseSensitively;
    if (wholeWords)
        flags |= QTextDocument::FindWholeWords;
    int count = 0;
    Transaction tx(*m_doc, tr("Text ersetzen"));
    std::function<void(const Node&)> visit = [&](const Node& n) {
        if (const auto* t = node_cast<TextObject>(&n)) {
            const QList<NodeId> chain = t->chain();
            if (!chain.isEmpty() && chain.first() != t->id())
                return;
            std::unique_ptr<QTextDocument> d = t->toDocument();
            int here = 0;
            QTextCursor c(d.get());
            while (true) {
                c = d->find(find, c, flags);
                if (c.isNull())
                    break;
                c.insertText(with);
                ++here;
            }
            if (here > 0) {
                count += here;
                tx.modify<TextObject>(t->id(), [&d](TextObject& o) { o.setFromDocument(*d); });
            }
            return;
        }
        for (int i = 0; i < n.childCount(); ++i)
            visit(*n.child(i));
    };
    for (int p = 0; p < m_doc->pageCount(); ++p)
        visit(m_doc->page(p));
    visit(m_doc->masterPage());
    if (count > 0)
        tx.commit();
    return count;
}

void Editor::ungroupAll()
{
    QList<NodeId> released;
    Transaction tx(*m_doc, tr("Gruppierung aller Objekte aufheben"));
    std::function<void(NodeId)> release = [&](NodeId id) {
        const auto* group = m_doc->findAs<GroupObject>(id);
        if (!group) {
            released.append(id);
            return;
        }
        const NodeId parentId = group->parent()->id();
        int index = group->indexInParent();
        QList<NodeId> children;
        while (group->childCount() > 0) {
            const NodeId childId = group->child(0)->id();
            tx.move(childId, parentId, index++);
            children.append(childId);
        }
        tx.remove(id);
        for (NodeId c : children)
            release(c);
    };
    for (NodeId id : topLevelSelection())
        release(id);
    if (tx.isEmpty())
        return;
    tx.commit();
    setSelection(released);
}

void Editor::unlockAll()
{
    Transaction tx(*m_doc, tr("Alle Objekte freigeben"));
    std::function<void(const Node&)> visit = [&](const Node& n) {
        if (const auto* o = node_cast<DrawObject>(&n); o && o->locked())
            tx.modify<DrawObject>(o->id(), [](DrawObject& d) { d.setLocked(false); });
        for (int i = 0; i < n.childCount(); ++i)
            visit(*n.child(i));
    };
    visit(currentPage());
    visit(m_doc->masterPage());
    if (tx.isEmpty())
        return;
    tx.commit();
    emit selectionChanged();
}

void Editor::clearTransformations()
{
    Transaction tx(*m_doc, tr("Änderungen zurücknehmen"));
    for (NodeId id : topLevelSelection()) {
        const DrawObject* o = m_doc->findAs<DrawObject>(id);
        if (!o || node_cast<GroupObject>(o))
            continue;
        const QTransform t = o->transform();
        if (t.type() <= QTransform::TxTranslate)
            continue;
        // Mitte bleibt, Drehung/Neigung/Spiegelung/Streckung fallen weg
        const QPointF localCenter = o->localPath().boundingRect().center();
        const QPointF c = t.map(localCenter);
        const QTransform plain = QTransform::fromTranslate(c.x() - localCenter.x(), c.y() - localCenter.y());
        tx.modify<DrawObject>(id, [&plain](DrawObject& d) { d.setTransform(plain); });
    }
    if (!tx.isEmpty())
        tx.commit();
}

bool Editor::outlineToObject()
{
    QList<NodeId> created;
    Transaction tx(*m_doc, tr("Umriss in Objekt umwandeln"));
    for (NodeId id : topLevelSelection()) {
        const DrawObject* o = m_doc->findAs<DrawObject>(id);
        if (!o || !o->style().stroke || o->style().strokeWidth <= 0 || node_cast<GroupObject>(o))
            continue;
        const Style& s = o->style();
        // Kontur als Fläche: Strich (mit Linienstil) und Pfeilspitzen
        const outline::Stroke st = outline::withArrows(o->path(), s);
        QPainterPathStroker stroker;
        stroker.setWidth(s.strokeWidth);
        stroker.setJoinStyle(Qt::MiterJoin);
        const QList<double> dashes = outline::dashPattern(s.dash);
        stroker.setCapStyle(dashes.isEmpty() ? Qt::SquareCap : Qt::FlatCap);
        if (!dashes.isEmpty())
            stroker.setDashPattern(QList<qreal>(dashes.begin(), dashes.end()));
        QPainterPath area = stroker.createStroke(st.line).united(st.heads).simplified();
        auto shape = CurveObject::fromPath(area);
        Style fill;
        fill.fill = Fill::solid(*s.stroke);
        fill.stroke.reset();
        fill.strokeWidth = 0.0;
        shape->setStyle(fill);
        shape->setName(tr("Umriss"));
        const NodeId newId = tx.insert(o->parent()->id(), o->indexInParent() + 1, std::move(shape));
        created << newId;
        // Das Objekt selbst behält nur die Füllung; ohne Füllung verschwindet es
        if (s.fill.isNone()) {
            tx.remove(id);
        } else {
            tx.modify<DrawObject>(id, [](DrawObject& d) {
                Style ns = d.style();
                ns.stroke.reset();
                d.setStyle(ns);
            });
        }
    }
    if (created.isEmpty())
        return false;
    tx.commit();
    setSelection(created);
    return true;
}

bool Editor::joinCurves(double tolerance)
{
    // Offene Linienzüge der gewählten Kurven (Seitenkoordinaten)
    QList<NodeId> ids;
    QList<SubPath> open, closed;
    Style style;
    for (NodeId id : topLevelSelection()) {
        const auto* c = m_doc->findAs<CurveObject>(id);
        if (!c)
            continue;
        if (ids.isEmpty())
            style = c->style();
        ids << id;
        const QList<SubPath> parts = CurveObject::fromPath(c->path())->subPaths(); // Kopie: Temporäres lebt nicht weiter
        for (const SubPath& sp : parts)
            (sp.closed ? closed : open) << sp;
    }
    if (open.isEmpty())
        return false;
    auto dist = [](const QPointF& a, const QPointF& b) { return std::hypot(a.x() - b.x(), a.y() - b.y()); };
    auto reversed = [](const SubPath& sp) {
        CurveObject tmp(QList<SubPath>{sp});
        tmp.reverse(0);
        return tmp.subPaths().first();
    };
    auto append = [](SubPath& a, const SubPath& b) {
        // b beginnt am Ende von a: Anfangsknoten von b fällt mit dem Ende von a zusammen
        for (int i = 1; i < b.nodes.size(); ++i)
            a.nodes.append(b.nodes[i]);
    };
    bool merged = true;
    while (merged) {
        merged = false;
        for (int i = 0; i < open.size() && !merged; ++i) {
            for (int j = 0; j < open.size() && !merged; ++j) {
                if (i == j)
                    continue;
                SubPath& a = open[i];
                const SubPath b = open[j];
                const QPointF aEnd = a.nodes.last().point;
                if (dist(aEnd, b.nodes.first().point) <= tolerance) {
                    append(a, b);
                } else if (dist(aEnd, b.nodes.last().point) <= tolerance) {
                    append(a, reversed(b));
                } else {
                    continue;
                }
                open.removeAt(j);
                merged = true;
            }
        }
    }
    bool changed = false;
    for (SubPath& sp : open) {
        if (sp.nodes.size() > 2 && dist(sp.nodes.first().point, sp.nodes.last().point) <= tolerance) {
            // Ende auf den Anfang: geschlossen, das Schlusssegment gehört zum ersten Knoten
            const CurveNode last = sp.nodes.takeLast();
            sp.nodes.first().curveIn = last.curveIn;
            sp.nodes.first().c1 = last.c1;
            sp.nodes.first().c2 = last.c2;
            sp.closed = true;
            changed = true;
        }
    }
    int before = 0;
    for (NodeId id : ids)
        before += int(CurveObject::fromPath(m_doc->findAs<CurveObject>(id)->path())->subPaths().size());
    if (!changed && open.size() + closed.size() == before)
        return false;
    auto joined = std::make_unique<CurveObject>(closed + open);
    joined->setStyle(style);
    Transaction tx(*m_doc, tr("Kurven verbinden"));
    const Node* first = m_doc->find(ids.first());
    const NodeId parent = first->parent()->id();
    const int index = first->indexInParent();
    for (NodeId id : ids)
        tx.remove(id);
    const NodeId id = tx.insert(parent, std::min(index, m_doc->find(parent)->childCount()), std::move(joined));
    tx.commit();
    setSelection({id});
    return true;
}

void Editor::transformSelectionCopies(const QTransform& m, int copies, const QString& label)
{
    const QList<NodeId> ids = topLevelSelection(copies > 0);
    if (ids.isEmpty() || m.isIdentity())
        return;
    remember(label, [this, m, copies, label] { transformSelectionCopies(m, copies, label); });
    Transaction tx(*m_doc, label);
    if (copies <= 0) {
        for (NodeId id : ids)
            tx.modifySubtree<DrawObject>(id, [&m](DrawObject& o) { o.applyTransform(m); });
        tx.commit();
        return;
    }
    QList<NodeId> current = ids;
    for (int c = 0; c < copies; ++c) {
        QList<NodeId> next;
        for (NodeId id : current) {
            const Node* original = m_doc->find(id);
            std::unique_ptr<Node> copy = serial::fromJson(serial::toJson(*original, serial::Ids::Drop), serial::Ids::Drop);
            if (auto* obj = node_cast<DrawObject>(copy.get()))
                obj->applyTransform(m);
            next << tx.insert(original->parent()->id(), -1, std::move(copy));
        }
        current = next;
    }
    tx.commit();
    setSelection(current);
}

void Editor::groupSelection()
{
    const QList<NodeId> ids = topLevelSelection();
    if (ids.size() < 2)
        return;
    // Die Gruppe kommt an die Stelle des vordersten Objekts
    const Node* front = m_doc->find(ids.last());
    const NodeId parentId = front->parent()->id();
    Transaction tx(*m_doc, tr("Gruppieren"));
    const NodeId group = tx.insert(parentId, front->indexInParent() + 1, std::make_unique<GroupObject>());
    for (NodeId id : ids)
        tx.move(id, group, -1);
    tx.commit();
    setSelection({group});
}

void Editor::ungroupSelection()
{
    QList<NodeId> released;
    Transaction tx(*m_doc, tr("Gruppierung aufheben"));
    for (NodeId id : topLevelSelection()) {
        const auto* group = m_doc->findAs<GroupObject>(id);
        if (!group) {
            released.append(id);
            continue;
        }
        const NodeId parentId = group->parent()->id();
        int index = group->indexInParent();
        while (group->childCount() > 0) {
            const NodeId childId = group->child(0)->id();
            tx.move(childId, parentId, index++);
            released.append(childId);
        }
        tx.remove(id);
    }
    tx.commit();
    setSelection(released);
}

void Editor::setSelectionFill(const Fill& fill)
{
    const QList<NodeId> ids = topLevelSelection();
    if (ids.isEmpty()) {
        m_defaultStyle.fill = fill;
        return;
    }
    remember(fill.isGradient() ? tr("Farbverlauf") : tr("Füllung"), [this, fill] { setSelectionFill(fill); });
    Transaction tx(*m_doc, fill.isGradient() ? tr("Farbverlauf") : tr("Füllung"));
    for (NodeId id : ids) {
        tx.modifySubtree<DrawObject>(id, [&fill](DrawObject& o) {
            Style s = o.style();
            s.fill = fill;
            o.setStyle(s);
        });
    }
    tx.commit();
}

void Editor::setObjectEffect(NodeId id, Effect effect, const Ramp& ramp, const QString& label)
{
    const auto* o = m_doc->findAs<DrawObject>(id);
    if (!o || isLocked(id) || o->effect(effect) == ramp)
        return;
    Transaction tx(*m_doc, label);
    tx.modify<DrawObject>(id, [effect, &ramp](DrawObject& d) { d.setEffect(effect, ramp); });
    tx.commit();
}

void Editor::setSelectionEffect(Effect effect, const std::function<void(Ramp&)>& fn, const QString& label)
{
    Transaction tx(*m_doc, label);
    for (NodeId id : topLevelSelection()) {
        const auto* o = m_doc->findAs<DrawObject>(id);
        Ramp r = o->effect(effect);
        fn(r);
        if (r != o->effect(effect))
            tx.modify<DrawObject>(id, [effect, r](DrawObject& d) { d.setEffect(effect, r); });
    }
    if (!tx.isEmpty())
        tx.commit();
}

void Editor::setSelectionBlur(double mm)
{
    const QList<NodeId> ids = topLevelSelection();
    if (ids.isEmpty())
        return;
    Transaction tx(*m_doc, tr("Unschärfe"));
    for (NodeId id : ids) {
        if (m_doc->findAs<DrawObject>(id)->blur() != mm)
            tx.modify<DrawObject>(id, [mm](DrawObject& o) { o.setBlur(mm); });
    }
    tx.commit();
}

void Editor::modifyCurve(NodeId id, const std::function<void(CurveObject&)>& fn, const QString& label)
{
    if (!m_doc->findAs<CurveObject>(id) || isLocked(id))
        return;
    Transaction tx(*m_doc, label);
    tx.modify<CurveObject>(id, fn);
    tx.commit();
}

void Editor::modifyObject(NodeId id, const std::function<void(DrawObject&)>& fn, const QString& label)
{
    if (!m_doc->findAs<DrawObject>(id) || isLocked(id))
        return;
    Transaction tx(*m_doc, label);
    tx.modify<DrawObject>(id, fn);
    tx.commit();
}

void Editor::modifyText(NodeId id, const std::function<void(TextObject&)>& fn, const QString& label)
{
    if (!m_doc->findAs<TextObject>(id) || isLocked(id))
        return;
    Transaction tx(*m_doc, label);
    tx.modify<TextObject>(id, fn);
    tx.commit();
}

void Editor::modifyTable(NodeId id, const std::function<void(TableObject&)>& fn, const QString& label)
{
    if (!m_doc->findAs<TableObject>(id) || isLocked(id))
        return;
    Transaction tx(*m_doc, label);
    tx.modify<TableObject>(id, fn);
    tx.commit();
}

void Editor::setImageFrame(NodeId id, const QList<SubPath>& frame, const QString& label)
{
    const auto* image = m_doc->findAs<ImageObject>(id);
    if (!image || isLocked(id) || image->frame() == frame)
        return;
    Transaction tx(*m_doc, label);
    tx.modify<ImageObject>(id, [&frame](ImageObject& o) { o.setFrame(frame); });
    tx.commit();
}

const ImageObject* Editor::selectedImage() const
{
    const QList<const DrawObject*> objects = selectedObjects();
    return objects.size() == 1 ? node_cast<ImageObject>(objects.first()) : nullptr;
}

NodeId Editor::vectorizeImage(NodeId id, const trace::Result& result, bool deleteOriginal)
{
    const auto* image = m_doc->findAs<ImageObject>(id);
    if (!image || result.shapes.isEmpty() || result.size.isEmpty())
        return InvalidNodeId;
    // Pixel -> lokale mm des Bildes; die Transformation des Bildes gilt für alle Kurven
    const QTransform toMm = QTransform::fromScale(image->size().width() / result.size.width(),
                                                  image->size().height() / result.size.height());
    auto makeCurve = [&](const trace::Shape& shape) {
        QList<SubPath> paths = shape.paths;
        for (SubPath& path : paths) {
            for (CurveNode& n : path.nodes) {
                n.point = toMm.map(n.point);
                n.c1 = toMm.map(n.c1);
                n.c2 = toMm.map(n.c2);
            }
        }
        auto curve = std::make_unique<CurveObject>(paths);
        curve->setTransform(image->transform());
        Style s;
        s.fill = Fill::solid(Color::fromQColor(shape.color));
        s.stroke.reset();
        curve->setStyle(s);
        return curve;
    };
    Transaction tx(*m_doc, tr("Bitmap vektorisieren"));
    const NodeId parent = image->parent()->id();
    const int index = image->indexInParent() + 1;
    NodeId traced = InvalidNodeId;
    if (result.shapes.size() == 1) {
        // Eine einzige Kurve ohne Gruppe
        traced = tx.insert(parent, index, makeCurve(result.shapes.first()));
    } else {
        traced = tx.insert(parent, index, std::make_unique<GroupObject>());
        // Farblich gruppieren: je Farbe eine Untergruppe, in der Reihenfolge des ersten Auftretens
        QHash<QRgb, NodeId> colorGroups;
        for (const trace::Shape& shape : result.shapes) {
            NodeId target = traced;
            if (result.groupByColor) {
                auto it = colorGroups.constFind(shape.color.rgb());
                if (it == colorGroups.constEnd())
                    it = colorGroups.insert(shape.color.rgb(), tx.insert(traced, -1, std::make_unique<GroupObject>()));
                target = *it;
            }
            tx.insert(target, -1, makeCurve(shape));
        }
    }
    if (deleteOriginal)
        tx.remove(id);
    tx.commit();
    setSelection({traced});
    return traced;
}

void Editor::setImagePixels(NodeId id, const QImage& pixels, const QString& label)
{
    const auto* image = m_doc->findAs<ImageObject>(id);
    if (!image || isLocked(id) || pixels.isNull())
        return;
    const QString key = imagepool::addImage(pixels);
    if (key.isEmpty() || key == image->key())
        return;
    Transaction tx(*m_doc, label);
    tx.modify<ImageObject>(id, [&key](ImageObject& o) { o.setKey(key); });
    tx.commit();
}

QList<const TextObject*> Editor::selectedTexts() const
{
    QList<const TextObject*> list;
    for (const DrawObject* o : selectedObjects()) {
        const auto* t = node_cast<TextObject>(o);
        if (t && t->isFollower())
            t = m_doc->findAs<TextObject>(t->chainHead());
        if (t && !list.contains(t))
            list << t;
    }
    return list;
}

void Editor::closeTextChains(Transaction& tx, const QList<NodeId>& removed) const
{
    // Alle Textrahmen in den gelöschten Teilbäumen
    QSet<NodeId> gone;
    std::function<void(const Node*)> collect = [&](const Node* n) {
        gone.insert(n->id());
        for (int i = 0; i < n->childCount(); ++i)
            collect(n->child(i));
    };
    for (NodeId id : removed) {
        if (const Node* n = m_doc->find(id))
            collect(n);
    }
    QSet<NodeId> done;
    for (NodeId id : std::as_const(gone)) {
        const auto* t = m_doc->findAs<TextObject>(id);
        if (!t || t->chain().isEmpty() || done.contains(t->chainHead()))
            continue;
        const QList<NodeId> chain = t->chain();
        done.insert(chain.first());
        QList<NodeId> survivors;
        for (NodeId member : chain) {
            if (!gone.contains(member))
                survivors << member;
        }
        if (survivors.isEmpty())
            continue;
        const auto* head = m_doc->findAs<TextObject>(chain.first());
        for (int i = 0; i < survivors.size(); ++i) {
            const NodeId next = i + 1 < survivors.size() ? survivors[i + 1] : 0;
            const bool newHead = i == 0 && survivors[0] != chain.first();
            const auto* member = m_doc->findAs<TextObject>(survivors[i]);
            if (member->nextFrame() == next && !newHead)
                continue;
            tx.modify<TextObject>(survivors[i], [&](TextObject& o) {
                o.setNextFrame(next);
                if (newHead) {
                    o.setParagraphs(head->paragraphs());
                    o.setHyphenate(head->hyphenate());
                }
            });
        }
    }
}

void Editor::detachConnectors(Transaction& tx, const QList<NodeId>& removed) const
{
    QSet<NodeId> gone;
    std::function<void(const Node*)> collect = [&](const Node* n) {
        gone.insert(n->id());
        for (int i = 0; i < n->childCount(); ++i)
            collect(n->child(i));
    };
    for (NodeId id : removed)
        if (const Node* n = m_doc->find(id))
            collect(n);
    for (const DrawObject* o : pickableObjects()) {
        const auto* c = node_cast<ConnectorObject>(o);
        if (!c || gone.contains(c->id()) || !(gone.contains(c->start().target) || gone.contains(c->end().target)))
            continue;
        // Lage der Enden jetzt festhalten
        const QPainterPath path = c->path();
        ConnectorObject::End a = c->start();
        ConnectorObject::End b = c->end();
        if (gone.contains(a.target)) {
            a.point = path.elementCount() ? QPointF(path.elementAt(0)) : a.point;
            a.target = 0;
        }
        if (gone.contains(b.target)) {
            b.point = path.elementCount() ? QPointF(path.elementAt(path.elementCount() - 1)) : b.point;
            b.target = 0;
        }
        tx.modify<ConnectorObject>(c->id(), [a, b](ConnectorObject& x) { x.setEnds(a, b); });
    }
}

bool Editor::canBreakConnectors() const
{
    for (const DrawObject* o : selectedObjects())
        if (const auto* c = node_cast<ConnectorObject>(o); c && c->isAttached())
            return true;
    return false;
}

void Editor::breakConnectors()
{
    Transaction tx(*m_doc, tr("Verbindung trennen"));
    for (const DrawObject* o : selectedObjects()) {
        const auto* c = node_cast<ConnectorObject>(o);
        if (!c || !c->isAttached() || isLocked(c->id()))
            continue;
        const QPainterPath path = c->path();
        ConnectorObject::End a = c->start();
        ConnectorObject::End b = c->end();
        a.point = QPointF(path.elementAt(0));
        b.point = QPointF(path.elementAt(path.elementCount() - 1));
        a.target = 0;
        b.target = 0;
        tx.modify<ConnectorObject>(c->id(), [a, b](ConnectorObject& x) { x.setEnds(a, b); });
    }
    if (!tx.isEmpty())
        tx.commit();
}

bool Editor::canLinkTextFrame(NodeId from, NodeId to) const
{
    const auto* a = m_doc->findAs<TextObject>(from);
    const auto* b = m_doc->findAs<TextObject>(to);
    if (!a || !b || a == b || a->kind() != TextObject::Kind::Paragraph || b->kind() != TextObject::Kind::Paragraph)
        return false;
    if (isLocked(from) || isLocked(to) || b->isFollower() || a->chain().contains(to))
        return false;
    return a->nextFrame() == 0;
}

bool Editor::linkTextFrame(NodeId from, NodeId to)
{
    if (!canLinkTextFrame(from, to))
        return false;
    const auto* a = m_doc->findAs<TextObject>(from);
    const auto* b = m_doc->findAs<TextObject>(to);
    const NodeId headId = a->chainHead();
    const auto* head = m_doc->findAs<TextObject>(headId);
    Transaction tx(*m_doc, tr("Text verknüpfen"));
    if (!b->isEmpty()) {
        // Eigener Text des Zielrahmens kommt ans Ende des Kettentextes
        QList<TextParagraph> story = head->isEmpty() ? QList<TextParagraph>() : head->paragraphs();
        story << b->paragraphs();
        tx.modify<TextObject>(headId, [&story](TextObject& o) { o.setParagraphs(story); });
        tx.modify<TextObject>(to, [](TextObject& o) { o.setParagraphs({}); });
    }
    tx.modify<TextObject>(from, [to](TextObject& o) { o.setNextFrame(to); });
    tx.commit();
    return true;
}

NodeId Editor::linkNewTextFrame(NodeId from, const QRectF& rect)
{
    const auto* a = m_doc->findAs<TextObject>(from);
    const NodeId layer = activeLayer();
    if (!a || a->kind() != TextObject::Kind::Paragraph || a->nextFrame() != 0 || isLocked(from) || layer == InvalidNodeId
        || rect.isEmpty())
        return InvalidNodeId;
    auto frame = std::make_unique<TextObject>(TextObject::Kind::Paragraph, rect.size());
    frame->setTransform(QTransform::fromTranslate(rect.left(), rect.top()));
    frame->setStyle(a->style());
    Transaction tx(*m_doc, tr("Text verknüpfen"));
    const NodeId id = tx.insert(layer, -1, std::move(frame));
    tx.modify<TextObject>(from, [id](TextObject& o) { o.setNextFrame(id); });
    tx.commit();
    setSelection({id});
    return id;
}

bool Editor::canLinkSelectedTextFrames() const
{
    QList<NodeId> frames;
    for (const DrawObject* o : selectedObjects()) {
        if (const auto* t = node_cast<TextObject>(o); t && t->kind() == TextObject::Kind::Paragraph)
            frames << t->id();
    }
    for (int i = 0; i + 1 < frames.size(); ++i) {
        if (canLinkTextFrame(frames[i], frames[i + 1]))
            return true;
    }
    return false;
}

void Editor::linkSelectedTextFrames()
{
    QList<NodeId> frames;
    for (const DrawObject* o : selectedObjects()) {
        if (const auto* t = node_cast<TextObject>(o); t && t->kind() == TextObject::Kind::Paragraph)
            frames << t->id();
    }
    // Jedes Paar einzeln; ans Ende der bisherigen Kette des Vorgängers, damit die Reihenfolge stimmt
    for (int i = 0; i + 1 < frames.size(); ++i) {
        const auto* a = m_doc->findAs<TextObject>(frames[i]);
        const NodeId last = a && !a->chain().isEmpty() ? a->chain().last() : frames[i];
        linkTextFrame(last, frames[i + 1]);
    }
}

bool Editor::canUnlinkSelectedTextFrames() const
{
    for (const DrawObject* o : selectedObjects()) {
        if (const auto* t = node_cast<TextObject>(o); t && !t->chain().isEmpty())
            return true;
    }
    return false;
}

void Editor::unlinkSelectedTextFrames()
{
    QList<NodeId> heads;
    for (const DrawObject* o : selectedObjects()) {
        if (const auto* t = node_cast<TextObject>(o); t && !t->chain().isEmpty() && !heads.contains(t->chainHead()))
            heads << t->chainHead();
    }
    if (heads.isEmpty())
        return;
    Transaction tx(*m_doc, tr("Verknüpfung aufheben"));
    for (NodeId headId : heads) {
        const auto* head = m_doc->findAs<TextObject>(headId);
        const QList<NodeId> chain = head->chain();
        const QList<QList<TextParagraph>> parts = head->paragraphsByFrame();
        for (int i = 0; i < chain.size() && i < parts.size(); ++i) {
            const QList<TextParagraph> part = parts[i];
            const bool hyphenate = head->hyphenate();
            tx.modify<TextObject>(chain[i], [&part, hyphenate](TextObject& o) {
                o.setNextFrame(0);
                o.setParagraphs(part);
                o.setHyphenate(hyphenate);
            });
        }
    }
    tx.commit();
}

void Editor::applyTextFormat(const std::function<void(CharFormat&)>& fn, const QString& label)
{
    Transaction tx(*m_doc, label);
    for (const TextObject* t : selectedTexts()) {
        if (!isLocked(t->id()))
            tx.modify<TextObject>(t->id(), [&fn](TextObject& o) { o.applyToAll(fn); });
    }
    if (!tx.isEmpty())
        tx.commit();
}

void Editor::setTextAlignment(TextParagraph::Align align)
{
    Transaction tx(*m_doc, tr("Ausrichtung"));
    for (const TextObject* t : selectedTexts()) {
        if (!isLocked(t->id()))
            tx.modify<TextObject>(t->id(), [align](TextObject& o) { o.setAlignment(align); });
    }
    if (!tx.isEmpty())
        tx.commit();
}

void Editor::setTextHyphenation(bool on)
{
    Transaction tx(*m_doc, tr("Silbentrennung"));
    for (const TextObject* t : selectedTexts()) {
        if (!isLocked(t->id()) && t->kind() == TextObject::Kind::Paragraph && t->hyphenate() != on)
            tx.modify<TextObject>(t->id(), [on](TextObject& o) { o.setHyphenate(on); });
    }
    if (!tx.isEmpty())
        tx.commit();
}

bool Editor::canConvertSelectionToCurves() const
{
    for (const DrawObject* o : selectedObjects()) {
        const auto* curve = node_cast<CurveObject>(o);
        if ((!curve || curve->media().isActive()) && !node_cast<GroupObject>(o))
            return true;
    }
    return false;
}

void Editor::convertSelectionToCurves()
{
    const QList<NodeId> ids = topLevelSelection();
    Transaction tx(*m_doc, tr("In Kurven konvertieren"));
    QList<NodeId> result;
    for (NodeId id : ids) {
        const DrawObject* o = m_doc->findAs<DrawObject>(id);
        const auto* existing = node_cast<CurveObject>(o);
        if (!o || (existing && !existing->media().isActive()) || node_cast<GroupObject>(o)) {
            result.append(id);
            continue;
        }
        const NodeId parent = o->parent()->id();
        const int index = o->indexInParent();
        if (o->blurRamp().isActive() && !node_cast<ImageObject>(o)) {
            // Unschärfe wird dauerhaft: Gruppe aus Stufenkonturen (bake::blurToContours)
            QList<bake::Part> parts;
            if (const auto* text = node_cast<TextObject>(o)) {
                for (const TextObject::Piece& piece : text->shownPieces()) {
                    const Fill fill = piece.color ? Fill::solid(*piece.color) : text->style().fill;
                    if (!fill.isNone())
                        parts << bake::Part{text->transform().map(piece.path), fill};
                }
            } else {
                const Style& s = o->style();
                if (!s.fill.isNone())
                    parts << bake::Part{o->path(), s.fill};
                if (s.stroke) {
                    QPainterPathStroker stroker;
                    stroker.setWidth(std::max(0.01, s.strokeWidth));
                    stroker.setJoinStyle(Qt::MiterJoin);
                    parts << bake::Part{stroker.createStroke(o->path()), Fill::solid(*s.stroke)};
                }
            }
            QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
            std::unique_ptr<GroupObject> group =
                bake::blurToContours(parts, o->blurRamp(), o->transform(), o->localPath().boundingRect(),
                                     m_shapeDefaults.blurSteps, m_shapeDefaults.blurBands);
            QGuiApplication::restoreOverrideCursor();
            if (group) {
                group->setEffect(Effect::Transparency, o->transparency());
                group->setName(o->name());
            }
            tx.remove(id); // erst jetzt: danach gibt es o nicht mehr
            if (group)
                result.append(tx.insert(parent, index, std::move(group)));
            continue;
        }
        if (const auto* text = node_cast<TextObject>(o)) {
            // Text: je Farbe eine Kurve; mehrere Farben ergeben eine Gruppe
            // Alles Nötige vor dem Entfernen kopieren: danach gibt es das Textobjekt nicht mehr
            const QTransform textTransform = text->transform();
            const Style textStyle = text->style();
            const Ramp textBlur = text->blurRamp();
            const Ramp textAlpha = text->transparency();
            const bool shaped = text->hasEnvelope();
            auto curveFor = [&](const TextObject::Piece& piece) {
                std::unique_ptr<CurveObject> c = shaped ? std::make_unique<CurveObject>(pathops::fitPath(piece.path))
                                                        : CurveObject::fromPath(piece.path);
                c->setTransform(textTransform);
                Style s = textStyle;
                if (piece.color)
                    s.fill = Fill::solid(*piece.color);
                c->setStyle(s);
                c->setEffect(Effect::Blur, textBlur);
                c->setEffect(Effect::Transparency, textAlpha);
                return c;
            };
            const QList<TextObject::Piece> pieces = text->shownPieces();
            tx.remove(id);
            if (pieces.size() == 1) {
                result.append(tx.insert(parent, index, curveFor(pieces.first())));
            } else if (pieces.size() > 1) {
                const NodeId group = tx.insert(parent, index, std::make_unique<GroupObject>());
                for (const TextObject::Piece& piece : pieces)
                    tx.insert(group, -1, curveFor(piece));
                result.append(group);
            }
            continue;
        }
        if (existing && existing->media().kind == ArtisticMedia::Kind::Sprayer) {
            // Sprühdose: jedes versprühte Objekt eine Kurve in seiner Farbe, zusammen eine Gruppe
            auto group = std::make_unique<GroupObject>();
            group->setName(o->name());
            for (const SubPath& sp : existing->subPaths())
                for (const pathops::SprayPiece& piece : pathops::sprayPieces(CurveObject(QList<SubPath>{sp}).spinePath(), existing->media())) {
                    std::unique_ptr<CurveObject> c = CurveObject::fromPath(piece.path);
                    c->setTransform(o->transform());
                    Style s;
                    s.fill = Fill::solid(piece.color);
                    s.stroke.reset();
                    c->setStyle(s);
                    attachDetachedChild(*group, std::move(c));
                }
            tx.remove(id);
            result.append(tx.insert(parent, index, std::move(group)));
            continue;
        }
        // Gleiche Grundform, Lage, Füllung, Kontur und Unschärfe; nur der Typ wechselt.
        // Künstlerische Medien werden aufgebrochen: der Umriss des Strichs wird zur Kurve.
        // Eine Hülle wird dabei dauerhaft: die Kurve bekommt die verformte Form
        std::unique_ptr<CurveObject> curve;
        if (o->hasEnvelope()) {
            const QPainterPath shaped = o->enveloped(o->localPath());
            curve = std::make_unique<CurveObject>(pathops::fitPath(shaped));
        } else {
            curve = existing ? std::make_unique<CurveObject>(pathops::fitRegion(o->localPath(), 0.02))
                             : CurveObject::fromPath(o->localPath());
        }
        curve->setTransform(o->transform());
        curve->setStyle(o->style());
        curve->copyEffects(*o);
        curve->setEnvelopes({});
        curve->setName(o->name());
        tx.remove(id);
        result.append(tx.insert(parent, index, std::move(curve)));
    }
    if (tx.isEmpty())
        return;
    tx.commit();
    setSelection(result);
}

void Editor::setObjectFill(NodeId id, const Fill& fill, const QString& label)
{
    const DrawObject* o = m_doc->findAs<DrawObject>(id);
    if (!o || o->style().fill == fill)
        return;
    Transaction tx(*m_doc, label);
    tx.modifySubtree<DrawObject>(id, [&fill](DrawObject& obj) {
        Style s = obj.style();
        s.fill = fill;
        obj.setStyle(s);
    });
    tx.commit();
}

QSizeF Editor::selectionLocalSize() const
{
    const QList<const DrawObject*> objects = selectedObjects();
    return objects.isEmpty() ? QSizeF(1, 1) : objects.first()->localPath().boundingRect().size();
}

void Editor::setSelectionStroke(std::optional<Color> color)
{
    const QList<NodeId> ids = topLevelSelection();
    if (ids.isEmpty()) {
        m_defaultStyle.stroke = color;
        return;
    }
    remember(tr("Kontur"), [this, color] { setSelectionStroke(color); });
    Transaction tx(*m_doc, tr("Kontur"));
    for (NodeId id : ids) {
        tx.modifySubtree<DrawObject>(id, [&color](DrawObject& o) {
            Style s = o.style();
            s.stroke = color;
            o.setStyle(s);
        });
    }
    tx.commit();
}

void Editor::moveSelectionToMaster()
{
    const NodeId layer = topEditableLayer(m_doc->masterPage());
    const QList<NodeId> ids = topLevelSelection();
    if (layer == InvalidNodeId || ids.isEmpty())
        return;
    Transaction tx(*m_doc, tr("Auf Masterseite verschieben"));
    for (NodeId id : ids)
        tx.move(id, layer, -1);
    tx.commit();
}

void Editor::moveSelectionToCurrentPage()
{
    const NodeId layer = activeLayer();
    const QList<NodeId> ids = topLevelSelection();
    if (layer == InvalidNodeId || ids.isEmpty())
        return;
    Transaction tx(*m_doc, tr("Auf Seite verschieben"));
    for (NodeId id : ids)
        tx.move(id, layer, -1);
    tx.commit();
}

void Editor::insertPage(int index)
{
    index = std::clamp(index, 0, m_doc->pageCount());
    Transaction tx(*m_doc, tr("Seite einfügen"));
    // +1: Kind 0 der Wurzel ist die Masterseite
    tx.insert(m_doc->root().id(), index + 1, createPage(currentPage().size(), tr("Ebene 1")));
    tx.commit();
    setCurrentPageIndex(index);
}

void Editor::deletePage(int index)
{
    if (m_doc->pageCount() <= 1 || index < 0 || index >= m_doc->pageCount())
        return;
    clearSelection();
    Transaction tx(*m_doc, tr("Seite löschen"));
    tx.remove(m_doc->page(index).id());
    tx.commit();
    setCurrentPageIndex(std::min(index, m_doc->pageCount() - 1));
}

void Editor::setPageSize(const QSizeF& size, bool allPages)
{
    Transaction tx(*m_doc, tr("Seitengrösse"));
    auto resize = [&size](Page& p) { p.setSize(size); };
    if (allPages) {
        tx.modify<Page>(m_doc->masterPage().id(), resize);
        for (int i = 0; i < m_doc->pageCount(); ++i)
            tx.modify<Page>(m_doc->page(i).id(), resize);
    } else {
        tx.modify<Page>(currentPage().id(), resize);
    }
    tx.commit();
}

} // namespace vek

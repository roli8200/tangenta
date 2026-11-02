#include "Document.h"

#include "CloneObject.h"

#include "Serialization.h"
#include "TextObject.h"
#include "ConnectorObject.h"

namespace vek {

Document::Document(const QSizeF& pageSize, QObject* parent)
    : Document(std::make_unique<DocumentRoot>(), parent)
{
    insertNode(m_root->id(), -1, createPage(pageSize, tr("Master-Ebene 1"), true));
    insertNode(m_root->id(), -1, createPage(pageSize, tr("Ebene 1")));
}

Document::Document(std::unique_ptr<DocumentRoot> root, QObject* parent)
    : QObject(parent), m_root(std::move(root))
{
    indexSubtree(*m_root);
    updateTextFlow();
}

std::unique_ptr<Document> Document::fromTree(std::unique_ptr<Node> root, QString* error)
{
    auto fail = [error](const QString& msg) {
        if (error)
            *error = msg;
        return std::unique_ptr<Document>();
    };
    auto* typedRoot = node_cast<DocumentRoot>(root.get());
    if (!typedRoot)
        return fail(tr("Die Wurzel ist kein Dokument."));
    if (root->childCount() < 2 || !node_cast<Page>(root->child(0))->master())
        return fail(tr("Masterseite oder Seiten fehlen."));
    for (int i = 1; i < root->childCount(); ++i) {
        if (node_cast<Page>(root->child(i))->master())
            return fail(tr("Mehr als eine Masterseite."));
    }
    root.release();
    return std::unique_ptr<Document>(new Document(std::unique_ptr<DocumentRoot>(typedRoot), nullptr));
}

Document::~Document() = default;

const Page& Document::masterPage() const
{
    return *static_cast<const Page*>(m_root->child(0));
}

int Document::pageCount() const
{
    return m_root->childCount() - 1;
}

const Page& Document::page(int index) const
{
    return *static_cast<const Page*>(m_root->child(index + 1));
}

int Document::pageIndex(NodeId pageId) const
{
    const Node* n = find(pageId);
    if (!n || n->parent() != m_root.get() || n == m_root->child(0))
        return -1;
    return n->indexInParent() - 1;
}

const Node* Document::find(NodeId id) const
{
    const auto it = m_nodes.find(id);
    return it == m_nodes.end() ? nullptr : it->second;
}

Node* Document::findMutable(NodeId id)
{
    const auto it = m_nodes.find(id);
    return it == m_nodes.end() ? nullptr : it->second;
}

const Page* Document::pageOf(NodeId id) const
{
    for (const Node* n = find(id); n; n = n->parent()) {
        if (auto* p = node_cast<Page>(n))
            return p;
    }
    return nullptr;
}

void Document::indexSubtree(Node& node)
{
    // Ids bleiben über Undo/Redo und Speichern stabil; nur neue oder kollidierende Knoten
    // bekommen eine frische Id.
    if (node.m_id == InvalidNodeId || m_nodes.count(node.m_id))
        node.m_id = m_nextId++;
    else if (node.m_id >= m_nextId)
        m_nextId = node.m_id + 1;
    m_nodes[node.m_id] = &node;
    for (auto& c : node.m_children)
        indexSubtree(*c);
}

void Document::unindexSubtree(const Node& node)
{
    m_nodes.erase(node.m_id);
    for (const auto& c : node.m_children)
        unindexSubtree(*c);
}

NodeId Document::insertNode(NodeId parentId, int index, std::unique_ptr<Node> node)
{
    Node* parent = findMutable(parentId);
    Q_ASSERT(parent && node && !node->m_parent);
    Q_ASSERT(parent->canContain(*node));
    auto& siblings = parent->m_children;
    if (index < 0 || index > static_cast<int>(siblings.size()))
        index = static_cast<int>(siblings.size());
    node->m_parent = parent;
    Node* raw = node.get();
    siblings.insert(siblings.begin() + index, std::move(node));
    indexSubtree(*raw);
    return raw->m_id;
}

std::unique_ptr<Node> Document::detachNode(NodeId id)
{
    Node* node = findMutable(id);
    Q_ASSERT(node && node->m_parent);
    auto& siblings = node->m_parent->m_children;
    auto it = siblings.begin() + node->indexInParent();
    std::unique_ptr<Node> owned = std::move(*it);
    siblings.erase(it);
    owned->m_parent = nullptr;
    unindexSubtree(*owned);
    return owned;
}

void Document::apply(const Change& change, bool forward)
{
    switch (change.kind) {
    case Change::Kind::Insert:
    case Change::Kind::Remove: {
        const bool insert = (change.kind == Change::Kind::Insert) == forward;
        if (insert) {
            const QJsonObject& tree = change.kind == Change::Kind::Insert ? change.after : change.before;
            std::unique_ptr<Node> node = serial::fromJson(tree, serial::Ids::Keep);
            Q_ASSERT(node);
            insertNode(change.parentId, change.index, std::move(node));
        } else {
            detachNode(change.id);
        }
        break;
    }
    case Change::Kind::Modify: {
        const QJsonObject& states = forward ? change.after : change.before;
        for (auto it = states.begin(); it != states.end(); ++it) {
            Node* n = findMutable(it.key().toULongLong());
            Q_ASSERT(n);
            n->readProperties(it.value().toObject());
        }
        break;
    }
    }
}

void Document::commitChanges(ChangeSet set)
{
    const bool wasModified = isModified();
    m_history.push(std::move(set));
    emitChanged(wasModified);
}

bool Document::mergeUndo(std::size_t mark, const QString& label)
{
    if (m_inTransaction)
        return false;
    const bool wasModified = isModified();
    if (!m_history.merge(mark, label))
        return false;
    emitChanged(wasModified); // Beschriftung von Rückgängig ändert sich
    return true;
}

void Document::undo()
{
    if (!canUndo() || m_inTransaction)
        return;
    const bool wasModified = isModified();
    const ChangeSet& set = m_history.nextUndo();
    for (auto it = set.changes.rbegin(); it != set.changes.rend(); ++it)
        apply(*it, false);
    m_history.stepBack();
    emitChanged(wasModified);
}

void Document::redo()
{
    if (!canRedo() || m_inTransaction)
        return;
    const bool wasModified = isModified();
    for (const Change& c : m_history.nextRedo().changes)
        apply(c, true);
    m_history.stepForward();
    emitChanged(wasModified);
}

void Document::setClean()
{
    const bool wasModified = isModified();
    m_history.setClean();
    if (wasModified)
        emit modifiedChanged(false);
}

void Document::updateTextFlow()
{
    std::vector<const TextObject*> texts;
    std::vector<const ConnectorObject*> connectors;
    std::vector<const CloneObject*> clones;
    for (const auto& [id, node] : m_nodes) {
        if (const auto* t = node_cast<TextObject>(node))
            texts.push_back(t);
        else if (const auto* c = node_cast<ConnectorObject>(node))
            connectors.push_back(c);
        else if (const auto* k = node_cast<CloneObject>(node))
            clones.push_back(k);
    }
    // Klone und Symbolinstanzen folgen ihrem Original (Klon eines Klons: mehrere Durchgänge)
    for (int pass = 0; pass < 4; ++pass) {
        bool changed = false;
        for (const CloneObject* k : clones)
            changed = k->resolve([this](NodeId id) { return findAs<DrawObject>(id); }) || changed;
        if (!changed)
            break;
    }
    // Text an Pfad: Umriss des Pfadobjekts übernehmen
    for (const TextObject* t : texts)
        if (t->onPath())
            t->resolvePath([this](NodeId id) { return findAs<DrawObject>(id); });
    TextObject::linkChains(std::move(texts));
    // Verbindungslinien folgen ihren Objekten
    for (const ConnectorObject* c : connectors)
        c->resolve([this](NodeId id) { return findAs<DrawObject>(id); });
}

void Document::emitChanged(bool wasModified)
{
    updateTextFlow();
    emit changed();
    if (wasModified != isModified())
        emit modifiedChanged(isModified());
}

} // namespace vek

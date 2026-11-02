#include "Transaction.h"

#include "Serialization.h"

namespace vek {

namespace {

void collectProperties(const Node& node, bool subtree, QJsonObject& out)
{
    out[QString::number(node.id())] = serial::properties(node);
    if (subtree) {
        for (int i = 0; i < node.childCount(); ++i)
            collectProperties(*node.child(i), true, out);
    }
}

} // namespace

Transaction::Transaction(Document& doc, const QString& label)
    : m_doc(doc)
{
    Q_ASSERT(!doc.m_inTransaction); // keine verschachtelten Transaktionen
    m_doc.m_inTransaction = true;
    m_set.label = label;
}

Transaction::~Transaction()
{
    if (m_open)
        rollback();
}

NodeId Transaction::insert(NodeId parentId, int index, std::unique_ptr<Node> node)
{
    Q_ASSERT(m_open);
    const Node* parent = m_doc.find(parentId);
    if (!parent || !node || !parent->canContain(*node))
        return InvalidNodeId;
    const NodeId id = m_doc.insertNode(parentId, index, std::move(node));
    const Node* inserted = m_doc.find(id);

    Change c;
    c.kind = Change::Kind::Insert;
    c.id = id;
    c.parentId = parentId;
    c.index = inserted->indexInParent();
    c.after = serial::toJson(*inserted);
    m_set.changes.push_back(std::move(c));
    return id;
}

void Transaction::remove(NodeId id)
{
    Q_ASSERT(m_open);
    const Node* node = m_doc.find(id);
    if (!node || !node->parent())
        return;
    Change c;
    c.kind = Change::Kind::Remove;
    c.id = id;
    c.parentId = node->parent()->id();
    c.index = node->indexInParent();
    c.before = serial::toJson(*node);
    m_doc.detachNode(id);
    m_set.changes.push_back(std::move(c));
}

void Transaction::move(NodeId id, NodeId newParentId, int index)
{
    Q_ASSERT(m_open);
    const Node* node = m_doc.find(id);
    const Node* newParent = m_doc.find(newParentId);
    if (!node || !node->parent() || !newParent || !newParent->canContain(*node) || node == newParent
        || node->isAncestorOf(newParent))
        return;

    Change out;
    out.kind = Change::Kind::Remove;
    out.id = id;
    out.parentId = node->parent()->id();
    out.index = node->indexInParent();
    out.before = serial::toJson(*node);
    std::unique_ptr<Node> owned = m_doc.detachNode(id);
    m_set.changes.push_back(std::move(out));

    const NodeId sameId = m_doc.insertNode(newParentId, index, std::move(owned));
    Q_ASSERT(sameId == id);
    const Node* moved = m_doc.find(sameId);
    Change in;
    in.kind = Change::Kind::Insert;
    in.id = sameId;
    in.parentId = newParentId;
    in.index = moved->indexInParent();
    in.after = serial::toJson(*moved);
    m_set.changes.push_back(std::move(in));
}

void Transaction::modifyImpl(NodeId id, bool subtree, const std::function<void(Node&)>& fn)
{
    Q_ASSERT(m_open);
    Node* node = m_doc.findMutable(id);
    if (!node)
        return;
    Change c;
    c.kind = Change::Kind::Modify;
    c.id = id;
    collectProperties(*node, subtree, c.before);
    fn(*node);
    collectProperties(*node, subtree, c.after);
    if (c.before != c.after)
        m_set.changes.push_back(std::move(c));
}

void Transaction::commit()
{
    Q_ASSERT(m_open);
    m_open = false;
    m_doc.m_inTransaction = false;
    if (!m_set.changes.empty())
        m_doc.commitChanges(std::move(m_set));
}

void Transaction::rollback()
{
    Q_ASSERT(m_open);
    for (auto it = m_set.changes.rbegin(); it != m_set.changes.rend(); ++it)
        m_doc.apply(*it, false);
    m_set.changes.clear();
    m_open = false;
    m_doc.m_inTransaction = false;
}

} // namespace vek

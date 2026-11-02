#include "Node.h"

namespace vek {

Node::~Node() = default;

void attachDetachedChild(Node& parent, std::unique_ptr<Node> child)
{
    Q_ASSERT(!child->m_parent);
    child->m_parent = &parent;
    parent.m_children.push_back(std::move(child));
}

std::unique_ptr<Node> replaceDetachedChild(Node& parent, int index, std::unique_ptr<Node> child)
{
    Q_ASSERT(!child->m_parent);
    auto& slot = parent.m_children.at(static_cast<std::size_t>(index));
    std::unique_ptr<Node> old = std::move(slot);
    old->m_parent = nullptr;
    child->m_parent = &parent;
    slot = std::move(child);
    return old;
}

void setDetachedId(Node& node, NodeId id)
{
    Q_ASSERT(!node.m_parent);
    node.m_id = id;
}

int Node::indexInParent() const
{
    if (!m_parent)
        return -1;
    const auto& siblings = m_parent->m_children;
    for (std::size_t i = 0; i < siblings.size(); ++i) {
        if (siblings[i].get() == this)
            return static_cast<int>(i);
    }
    return -1;
}

bool Node::isAncestorOf(const Node* other) const
{
    for (const Node* n = other ? other->m_parent : nullptr; n; n = n->m_parent) {
        if (n == this)
            return true;
    }
    return false;
}

void Node::writeProperties(QJsonObject& out) const
{
    if (!m_name.isEmpty())
        out[QStringLiteral("name")] = m_name;
}

void Node::readProperties(const QJsonObject& in)
{
    m_name = in.value(QStringLiteral("name")).toString();
}

} // namespace vek

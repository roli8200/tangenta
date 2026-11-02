#pragma once

#include <QJsonObject>
#include <QString>

#include <cstdint>
#include <memory>
#include <vector>

namespace vek {

using NodeId = std::uint64_t;
constexpr NodeId InvalidNodeId = 0;

class Document;

// Basis aller Knoten im Dokumentbaum (Wurzel, Seiten, Ebenen, Zeichenobjekte).
//
// Regeln, die das Undo strukturell korrekt halten:
//  - Die Baumstruktur (Eltern, Kinder) ändert ausschliesslich das Dokument, und zwar nur
//    über eine Transaction.
//  - Eigenschaften ändert man nur innerhalb von Transaction::modify(). Ausserhalb sieht man
//    Knoten nur als const.
//  - writeProperties()/readProperties() erfassen den *vollständigen* eigenen Zustand ohne
//    Kinder. Was dort fehlt, geht beim Undo und beim Speichern gleichermassen verloren.
class Node {
public:
    virtual ~Node();
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    NodeId id() const { return m_id; }
    Node* parent() const { return m_parent; }
    int childCount() const { return static_cast<int>(m_children.size()); }
    Node* child(int index) const { return m_children.at(static_cast<std::size_t>(index)).get(); }
    int indexInParent() const;
    bool isAncestorOf(const Node* other) const;

    // Stabiler Typname, landet so im Dateiformat
    virtual QString typeName() const = 0;
    // Darf child unter diesem Knoten hängen?
    virtual bool canContain(const Node& child) const = 0;

    virtual void writeProperties(QJsonObject& out) const;
    virtual void readProperties(const QJsonObject& in);

    QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

protected:
    Node() = default;

private:
    friend class Document;
    friend void attachDetachedChild(Node& parent, std::unique_ptr<Node> child);
    friend std::unique_ptr<Node> replaceDetachedChild(Node& parent, int index, std::unique_ptr<Node> child);
    friend void setDetachedId(Node& node, NodeId id);

    NodeId m_id = InvalidNodeId;
    Node* m_parent = nullptr;
    std::vector<std::unique_ptr<Node>> m_children;
    QString m_name;
};

// Hängt child an einen Teilbaum, der noch in keinem Dokument liegt (Aufbau vor dem Einfügen).
void attachDetachedChild(Node& parent, std::unique_ptr<Node> child);
// Ersetzt ein Kind in einem solchen Teilbaum (Aufbau vor dem Einfügen); gibt das alte zurück.
std::unique_ptr<Node> replaceDetachedChild(Node& parent, int index, std::unique_ptr<Node> child);
// Setzt die Id eines Knotens, der noch in keinem Dokument liegt (Laden, Undo).
void setDetachedId(Node& node, NodeId id);

// Bequemer Downcast für const- und nicht-const-Zeiger
template <class T>
T* node_cast(Node* n) { return dynamic_cast<T*>(n); }
template <class T>
const T* node_cast(const Node* n) { return dynamic_cast<const T*>(n); }

} // namespace vek

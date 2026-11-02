#pragma once

#include "Document.h"

#include <functional>

namespace vek {

// Bündelt Änderungen am Dokument zu einem Undo-Schritt.
//
//   Transaction tx(doc, tr("Drehen"));
//   tx.modifySubtree<DrawObject>(id, [&](DrawObject& o) { o.rotate(30, center); });
//   tx.commit();
//
// Jede Operation hält Vorher- und Nachher-Zustand sofort als Daten fest. Undo ist damit
// für jede Operation gleich korrekt, ohne dass ein Werkzeug sein eigenes Inverses schreibt.
// Wird die Transaktion ohne commit() zerstört, nimmt sie alle Änderungen zurück.
class Transaction {
public:
    Transaction(Document& doc, const QString& label);
    ~Transaction();
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    // index -1 = ans Ende. Teilbaum-Knoten ohne Id bekommen neue Ids.
    NodeId insert(NodeId parentId, int index, std::unique_ptr<Node> node);
    void remove(NodeId id);
    // index bezieht sich auf die Geschwister nach dem Herauslösen
    void move(NodeId id, NodeId newParentId, int index);

    // Eigenschaften genau eines Knotens ändern
    template <class T = Node, class F>
    void modify(NodeId id, F&& fn)
    {
        modifyImpl(id, false, [&](Node& n) { fn(dynamic_cast<T&>(n)); });
    }
    // Eigenschaften eines Knotens und aller Nachfahren ändern (z.B. Gruppe transformieren).
    // Die Struktur darf fn dabei nicht verändern.
    template <class T = Node, class F>
    void modifySubtree(NodeId id, F&& fn)
    {
        modifyImpl(id, true, [&](Node& n) { fn(dynamic_cast<T&>(n)); });
    }

    void commit();
    void rollback();
    bool isEmpty() const { return m_set.changes.empty(); }

    Document& document() { return m_doc; }

private:
    void modifyImpl(NodeId id, bool subtree, const std::function<void(Node&)>& fn);

    Document& m_doc;
    ChangeSet m_set;
    bool m_open = true;
};

} // namespace vek

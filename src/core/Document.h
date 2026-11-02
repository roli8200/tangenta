#pragma once

#include "History.h"
#include "Page.h"

#include <QObject>

#include <memory>
#include <unordered_map>

namespace vek {

class Transaction;

// Das Dokument besitzt den Knotenbaum
//
//   document
//   ├── masterPage      globaler Objektbaum, auf allen Seiten sichtbar
//   │   └── layer …
//   ├── page            Seite 1
//   │   └── layer …
//   └── page …
//
// Von aussen ist der Baum nur lesbar. Jede Änderung läuft über eine Transaction und
// landet als ChangeSet im Verlauf; Undo/Redo spielt diese Daten zurück bzw. erneut ab.
class Document : public QObject {
    Q_OBJECT

public:
    // Neues Dokument: Masterseite und eine Seite, je mit einer Ebene
    explicit Document(const QSizeF& pageSize = Page::A4, QObject* parent = nullptr);
    // Aus geladenem Baum; liefert nullptr, wenn die Wurzel nicht der Strukturregel entspricht
    static std::unique_ptr<Document> fromTree(std::unique_ptr<Node> root, QString* error = nullptr);
    ~Document() override;

    const DocumentRoot& root() const { return *m_root; }
    const Page& masterPage() const;
    int pageCount() const;
    const Page& page(int index) const;
    int pageIndex(NodeId pageId) const;

    const Node* find(NodeId id) const;
    template <class T>
    const T* findAs(NodeId id) const { return node_cast<T>(find(id)); }
    // Seite, auf der ein Knoten liegt (auch die Masterseite)
    const Page* pageOf(NodeId id) const;

    bool canUndo() const { return m_history.canUndo(); }
    int undoCount() const { return int(m_history.undoCount()); }
    bool canRedo() const { return m_history.canRedo(); }
    QString undoText() const { return canUndo() ? m_history.nextUndo().label : QString(); }
    QString redoText() const { return canRedo() ? m_history.nextRedo().label : QString(); }
    void undo();
    // Rückgängig-Tiefe wie in den Optionen (Anzahl Schritte), 0 = unbegrenzt
    void setUndoLimit(int steps) { m_history.setLimit(steps > 0 ? std::size_t(steps) : 0); }
    void redo();

    // Marke für mergeUndo(): alles danach wird ein einziger Rückgängig-Schritt
    std::size_t undoMark() const { return m_history.pushed(); }
    bool mergeUndo(std::size_t mark, const QString& label);

    bool isModified() const { return !m_history.isClean(); }
    void setClean();

signals:
    // Nach jeder abgeschlossenen Transaktion, nach Undo und Redo
    void changed();
    void modifiedChanged(bool modified);

private:
    friend class Transaction;

    Document(std::unique_ptr<DocumentRoot> root, QObject* parent);

    Node* findMutable(NodeId id);
    NodeId insertNode(NodeId parentId, int index, std::unique_ptr<Node> node);
    std::unique_ptr<Node> detachNode(NodeId id);
    void indexSubtree(Node& node);
    void unindexSubtree(const Node& node);
    void apply(const Change& change, bool forward);
    void commitChanges(ChangeSet set);
    void emitChanged(bool wasModified);
    // Textketten neu verknüpfen und den Text durch die Rahmen fliessen lassen
    void updateTextFlow();

    std::unique_ptr<DocumentRoot> m_root;
    std::unordered_map<NodeId, Node*> m_nodes;
    NodeId m_nextId = 1;
    History m_history;
    bool m_inTransaction = false;
};

} // namespace vek

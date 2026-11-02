#pragma once

#include "Node.h"

#include <QJsonObject>
#include <QString>

#include <cstddef>
#include <vector>

namespace vek {

// Eine einzelne Strukturänderung oder Eigenschaftsänderung, vollständig als Daten.
//  Insert: after  = eingefügter Teilbaum
//  Remove: before = entfernter Teilbaum
//  Modify: before/after = {"<id>": Eigenschaften, ...} für alle betroffenen Knoten
struct Change {
    enum class Kind { Insert, Remove, Modify };

    Kind kind = Kind::Modify;
    NodeId id = InvalidNodeId;
    NodeId parentId = InvalidNodeId;
    int index = -1;
    QJsonObject before;
    QJsonObject after;
};

// Ein Undo-Schritt: alle Änderungen einer Transaktion in Ausführungsreihenfolge
struct ChangeSet {
    QString label;
    std::vector<Change> changes;
};

// Undo-/Redo-Verlauf. Reine Datenhaltung; das Anwenden übernimmt das Dokument.
class History {
public:
    void push(ChangeSet set);
    void clear();

    bool canUndo() const { return m_cursor > 0; }
    bool canRedo() const { return m_cursor < m_sets.size(); }
    const ChangeSet& nextUndo() const { return m_sets[m_cursor - 1]; }
    const ChangeSet& nextRedo() const { return m_sets[m_cursor]; }
    void stepBack() { --m_cursor; }
    void stepForward() { ++m_cursor; }

    bool isClean() const { return m_cleanIndex == static_cast<std::ptrdiff_t>(m_cursor); }
    void setClean() { m_cleanIndex = static_cast<std::ptrdiff_t>(m_cursor); }

    std::size_t size() const { return m_sets.size(); }
    std::size_t undoCount() const { return m_cursor; }
    // Höchstens so viele Schritte behalten (die ältesten fallen weg), 0 = unbegrenzt
    void setLimit(std::size_t limit);
    std::size_t limit() const { return m_limit; }
    // Zähler aller je abgelegten Schritte (wächst nur): Marke für merge()
    std::size_t pushed() const { return m_pushed; }
    // Alle seit der Marke abgelegten Schritte zu einem zusammenfassen (z.B. eine KI-Antwort);
    // nur, wenn sie zuoberst liegen (nichts rückgängig gemacht). true, wenn zusammengefasst.
    bool merge(std::size_t mark, const QString& label);

private:
    std::vector<ChangeSet> m_sets;
    std::size_t m_cursor = 0;
    std::ptrdiff_t m_cleanIndex = 0; // -1: sauberer Zustand nicht mehr erreichbar
    std::size_t m_limit = 0;
    std::size_t m_pushed = 0;
    void trim();
};

} // namespace vek

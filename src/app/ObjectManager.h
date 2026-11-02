#pragma once

#include "Editor.h"

#include <QTreeWidget>

namespace vek {

// Objekt-Manager: zeigt Masterseite und alle Seiten mit Ebenen und Objekten als Baum.
// Auswahl ist in beide Richtungen mit der Zeichenfläche verbunden.
class ObjectManager : public QTreeWidget {
    Q_OBJECT

public:
    explicit ObjectManager(Editor& editor, QWidget* parent = nullptr);

private:
    void rebuild();
    void syncSelectionFromEditor();
    void onItemSelectionChanged();
    QTreeWidgetItem* addNode(QTreeWidgetItem* parent, const Node& node);

    Editor& m_editor;
    QHash<NodeId, QTreeWidgetItem*> m_items;
    bool m_syncing = false;
};

// Anzeigename eines Knotens, z.B. "Rechteck" oder "Gruppe aus 3 Objekten"
QString displayName(const Node& node);

} // namespace vek

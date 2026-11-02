#include "ObjectManager.h"

#include "CurveObject.h"
#include "GroupObject.h"
#include "RectangleObject.h"

#include <QHeaderView>

namespace vek {

QString displayName(const Node& node)
{
    if (!node.name().isEmpty())
        return node.name();
    if (auto* page = node_cast<Page>(&node))
        return page->master() ? QObject::tr("Masterseite") : QObject::tr("Seite %1").arg(page->indexInParent());
    if (node_cast<GroupObject>(&node))
        return QObject::tr("Gruppe aus %1 Objekten").arg(node.childCount());
    if (node_cast<RectangleObject>(&node))
        return QObject::tr("Rechteck");
    if (node_cast<CurveObject>(&node))
        return QObject::tr("Kurve");
    return node.typeName();
}

ObjectManager::ObjectManager(Editor& editor, QWidget* parent)
    : QTreeWidget(parent), m_editor(editor)
{
    setHeaderHidden(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(&m_editor, &Editor::documentChanged, this, &ObjectManager::rebuild);
    connect(&m_editor, &Editor::documentReplaced, this, &ObjectManager::rebuild);
    connect(&m_editor, &Editor::currentPageChanged, this, &ObjectManager::rebuild);
    connect(&m_editor, &Editor::selectionChanged, this, &ObjectManager::syncSelectionFromEditor);
    connect(this, &QTreeWidget::itemSelectionChanged, this, &ObjectManager::onItemSelectionChanged);
    rebuild();
}

QTreeWidgetItem* ObjectManager::addNode(QTreeWidgetItem* parent, const Node& node)
{
    auto* item = new QTreeWidgetItem(parent);
    item->setText(0, displayName(node));
    item->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(node.id()));
    const bool selectable = node_cast<DrawObject>(&node) != nullptr;
    if (!selectable)
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    if (auto* layer = node_cast<Layer>(&node); layer && !layer->visible())
        item->setForeground(0, palette().color(QPalette::Disabled, QPalette::Text));
    m_items.insert(node.id(), item);
    for (int i = node.childCount() - 1; i >= 0; --i) // vorderstes Objekt oben
        addNode(item, *node.child(i));
    return item;
}

void ObjectManager::rebuild()
{
    m_syncing = true;
    clear();
    m_items.clear();
    const Document& doc = m_editor.document();
    auto* root = invisibleRootItem();
    addNode(root, doc.masterPage())->setExpanded(true);
    for (int i = 0; i < doc.pageCount(); ++i) {
        QTreeWidgetItem* item = addNode(root, doc.page(i));
        const bool current = i == m_editor.currentPageIndex();
        QFont f = item->font(0);
        f.setBold(current);
        item->setFont(0, f);
        item->setExpanded(current);
    }
    for (QTreeWidgetItem* layerItem : findItems(QStringLiteral("*"), Qt::MatchWildcard | Qt::MatchRecursive)) {
        if (node_cast<Layer>(doc.find(layerItem->data(0, Qt::UserRole).toULongLong())))
            layerItem->setExpanded(true);
    }
    m_syncing = false;
    syncSelectionFromEditor();
}

void ObjectManager::syncSelectionFromEditor()
{
    m_syncing = true;
    clearSelection();
    for (NodeId id : m_editor.selection()) {
        if (QTreeWidgetItem* item = m_items.value(id)) {
            item->setSelected(true);
            scrollToItem(item);
        }
    }
    m_syncing = false;
}

void ObjectManager::onItemSelectionChanged()
{
    if (m_syncing)
        return;
    const Document& doc = m_editor.document();
    QList<NodeId> ids;
    int targetPage = -1;
    for (QTreeWidgetItem* item : selectedItems()) {
        const NodeId id = item->data(0, Qt::UserRole).toULongLong();
        const Page* page = doc.pageOf(id);
        if (page && !page->master())
            targetPage = doc.pageIndex(page->id());
        ids.append(id);
    }
    // Objekt auf einer anderen Seite gewählt: dorthin wechseln
    if (targetPage >= 0 && targetPage != m_editor.currentPageIndex())
        m_editor.setCurrentPageIndex(targetPage);
    m_editor.setSelection(ids);
}

} // namespace vek

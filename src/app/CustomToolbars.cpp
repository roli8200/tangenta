#include "CustomToolbars.h"

#include <QStyle>
#include <QAction>
#include <QActionEvent>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QSettings>
#include <QToolButton>

#include <functional>

namespace vek {

namespace {

const QString kSettingsKey = QStringLiteral("toolbars/custom");

QString plainText(QString text)
{
    text.remove(QLatin1Char('&'));
    const int tab = text.indexOf(QLatin1Char('\t'));
    if (tab >= 0)
        text.truncate(tab);
    return text.trimmed();
}

void collect(QMenu* menu, const QString& path, const QString& index, QList<CustomToolbars::Command>& out)
{
    const QList<QAction*> actions = menu->actions();
    for (int i = 0; i < actions.size(); ++i) {
        QAction* a = actions[i];
        if (a->isSeparator() || a->property("dynamicEntry").toBool())
            continue;
        const QString p = path + QLatin1Char('/') + plainText(a->text());
        const QString n = index + QLatin1Char('/') + QString::number(i);
        if (a->menu())
            collect(a->menu(), p, n, out);
        else
            out.append({p, n, a});
    }
}

} // namespace

// --- CustomToolBar -----------------------------------------------------------------------------

CustomToolBar::CustomToolBar(CustomToolbars& owner, const QString& id, const QString& name, QWidget* parent)
    : QToolBar(name, parent), m_owner(owner), m_id(id)
{
    setObjectName(id);
    setIconSize(QSize(20, 20));
    setAcceptDrops(true);
    setMovable(true);
    setFloatable(true);
    setToolTip(QString());
    connect(this, &QToolBar::orientationChanged, this, &CustomToolBar::updateMinimumSize);
    updateMinimumSize();
}

bool CustomToolBar::editing() const
{
    return m_owner.customizing() || (QApplication::keyboardModifiers() & Qt::AltModifier);
}

void CustomToolBar::insertCommand(QAction* command, int index)
{
    const QList<QAction*> list = actions();
    if (index < 0 || index >= list.size())
        addAction(command);
    else
        insertAction(list[index], command);
}

void CustomToolBar::insertSeparatorAt(int index)
{
    const QList<QAction*> list = actions();
    if (index < 0 || index >= list.size())
        addSeparator();
    else
        insertSeparator(list[index]);
}

void CustomToolBar::removeAt(int index)
{
    const QList<QAction*> list = actions();
    if (index < 0 || index >= list.size())
        return;
    QAction* a = list[index];
    removeAction(a);
    // Trennlinien gehören der Leiste, Befehle den Menüs
    if (a->isSeparator() && a->parent() == this)
        a->deleteLater();
}

void CustomToolBar::moveItem(int from, int to)
{
    const QList<QAction*> list = actions();
    if (from < 0 || from >= list.size() || to < 0 || to > list.size() || to == from || to == from + 1)
        return;
    QAction* a = list[from];
    QAction* before = to < list.size() ? list[to] : nullptr;
    removeAction(a);
    if (before)
        insertAction(before, a);
    else
        addAction(a);
}

QWidget* CustomToolBar::itemWidget(int index) const
{
    const QList<QAction*> list = actions();
    return index >= 0 && index < list.size() ? widgetForAction(list[index]) : nullptr;
}

int CustomToolBar::dropIndex(const QPoint& pos) const
{
    const QList<QAction*> list = actions();
    for (int i = 0; i < list.size(); ++i) {
        const QWidget* w = widgetForAction(list[i]);
        if (!w || !w->isVisible())
            continue;
        const QPoint c = w->geometry().center();
        if (orientation() == Qt::Horizontal ? pos.x() < c.x() : pos.y() < c.y())
            return i;
    }
    return list.size();
}

void CustomToolBar::actionEvent(QActionEvent* event)
{
    QToolBar::actionEvent(event);
    if (event->type() == QEvent::ActionAdded) {
        if (QWidget* w = widgetForAction(event->action()))
            w->installEventFilter(this);
    }
    if (event->type() != QEvent::ActionChanged) {
        updateMinimumSize();
        m_owner.save();
        emit m_owner.changed();
    }
}

bool CustomToolBar::eventFilter(QObject* watched, QEvent* event)
{
    auto* w = qobject_cast<QWidget*>(watched);
    if (!w || w->parentWidget() != this)
        return QToolBar::eventFilter(watched, event);
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* e = static_cast<QMouseEvent*>(event);
        if (e->button() == Qt::RightButton && m_owner.customizing()) {
            // Auch für ausgegraute Befehle (die bekommen von Qt kein Kontextmenü-Ereignis)
            QContextMenuEvent forwarded(QContextMenuEvent::Mouse, w->mapTo(this, e->position().toPoint()),
                                        e->globalPosition().toPoint());
            contextMenuEvent(&forwarded);
            return true;
        }
        if (e->button() != Qt::LeftButton || !editing())
            break;
        m_pressed = w;
        m_pressPos = e->globalPosition().toPoint();
        return true;
    }
    case QEvent::MouseMove: {
        if (!m_pressed || m_pressed != w)
            break;
        auto* e = static_cast<QMouseEvent*>(event);
        if ((e->globalPosition().toPoint() - m_pressPos).manhattanLength() < QApplication::startDragDistance())
            return true;
        const QList<QAction*> list = actions();
        int index = -1;
        for (int i = 0; i < list.size(); ++i)
            if (widgetForAction(list[i]) == w)
                index = i;
        m_pressed = nullptr;
        if (index >= 0)
            startDrag(index);
        return true;
    }
    case QEvent::MouseButtonRelease:
        if (m_pressed == w) {
            m_pressed = nullptr;
            return true;
        }
        break;
    case QEvent::MouseButtonDblClick:
        if (editing())
            return true;
        break;
    case QEvent::ContextMenu:
        // Beim Anpassen öffnet schon der Rechtsklick (MouseButtonPress) das Menü der Schaltfläche
        if (m_owner.customizing())
            return true;
        break;
    default:
        break;
    }
    return QToolBar::eventFilter(watched, event);
}

void CustomToolBar::startDrag(int index)
{
    QAction* a = actions().value(index);
    if (!a)
        return;
    auto* drag = new QDrag(this);
    drag->setMimeData(CustomToolbars::mimeFor(a, m_id, index));
    if (QWidget* w = widgetForAction(a))
        drag->setPixmap(w->grab());
    const Qt::DropAction result = drag->exec(Qt::MoveAction | Qt::LinkAction, Qt::MoveAction);
    // Link: innerhalb dieser Leiste verschoben; Move: in eine andere Leiste; nichts: herausgezogen
    if (result == Qt::LinkAction)
        return;
    const int now = actions().indexOf(a);
    if (now >= 0)
        removeAt(now);
}

void CustomToolBar::dragEnterEvent(QDragEnterEvent* event)
{
    if (!CustomToolbars::actionFrom(event->mimeData())) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
    m_dropIndex = dropIndex(event->position().toPoint());
    update();
}

void CustomToolBar::dragMoveEvent(QDragMoveEvent* event)
{
    if (!CustomToolbars::actionFrom(event->mimeData())) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
    const int index = dropIndex(event->position().toPoint());
    if (index != m_dropIndex) {
        m_dropIndex = index;
        update();
    }
}

void CustomToolBar::dragLeaveEvent(QDragLeaveEvent* event)
{
    QToolBar::dragLeaveEvent(event);
    m_dropIndex = -1;
    update();
}

void CustomToolBar::dropEvent(QDropEvent* event)
{
    QString sourceBar;
    int sourceIndex = -1;
    QAction* a = CustomToolbars::actionFrom(event->mimeData(), &sourceBar, &sourceIndex);
    const int index = dropIndex(event->position().toPoint());
    m_dropIndex = -1;
    update();
    if (!a) {
        event->ignore();
        return;
    }
    if (sourceBar == m_id && sourceIndex >= 0) {
        moveItem(sourceIndex, index);
        event->setDropAction(Qt::LinkAction);
        event->accept();
        return;
    }
    if (a->isSeparator()) {
        insertSeparatorAt(index);
    } else {
        // Ein Befehl steht höchstens einmal in derselben Leiste
        const int existing = actions().indexOf(a);
        if (existing >= 0) {
            moveItem(existing, index);
        } else {
            insertCommand(a, index);
        }
    }
    event->setDropAction(sourceBar.isEmpty() ? Qt::CopyAction : Qt::MoveAction);
    event->accept();
}

void CustomToolBar::paintEvent(QPaintEvent* event)
{
    QToolBar::paintEvent(event);
    QPainter p(this);
    if (actions().isEmpty()) {
        p.setPen(QColor(0x80, 0x86, 0x8e));
        QFont f = font();
        f.setItalic(true);
        p.setFont(f);
        const int handle = isMovable() && !isFloating() ? style()->pixelMetric(QStyle::PM_ToolBarHandleExtent, nullptr, this) : 0;
        const QRect area = rect().adjusted(handle + 10, 0, -4, 0);
        if (orientation() == Qt::Horizontal)
            p.drawText(area, Qt::AlignVCenter | Qt::AlignLeft, tr("Befehle hierher ziehen"));
    }
    if (m_owner.customizing()) {
        QPen pen(QColor(0x3d, 0x7b, 0xd6), 1, Qt::DashLine);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect().adjusted(0, 0, -1, -1));
    }
    if (m_dropIndex >= 0) {
        // Einfügemarke zwischen den Schaltflächen
        const QList<QAction*> list = actions();
        const bool horizontal = orientation() == Qt::Horizontal;
        int at = horizontal ? 10 : 10;
        if (m_dropIndex < list.size()) {
            if (const QWidget* w = widgetForAction(list[m_dropIndex]))
                at = horizontal ? w->geometry().left() - 1 : w->geometry().top() - 1;
        } else if (!list.isEmpty()) {
            if (const QWidget* w = widgetForAction(list.last()))
                at = horizontal ? w->geometry().right() + 2 : w->geometry().bottom() + 2;
        }
        p.setPen(QPen(QColor(0x1a, 0x1a, 0x1a), 2));
        if (horizontal)
            p.drawLine(at, 3, at, height() - 4);
        else
            p.drawLine(3, at, width() - 4, at);
    }
}

void CustomToolBar::contextMenuEvent(QContextMenuEvent* event)
{
    if (!m_owner.customizing()) {
        QToolBar::contextMenuEvent(event);
        return;
    }
    const int index = [&] {
        const QList<QAction*> list = actions();
        for (int i = 0; i < list.size(); ++i)
            if (const QWidget* w = widgetForAction(list[i]); w && w->geometry().contains(event->pos()))
                return i;
        return -1;
    }();
    QMenu menu(this);
    if (index >= 0) {
        menu.addAction(tr("Trennlinie davor einfügen"), this, [this, index] { insertSeparatorAt(index); });
        menu.addAction(tr("Aus der Leiste entfernen"), this, [this, index] { removeAt(index); });
    } else {
        menu.addAction(tr("Trennlinie anfügen"), this, [this] { insertSeparatorAt(-1); });
    }
    menu.exec(event->globalPos());
}

void CustomToolBar::updateMinimumSize()
{
    // Leer bliebe die Leiste nur ein Griff: Platz für den Hinweis und als Ablagefläche
    if (actions().isEmpty())
        setMinimumSize(orientation() == Qt::Horizontal ? QSize(190, 30) : QSize(30, 120));
    else
        setMinimumSize(0, 0);
}

// --- CustomToolbars ----------------------------------------------------------------------------

CustomToolbars::CustomToolbars(QMainWindow* window) : QObject(window), m_window(window)
{
    setObjectName(QStringLiteral("customToolbars"));
}

CustomToolBar* CustomToolbars::bar(const QString& id) const
{
    for (CustomToolBar* b : m_bars)
        if (b->id() == id)
            return b;
    return nullptr;
}

CustomToolBar* CustomToolbars::create(const QString& name)
{
    int n = 1;
    while (bar(QStringLiteral("custom%1").arg(n)))
        ++n;
    auto* b = new CustomToolBar(*this, QStringLiteral("custom%1").arg(n), name, m_window);
    m_bars.append(b);
    m_window->addToolBarBreak(Qt::TopToolBarArea);
    m_window->addToolBar(Qt::TopToolBarArea, b);
    b->show();
    save();
    emit changed();
    return b;
}

void CustomToolbars::rename(CustomToolBar* b, const QString& name)
{
    if (!b || name.trimmed().isEmpty())
        return;
    b->setWindowTitle(name.trimmed());
    save();
    emit changed();
}

void CustomToolbars::remove(CustomToolBar* b)
{
    if (!m_bars.removeOne(b))
        return;
    m_window->removeToolBar(b);
    b->deleteLater();
    save();
    emit changed();
}

QList<CustomToolbars::Command> CustomToolbars::commands() const
{
    QList<Command> out;
    const QList<QAction*> top = m_window->menuBar()->actions();
    for (int i = 0; i < top.size(); ++i)
        if (top[i]->menu())
            collect(top[i]->menu(), plainText(top[i]->text()), QString::number(i), out);
    // Befehle ausserhalb der Menüleiste (Werkzeuge, Schubsen …)
    int g = 0;
    for (QMenu* group : m_window->findChildren<QMenu*>()) {
        if (!group->property("commandGroup").toBool())
            continue;
        collect(group, plainText(group->title()), QStringLiteral("g%1").arg(g++), out);
    }
    return out;
}

QAction* CustomToolbars::resolve(const QString& path, const QString& index) const
{
    const QList<Command> all = commands();
    for (const Command& c : all)
        if (c.path == path)
            return c.action;
    for (const Command& c : all)
        if (!index.isEmpty() && c.index == index)
            return c.action;
    return nullptr;
}

CustomToolbars::Command CustomToolbars::commandFor(QAction* action) const
{
    for (const Command& c : commands())
        if (c.action == action)
            return c;
    return {};
}

QStringList CustomToolbars::barsContaining(QAction* action) const
{
    QStringList names;
    for (const CustomToolBar* b : m_bars)
        if (b->actions().contains(action))
            names << b->windowTitle();
    return names;
}

void CustomToolbars::load()
{
    const QJsonArray list = QJsonDocument::fromJson(QSettings().value(kSettingsKey).toByteArray()).array();
    m_loading = true;
    for (const QJsonValue& v : list) {
        const QJsonObject o = v.toObject();
        const QString id = o.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || bar(id))
            continue;
        auto* b = new CustomToolBar(*this, id, o.value(QStringLiteral("name")).toString(), m_window);
        m_bars.append(b);
        m_window->addToolBarBreak(Qt::TopToolBarArea);
        m_window->addToolBar(Qt::TopToolBarArea, b);
        // Beim Laden nicht nach jedem Eintrag speichern
        const QSignalBlocker block(this);
        for (const QJsonValue& item : o.value(QStringLiteral("items")).toArray()) {
            if (item.toString() == QLatin1String("-")) {
                b->insertSeparatorAt(-1);
            } else {
                const QJsonObject c = item.toObject();
                if (QAction* a = resolve(c.value(QStringLiteral("path")).toString(), c.value(QStringLiteral("index")).toString()))
                    b->insertCommand(a);
            }
        }
    }
    m_loading = false;
}

void CustomToolbars::shutdown()
{
    m_loading = true;
    qDeleteAll(m_bars);
    m_bars.clear();
}

void CustomToolbars::save() const
{
    if (m_loading)
        return;
    QJsonArray list;
    const QList<Command> all = commands();
    for (const CustomToolBar* b : m_bars) {
        QJsonArray items;
        for (QAction* a : b->actions()) {
            if (a->isSeparator()) {
                items.append(QStringLiteral("-"));
                continue;
            }
            for (const Command& c : all) {
                if (c.action == a) {
                    items.append(QJsonObject{{QStringLiteral("path"), c.path}, {QStringLiteral("index"), c.index}});
                    break;
                }
            }
        }
        list.append(QJsonObject{{QStringLiteral("id"), b->id()}, {QStringLiteral("name"), b->windowTitle()},
                                {QStringLiteral("items"), items}});
    }
    QSettings().setValue(kSettingsKey, QJsonDocument(list).toJson(QJsonDocument::Compact));
}

void CustomToolbars::setCustomizing(bool on)
{
    m_customizing = on;
    for (CustomToolBar* b : m_bars)
        b->update();
}

QMimeData* CustomToolbars::mimeFor(QAction* action, const QString& sourceBar, int sourceIndex)
{
    auto* mime = new QMimeData;
    const QString payload = QStringLiteral("%1\n%2\n%3").arg(quintptr(action)).arg(sourceBar).arg(sourceIndex);
    mime->setData(QString::fromLatin1(kMime), payload.toUtf8());
    mime->setText(plainText(action->text()));
    return mime;
}

QAction* CustomToolbars::actionFrom(const QMimeData* mime, QString* sourceBar, int* sourceIndex)
{
    if (!mime || !mime->hasFormat(QString::fromLatin1(kMime)))
        return nullptr;
    const QStringList parts = QString::fromUtf8(mime->data(QString::fromLatin1(kMime))).split(QLatin1Char('\n'));
    if (parts.size() < 3)
        return nullptr;
    // Nur innerhalb desselben Programms: der Zeiger gehört zu einer Aktion dieses Prozesses
    auto* a = reinterpret_cast<QAction*>(parts[0].toULongLong());
    bool known = false;
    for (QWidget* top : QApplication::topLevelWidgets()) {
        if (auto* owner = top->findChild<CustomToolbars*>(QStringLiteral("customToolbars"), Qt::FindDirectChildrenOnly)) {
            for (const Command& c : owner->commands())
                known = known || c.action == a;
            for (const CustomToolBar* b : owner->bars())
                known = known || b->actions().contains(a);
        }
    }
    if (!known)
        return nullptr;
    if (sourceBar)
        *sourceBar = parts[1];
    if (sourceIndex)
        *sourceIndex = parts[2].toInt();
    return a;
}

} // namespace vek

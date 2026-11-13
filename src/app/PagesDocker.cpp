#include "PagesDocker.h"

#include "Canvas.h"
#include "Editor.h"
#include "ExpressionSpinBox.h"
#include "Icons.h"
#include "Options.h"
#include "Serialization.h"

#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QRadioButton>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <algorithm>

namespace vek {

namespace pagecommands {

namespace {

QDialogButtonBox* okCancel(QDialog& dialog)
{
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    return buttons;
}

} // namespace

void rename(QWidget* parent, Editor& editor, int index)
{
    if (index < 0 || index >= editor.document().pageCount())
        return;
    bool ok = false;
    // Mit dem bisherigen Namen vorbelegt (ohne eigenen Namen «Seite n»); bleibt es bei «Seite n»,
    // behält die Seite ihren automatischen Namen
    const QString automatic = QObject::tr("Seite %1").arg(index + 1);
    const QString current = editor.document().page(index).name();
    const QString name = QInputDialog::getText(parent, QObject::tr("Seite umbenennen"), QObject::tr("Seitenname:"),
                                               QLineEdit::Normal, current.isEmpty() ? automatic : current, &ok);
    if (ok)
        editor.renamePage(index, name.trimmed() == automatic ? QString() : name.trimmed());
}

void insertPages(QWidget* parent, Editor& editor, int index)
{
    const int count = editor.document().pageCount();
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Seiten einfügen"));
    auto* form = new QFormLayout(&dialog);
    auto* number = new QSpinBox(&dialog);
    number->setObjectName(QStringLiteral("count"));
    number->setRange(1, 999);
    auto* before = new QRadioButton(QObject::tr("Davor"), &dialog);
    before->setObjectName(QStringLiteral("before"));
    auto* after = new QRadioButton(QObject::tr("Danach"), &dialog);
    after->setObjectName(QStringLiteral("after"));
    after->setChecked(true);
    auto* where = new QHBoxLayout;
    where->addWidget(before);
    where->addWidget(after);
    auto* page = new QSpinBox(&dialog);
    page->setObjectName(QStringLiteral("page"));
    page->setRange(1, count);
    page->setValue(std::clamp(index, 0, count - 1) + 1);
    form->addRow(QObject::tr("Anzahl Seiten:"), number);
    form->addRow(QObject::tr("Einfügen:"), where);
    form->addRow(QObject::tr("Seite:"), page);
    form->addRow(okCancel(dialog));
    if (dialog.exec() != QDialog::Accepted)
        return;
    const int at = page->value() - 1 + (after->isChecked() ? 1 : 0);
    editor.insertPages(at, number->value());
}

void duplicate(QWidget* parent, Editor& editor, int index)
{
    if (index < 0 || index >= editor.document().pageCount())
        return;
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Seite duplizieren"));
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(QObject::tr("Neue Seite einfügen:"), &dialog));
    auto* before = new QRadioButton(QObject::tr("Vor der ausgewählten Seite"), &dialog);
    before->setObjectName(QStringLiteral("before"));
    auto* after = new QRadioButton(QObject::tr("Nach der ausgewählten Seite"), &dialog);
    after->setObjectName(QStringLiteral("after"));
    after->setChecked(true);
    auto* placement = new QButtonGroup(&dialog);
    placement->addButton(before);
    placement->addButton(after);
    layout->addWidget(before);
    layout->addWidget(after);
    layout->addSpacing(6);
    auto* layersOnly = new QRadioButton(QObject::tr("Nur Ebenen kopieren"), &dialog);
    layersOnly->setObjectName(QStringLiteral("layersOnly"));
    auto* withContent = new QRadioButton(QObject::tr("Ebenen und Inhalt kopieren"), &dialog);
    withContent->setObjectName(QStringLiteral("withContent"));
    withContent->setChecked(true);
    auto* content = new QButtonGroup(&dialog);
    content->addButton(layersOnly);
    content->addButton(withContent);
    layout->addWidget(layersOnly);
    layout->addWidget(withContent);
    layout->addWidget(okCancel(dialog));
    if (dialog.exec() != QDialog::Accepted)
        return;
    editor.duplicatePage(index, withContent->isChecked(), after->isChecked());
}

void move(QWidget* parent, Editor& editor, int index)
{
    const int count = editor.document().pageCount();
    if (count < 2 || index < 0 || index >= count)
        return;
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Seite verschieben"));
    auto* form = new QFormLayout(&dialog);
    auto* page = new QSpinBox(&dialog);
    page->setObjectName(QStringLiteral("page"));
    page->setRange(1, count);
    page->setValue(index + 1);
    auto* target = new QSpinBox(&dialog);
    target->setObjectName(QStringLiteral("target"));
    target->setRange(1, count);
    target->setValue(index + 1);
    form->addRow(QObject::tr("Seite:"), page);
    form->addRow(QObject::tr("Neue Position:"), target);
    form->addRow(okCancel(dialog));
    if (dialog.exec() != QDialog::Accepted)
        return;
    editor.movePage(page->value() - 1, target->value() - 1);
}

void toggleOrientation(Editor& editor, int index)
{
    editor.setPageOrientation(index, !editor.isPageLandscape(index));
}

void fillMenu(QMenu& menu, QWidget* parent, Editor& editor, int index)
{
    menu.addAction(QObject::tr("Seite &umbenennen …"), parent, [parent, &editor, index] { rename(parent, editor, index); });
    menu.addSeparator();
    menu.addAction(icon(QStringLiteral("page-insert")), QObject::tr("Seite &davor einfügen"), parent,
                   [&editor, index] { editor.insertPage(index); });
    menu.addAction(icon(QStringLiteral("page-insert")), QObject::tr("Seite d&anach einfügen"), parent,
                   [&editor, index] { editor.insertPage(index + 1); });
    menu.addAction(QObject::tr("Seiten &einfügen …"), parent, [parent, &editor, index] { insertPages(parent, editor, index); });
    menu.addAction(icon(QStringLiteral("page-duplicate")), QObject::tr("Seite d&uplizieren …"), parent,
                   [parent, &editor, index] { duplicate(parent, editor, index); });
    QAction* mv = menu.addAction(QObject::tr("Seite &verschieben …"), parent, [parent, &editor, index] { move(parent, editor, index); });
    mv->setEnabled(editor.document().pageCount() > 1);
    QAction* del = menu.addAction(icon(QStringLiteral("page-delete")), QObject::tr("Seite &löschen"), parent,
                                  [&editor, index] { editor.deletePage(index); });
    del->setEnabled(editor.document().pageCount() > 1);
    menu.addSeparator();
    menu.addAction(icon(editor.isPageLandscape(index) ? QStringLiteral("page-portrait") : QStringLiteral("page-landscape")),
                   QObject::tr("Seitenausrichtung &wechseln"), parent, [&editor, index] { toggleOrientation(editor, index); });
}

} // namespace pagecommands

// --- Liste ---------------------------------------------------------------------------------------

PageThumbnailList::PageThumbnailList(QWidget* parent)
    : QListWidget(parent)
{
    setDragEnabled(false);
    setDragDropMode(QAbstractItemView::NoDragDrop);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setMovement(QListView::Static);
    setResizeMode(QListView::Adjust);
    setUniformItemSizes(true);
}

int PageThumbnailList::insertionAt(const QPoint& pos) const
{
    const QListWidgetItem* it = itemAt(pos);
    if (!it) {
        // Unter dem letzten Eintrag: ans Ende
        int last = -1;
        for (int i = 0; i < count(); ++i) {
            if (!item(i)->isHidden())
                last = std::max(last, PagesDocker::pageOf(item(i)));
        }
        return last + 1;
    }
    const QRect r = visualItemRect(it);
    const bool after = viewMode() == QListView::IconMode ? pos.x() > r.center().x() : pos.y() > r.center().y();
    return PagesDocker::pageOf(it) + (after ? 1 : 0);
}

void PageThumbnailList::mousePressEvent(QMouseEvent* e)
{
    QListWidget::mousePressEvent(e);
    m_dragging = false;
    m_insertion = -1;
    const QListWidgetItem* it = itemAt(e->position().toPoint());
    m_pressPage = e->button() == Qt::LeftButton && it ? PagesDocker::pageOf(it) : -1;
    m_pressPos = e->position().toPoint();
}

void PageThumbnailList::mouseMoveEvent(QMouseEvent* e)
{
    if (m_pressPage >= 0 && (e->buttons() & Qt::LeftButton)) {
        const QPoint pos = e->position().toPoint();
        if (!m_dragging && (pos - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            m_dragging = true;
            viewport()->setCursor(Qt::DragMoveCursor);
        }
        if (m_dragging) {
            m_insertion = insertionAt(pos);
            viewport()->update();
        }
        return;
    }
    QListWidget::mouseMoveEvent(e);
}

void PageThumbnailList::mouseReleaseEvent(QMouseEvent* e)
{
    if (m_dragging && e->button() == Qt::LeftButton) {
        const int from = m_pressPage;
        const int insertion = insertionAt(e->position().toPoint());
        m_dragging = false;
        m_pressPage = -1;
        m_insertion = -1;
        viewport()->unsetCursor();
        viewport()->update();
        // Einfügestelle hinter der Seite selbst: Index nach dem Herauslösen
        const int to = insertion > from ? insertion - 1 : insertion;
        if (to != from)
            emit pageMoveRequested(from, to);
        return;
    }
    m_pressPage = -1;
    QListWidget::mouseReleaseEvent(e);
}

void PageThumbnailList::paintEvent(QPaintEvent* e)
{
    QListWidget::paintEvent(e);
    if (!m_dragging || m_insertion < 0)
        return;
    // Einfügemarke: blaue Linie vor bzw. hinter dem Eintrag
    const QListWidgetItem* before = nullptr;
    const QListWidgetItem* after = nullptr;
    for (int i = 0; i < count(); ++i) {
        const int page = PagesDocker::pageOf(item(i));
        if (page == m_insertion)
            before = item(i);
        if (page == m_insertion - 1)
            after = item(i);
    }
    QPainter p(viewport());
    p.setPen(QPen(Canvas::previewColor(), 2));
    const bool grid = viewMode() == QListView::IconMode;
    if (before) {
        const QRect r = visualItemRect(before);
        grid ? p.drawLine(r.topLeft(), r.bottomLeft()) : p.drawLine(r.topLeft(), r.topRight());
    } else if (after) {
        const QRect r = visualItemRect(after);
        grid ? p.drawLine(r.topRight(), r.bottomRight()) : p.drawLine(r.bottomLeft(), r.bottomRight());
    }
}

// --- Andockfenster -------------------------------------------------------------------------------

PagesDocker::PagesDocker(Editor& editor, Canvas& canvas, QWidget* parent)
    : QWidget(parent), m_editor(editor), m_canvas(canvas)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Suchen"));
    m_search->setClearButtonEnabled(true);
    layout->addWidget(m_search);

    auto* bar = new QHBoxLayout;
    bar->setSpacing(1);
    auto button = [this, bar](const QString& iconName, const QString& tip) {
        auto* b = new QToolButton(this);
        b->setIcon(icon(iconName));
        b->setIconSize(QSize(18, 18));
        b->setToolTip(tip);
        b->setAutoRaise(true);
        bar->addWidget(b);
        return b;
    };
    QToolButton* add = button(QStringLiteral("page-insert"), tr("Seite hinzufügen"));
    add->setObjectName(QStringLiteral("addPage"));
    connect(add, &QToolButton::clicked, this, [this] { m_editor.insertPage(m_editor.currentPageIndex() + 1); });
    QToolButton* remove = button(QStringLiteral("page-delete"), tr("Seite löschen"));
    remove->setObjectName(QStringLiteral("deletePage"));
    connect(remove, &QToolButton::clicked, this, [this] { m_editor.deletePage(m_editor.currentPageIndex()); });
    bar->addStretch(1);
    m_multiButton = button(QStringLiteral("pages-multi"), tr("Mehrseitenansicht"));
    m_multiButton->setObjectName(QStringLiteral("multiPage"));
    m_multiButton->setCheckable(true);
    connect(m_multiButton, &QToolButton::clicked, this, [this](bool on) { m_canvas.setMultiPageView(on); });

    // Einstellungen der Mehrseitenansicht: Anordnung, Spalten, Abstand
    QToolButton* settings = button(QStringLiteral("page-settings"), tr("Einstellungen der Mehrseitenansicht"));
    settings->setObjectName(QStringLiteral("multiPageSettings"));
    settings->setPopupMode(QToolButton::InstantPopup);
    auto* settingsMenu = new QMenu(settings);
    auto* panel = new QWidget(settingsMenu);
    auto* form = new QFormLayout(panel);
    auto* arrangement = new QComboBox(panel);
    arrangement->addItems({tr("Raster"), tr("Waagrecht"), tr("Senkrecht")});
    auto* columns = new QSpinBox(panel);
    columns->setRange(1, 50);
    auto* spacing = new ExpressionSpinBox(panel);
    spacing->setRange(0, 1000);
    spacing->setDecimals(1);
    spacing->setSuffix(QStringLiteral(" mm"));
    form->addRow(tr("Layout:"), arrangement);
    form->addRow(tr("Spalten:"), columns);
    form->addRow(tr("Abstand:"), spacing);
    auto* panelAction = new QWidgetAction(settingsMenu);
    panelAction->setDefaultWidget(panel);
    settingsMenu->addAction(panelAction);
    settings->setMenu(settingsMenu);
    connect(settingsMenu, &QMenu::aboutToShow, this, [=] {
        const Options& o = Options::instance();
        QSignalBlocker b1(arrangement), b2(columns), b3(spacing);
        arrangement->setCurrentIndex(std::clamp(o.integer(QStringLiteral("doc/layout/multiArrangement")), 0, 2));
        columns->setValue(o.integer(QStringLiteral("doc/layout/multiColumns")));
        spacing->setValue(o.number(QStringLiteral("doc/layout/multiSpacing")));
        columns->setEnabled(arrangement->currentIndex() == 0);
    });
    // Nach jeder Änderung des Layouts alle Seiten neu einpassen
    auto refit = [this] {
        if (m_canvas.multiPageView())
            m_canvas.fitAllPages();
    };
    connect(arrangement, &QComboBox::currentIndexChanged, this, [columns, refit](int i) {
        columns->setEnabled(i == 0);
        Options::instance().set(QStringLiteral("doc/layout/multiArrangement"), i);
        refit();
    });
    connect(columns, &QSpinBox::valueChanged, this, [refit](int v) {
        Options::instance().set(QStringLiteral("doc/layout/multiColumns"), v);
        refit();
    });
    connect(spacing, &QDoubleSpinBox::valueChanged, this, [refit](double v) {
        Options::instance().set(QStringLiteral("doc/layout/multiSpacing"), v);
        refit();
    });

    // Optionen: alle Seitenbefehle
    QToolButton* options = button(QString(), tr("Optionen"));
    options->setObjectName(QStringLiteral("pageOptions"));
    options->setText(QStringLiteral("…"));
    options->setPopupMode(QToolButton::InstantPopup);
    auto* optionsMenu = new QMenu(options);
    options->setMenu(optionsMenu);
    connect(optionsMenu, &QMenu::aboutToShow, this, [this, optionsMenu] {
        optionsMenu->clear();
        pagecommands::fillMenu(*optionsMenu, this, m_editor, m_editor.currentPageIndex());
        optionsMenu->addSeparator();
        QAction* multi = optionsMenu->addAction(tr("&Mehrseitenansicht"), this, [this](bool on) { m_canvas.setMultiPageView(on); });
        multi->setCheckable(true);
        multi->setChecked(m_canvas.multiPageView());
    });
    layout->addLayout(bar);

    m_list = new PageThumbnailList(this);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_list, 1);

    // Unten: Listen- oder Rasteransicht, Miniaturgrösse
    auto* bottom = new QHBoxLayout;
    bottom->setSpacing(1);
    auto toggle = [this, bottom](const QString& iconName, const QString& tip) {
        auto* b = new QToolButton(this);
        b->setIcon(icon(iconName));
        b->setIconSize(QSize(16, 16));
        b->setToolTip(tip);
        b->setAutoRaise(true);
        b->setCheckable(true);
        bottom->addWidget(b);
        return b;
    };
    m_listButton = toggle(QStringLiteral("pages-list"), tr("Listenansicht"));
    m_listButton->setObjectName(QStringLiteral("listView"));
    m_gridButton = toggle(QStringLiteral("pages-grid"), tr("Rasteransicht"));
    m_gridButton->setObjectName(QStringLiteral("gridView"));
    auto* views = new QButtonGroup(this);
    views->addButton(m_listButton);
    views->addButton(m_gridButton);
    m_listButton->setChecked(true);
    connect(m_listButton, &QToolButton::clicked, this, [this] { setGridView(false); });
    connect(m_gridButton, &QToolButton::clicked, this, [this] { setGridView(true); });
    m_size = new QSlider(Qt::Horizontal, this);
    m_size->setRange(32, 240);
    m_size->setToolTip(tr("Miniaturgrösse"));
    bottom->addSpacing(8);
    bottom->addWidget(m_size, 1);
    layout->addLayout(bottom);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(150);
    connect(m_timer, &QTimer::timeout, this, &PagesDocker::updateThumbnails);

    // Ansicht und Grösse je Benutzer merken
    QSettings s;
    m_size->setValue(s.value(QStringLiteral("pagesDocker/size"), 64).toInt());
    m_list->setIconSize(QSize(m_size->value(), m_size->value()));
    setGridView(s.value(QStringLiteral("pagesDocker/grid"), false).toBool());
    connect(m_size, &QSlider::valueChanged, this, [this](int v) {
        QSettings().setValue(QStringLiteral("pagesDocker/size"), v);
        m_list->setIconSize(QSize(v, v));
        if (gridView())
            m_list->setGridSize(gridCell(v));
        m_thumbnails.clear();
        m_dirty = true;
        scheduleThumbnails();
    });

    connect(m_search, &QLineEdit::textChanged, this, &PagesDocker::applyFilter);
    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        if (item)
            m_editor.setCurrentPageIndex(pageOf(item));
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        m_editor.setCurrentPageIndex(pageOf(item));
        m_canvas.fitPage();
    });
    connect(m_list, &PageThumbnailList::pageMoveRequested, this, [this](int from, int to) { m_editor.movePage(from, to); });
    connect(m_list, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        const QListWidgetItem* item = m_list->itemAt(pos);
        if (!item)
            return;
        const int index = pageOf(item);
        m_editor.setCurrentPageIndex(index);
        QMenu menu;
        pagecommands::fillMenu(menu, this, m_editor, index);
        menu.exec(m_list->viewport()->mapToGlobal(pos));
    });

    connect(&m_editor, &Editor::documentChanged, this, &PagesDocker::rebuild);
    connect(&m_editor, &Editor::documentReplaced, this, [this] {
        m_thumbnails.clear();
        rebuild();
    });
    connect(&m_editor, &Editor::currentPageChanged, this, [this] {
        QSignalBlocker block(m_list);
        if (QListWidgetItem* item = itemForPage(m_editor.currentPageIndex()))
            m_list->setCurrentItem(item);
    });
    rebuild();
}

QSize PagesDocker::gridCell(int icon) const
{
    // Platz für zwei Zeilen Namen und mindestens «8: Kontakt» ausgeschrieben
    const QFontMetrics fm(m_list->font());
    return QSize(std::max(icon + 24, fm.horizontalAdvance(QStringLiteral("00: Wandleuchten")) + 12),
                 icon + 2 * fm.lineSpacing() + 14);
}

bool PagesDocker::gridView() const
{
    return m_list->viewMode() == QListView::IconMode;
}

void PagesDocker::setGridView(bool grid)
{
    QSettings().setValue(QStringLiteral("pagesDocker/grid"), grid);
    m_list->setViewMode(grid ? QListView::IconMode : QListView::ListMode);
    m_list->setFlow(grid ? QListView::LeftToRight : QListView::TopToBottom);
    m_list->setWrapping(grid);
    m_list->setMovement(QListView::Static);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setSpacing(grid ? 4 : 2);
    m_list->setGridSize(grid ? gridCell(m_size->value()) : QSize());
    m_list->setWordWrap(grid);
    (grid ? m_gridButton : m_listButton)->setChecked(true);
}

int PagesDocker::pageOf(const QListWidgetItem* item)
{
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

QListWidgetItem* PagesDocker::itemForPage(int index) const
{
    for (int i = 0; i < m_list->count(); ++i) {
        if (pageOf(m_list->item(i)) == index)
            return m_list->item(i);
    }
    return nullptr;
}

void PagesDocker::rebuild()
{
    QSignalBlocker block(m_list);
    const Document& doc = m_editor.document();
    while (m_list->count() > doc.pageCount())
        delete m_list->takeItem(m_list->count() - 1);
    for (int i = 0; i < doc.pageCount(); ++i) {
        QListWidgetItem* item = i < m_list->count() ? m_list->item(i) : new QListWidgetItem(m_list);
        item->setData(Qt::UserRole, i);
        item->setText(m_canvas.pageTitle(i));
        const auto it = m_thumbnails.constFind(doc.page(i).id());
        if (it == m_thumbnails.constEnd())
            item->setIcon(QIcon());
    }
    if (QListWidgetItem* item = itemForPage(m_editor.currentPageIndex()))
        m_list->setCurrentItem(item);
    m_multiButton->setChecked(m_canvas.multiPageView());
    applyFilter();
    m_dirty = true;
    scheduleThumbnails();
}

void PagesDocker::applyFilter()
{
    const QString text = m_search->text().trimmed();
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem* item = m_list->item(i);
        item->setHidden(!text.isEmpty() && !item->text().contains(text, Qt::CaseInsensitive));
    }
}

void PagesDocker::scheduleThumbnails()
{
    if (isVisible())
        m_timer->start();
}

void PagesDocker::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    if (m_dirty)
        m_timer->start();
}

namespace {

// Kennung des sichtbaren Inhalts einer Seite: ändert sie sich nicht, bleibt die Miniatur
QByteArray pageSignature(const Document& doc, int index, int extent)
{
    QByteArray data = QJsonDocument(serial::toJson(doc.page(index))).toJson(QJsonDocument::Compact);
    data += QJsonDocument(serial::toJson(doc.masterPage())).toJson(QJsonDocument::Compact);
    data += QByteArray::number(extent);
    return QCryptographicHash::hash(data, QCryptographicHash::Md5);
}

} // namespace

QImage PagesDocker::renderThumbnail(int index, int extent) const
{
    const QSizeF size = m_editor.document().page(index).size();
    const qreal dpr = devicePixelRatioF();
    const double scale = extent * dpr / std::max(size.width(), size.height());
    QImage image(std::max(1, int(std::round(size.width() * scale))), std::max(1, int(std::round(size.height() * scale))),
                 QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter p(&image);
    m_canvas.renderPage(p, index, scale, QPointF(0, 0), true);
    p.setPen(QPen(QColor(0x9a, 0x9a, 0x9a), 0));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(0.5, 0.5, image.width() - 1, image.height() - 1));
    p.end();
    image.setDevicePixelRatio(dpr);
    return image;
}

void PagesDocker::updateThumbnails()
{
    m_timer->stop();
    m_dirty = false;
    const Document& doc = m_editor.document();
    const int extent = m_size->value();
    QHash<NodeId, Thumbnail> kept;
    for (int i = 0; i < doc.pageCount() && i < m_list->count(); ++i) {
        const NodeId id = doc.page(i).id();
        const QByteArray signature = pageSignature(doc, i, extent);
        Thumbnail thumb = m_thumbnails.value(id);
        if (thumb.image.isNull() || thumb.signature != signature)
            thumb = {renderThumbnail(i, extent), signature};
        kept.insert(id, thumb);
        const QImage& image = thumb.image;
        // Miniatur auf ein quadratisches Feld setzen, damit Hoch- und Querformat gleich ausgerichtet sind
        const qreal dpr = image.devicePixelRatio();
        QPixmap cell(QSize(extent, extent) * dpr);
        cell.setDevicePixelRatio(dpr);
        cell.fill(Qt::transparent);
        QPainter p(&cell);
        const QSizeF logical = QSizeF(image.size()) / dpr;
        p.drawImage(QPointF((extent - logical.width()) / 2.0, (extent - logical.height()) / 2.0), image);
        p.end();
        m_list->item(i)->setIcon(QIcon(cell));
    }
    m_thumbnails = kept;
}

int PagesDocker::thumbnailCount() const
{
    int n = 0;
    for (int i = 0; i < m_list->count(); ++i) {
        if (!m_list->item(i)->icon().isNull())
            ++n;
    }
    return n;
}

} // namespace vek

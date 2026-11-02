#include "EditDockers.h"

#include "ColorDialog.h"
#include "Editor.h"
#include "Icons.h"
#include "Outline.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace vek {

namespace {

QIcon swatch(const std::optional<Color>& c)
{
    QPixmap pix(28, 16);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setPen(QColor(0x70, 0x70, 0x70));
    if (c) {
        p.setBrush(c->toQColor());
        p.drawRect(pix.rect().adjusted(0, 0, -1, -1));
    } else {
        p.setBrush(Qt::white);
        p.drawRect(pix.rect().adjusted(0, 0, -1, -1));
        p.setPen(QPen(QColor(0xc0, 0x20, 0x20), 1.5));
        p.drawLine(pix.rect().topRight(), pix.rect().bottomLeft());
    }
    return QIcon(pix);
}

} // namespace

ObjectPropertiesDocker::ObjectPropertiesDocker(Editor& editor, QWidget* parent) : QWidget(parent), m_editor(editor)
{
    auto* form = new QFormLayout(this);
    m_kind = new QLabel(this);
    form->addRow(m_kind);
    m_name = new QLineEdit(this);
    form->addRow(tr("Name:"), m_name);
    m_fill = new QToolButton(this);
    m_fill->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    form->addRow(tr("Füllung:"), m_fill);
    m_stroke = new QToolButton(this);
    m_stroke->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    form->addRow(tr("Kontur:"), m_stroke);
    m_width = new QDoubleSpinBox(this);
    m_width->setRange(0, 1000);
    m_width->setDecimals(3);
    m_width->setSuffix(QStringLiteral(" mm"));
    m_width->setKeyboardTracking(false);
    form->addRow(tr("Konturbreite:"), m_width);
    m_dash = new QComboBox(this);
    m_dash->addItem(tr("Durchgehend"));
    for (int i = 1; i < outline::dashCount(); ++i)
        m_dash->addItem(tr("Linienstil %1").arg(i));
    form->addRow(tr("Linienstil:"), m_dash);
    m_transparency = new QDoubleSpinBox(this);
    m_transparency->setRange(0, 100);
    m_transparency->setSuffix(QStringLiteral(" %"));
    m_transparency->setKeyboardTracking(false);
    form->addRow(tr("Transparenz:"), m_transparency);
    m_blur = new QDoubleSpinBox(this);
    m_blur->setRange(0, 500);
    m_blur->setDecimals(2);
    m_blur->setSuffix(QStringLiteral(" mm"));
    m_blur->setKeyboardTracking(false);
    form->addRow(tr("Unschärfe:"), m_blur);
    m_locked = new QCheckBox(tr("Gesperrt"), this);
    form->addRow(m_locked);

    connect(m_name, &QLineEdit::editingFinished, this, [this] {
        if (!m_updating && m_editor.selection().size() == 1)
            m_editor.renameObject(m_editor.selection().first(), m_name->text());
    });
    connect(m_fill, &QToolButton::clicked, this, [this] {
        const QList<const DrawObject*> objects = m_editor.selectedObjects();
        if (objects.isEmpty())
            return;
        const Color start = objects.first()->style().fill.representativeColor().value_or(Color(255, 255, 255));
        if (const std::optional<Color> c = ColorDialog::getColor(this, tr("Füllung"), start))
            m_editor.setSelectionFill(Fill::solid(*c));
    });
    connect(m_stroke, &QToolButton::clicked, this, [this] {
        const QList<const DrawObject*> objects = m_editor.selectedObjects();
        if (objects.isEmpty())
            return;
        const Color start = objects.first()->style().stroke.value_or(Color(0, 0, 0));
        if (const std::optional<Color> c = ColorDialog::getColor(this, tr("Konturfarbe"), start))
            m_editor.setSelectionStroke(*c);
    });
    connect(m_width, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (!m_updating)
            m_editor.setSelectionStrokeWidth(v);
    });
    connect(m_dash, &QComboBox::activated, this, [this](int i) { m_editor.setSelectionOutlineShape(i, std::nullopt, std::nullopt); });
    connect(m_transparency, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (!m_updating)
            m_editor.setSelectionEffect(Effect::Transparency, [v](Ramp& r) { r = Ramp::uniform(v); }, tr("Transparenz"));
    });
    connect(m_blur, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (!m_updating)
            m_editor.setSelectionBlur(v);
    });
    connect(m_locked, &QCheckBox::toggled, this, [this](bool on) {
        if (!m_updating)
            m_editor.setSelectionLocked(on);
    });
    connect(&m_editor, &Editor::selectionChanged, this, &ObjectPropertiesDocker::refresh);
    connect(&m_editor, &Editor::documentChanged, this, &ObjectPropertiesDocker::refresh);
    refresh();
}

void ObjectPropertiesDocker::refresh()
{
    m_updating = true;
    const QList<const DrawObject*> objects = m_editor.selectedObjects();
    const bool any = !objects.isEmpty();
    for (QWidget* w : std::initializer_list<QWidget*>{m_name, m_fill, m_stroke, m_width, m_dash, m_transparency, m_blur, m_locked})
        w->setEnabled(any);
    if (!any) {
        m_kind->setText(tr("Keine Auswahl"));
        m_updating = false;
        return;
    }
    const DrawObject* o = objects.first();
    m_kind->setText(objects.size() == 1 ? tr("Objekt: %1").arg(o->typeName()) : tr("%1 Objekte").arg(objects.size()));
    m_name->setText(objects.size() == 1 ? o->name() : QString());
    m_name->setEnabled(objects.size() == 1);
    const std::optional<Color> fill = o->style().fill.isNone() ? std::nullopt : o->style().fill.representativeColor();
    m_fill->setIcon(swatch(fill));
    m_fill->setText(o->style().fill.isMesh()       ? tr("Maschenfüllung")
                    : o->style().fill.isGradient() ? tr("Farbverlauf")
                    : fill                          ? fill->toHex()
                                                    : tr("Keine"));
    m_stroke->setIcon(swatch(o->style().stroke));
    m_stroke->setText(o->style().stroke ? o->style().stroke->toHex() : tr("Keine"));
    m_width->setValue(o->style().strokeWidth);
    m_dash->setCurrentIndex(std::clamp(o->style().dash, 0, m_dash->count() - 1));
    m_transparency->setValue(o->transparency().maximum());
    m_blur->setValue(o->blur());
    m_locked->setChecked(o->locked());
    m_updating = false;
}

SymbolDocker::SymbolDocker(Editor& editor, QWidget* parent) : QWidget(parent), m_editor(editor)
{
    auto* layout = new QVBoxLayout(this);
    m_list = new QListWidget(this);
    layout->addWidget(m_list, 1);
    auto* row = new QHBoxLayout;
    auto* insert = new QPushButton(tr("Einfügen"), this);
    auto* rename = new QPushButton(tr("Umbenennen …"), this);
    auto* remove = new QPushButton(tr("Löschen"), this);
    row->addWidget(insert);
    row->addWidget(rename);
    row->addWidget(remove);
    layout->addLayout(row);
    auto current = [this]() -> NodeId { return m_list->currentItem() ? m_list->currentItem()->data(Qt::UserRole).toULongLong() : 0; };
    connect(insert, &QPushButton::clicked, this, [this, current] {
        if (const NodeId id = current())
            m_editor.placeSymbol(id, QRectF(QPointF(0, 0), m_editor.currentPage().size()).center());
    });
    connect(m_list, &QListWidget::itemDoubleClicked, insert, &QPushButton::click);
    connect(rename, &QPushButton::clicked, this, [this, current] {
        const NodeId id = current();
        if (!id)
            return;
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("Symbol umbenennen"), tr("Name:"), QLineEdit::Normal,
                                                   m_list->currentItem()->text().section(QLatin1String("  ("), 0, 0), &ok);
        if (ok && !name.trimmed().isEmpty())
            m_editor.renameSymbol(id, name.trimmed());
    });
    connect(remove, &QPushButton::clicked, this, [this, current] {
        const NodeId id = current();
        if (id && !m_editor.deleteSymbol(id))
            m_list->currentItem()->setToolTip(tr("Wird noch verwendet: erst die Instanzen löschen oder in Objekte zurückwandeln"));
    });
    connect(&m_editor, &Editor::documentChanged, this, &SymbolDocker::refresh);
    connect(&m_editor, &Editor::documentReplaced, this, &SymbolDocker::refresh);
    refresh();
}

void SymbolDocker::refresh()
{
    const NodeId keep = m_list->currentItem() ? m_list->currentItem()->data(Qt::UserRole).toULongLong() : 0;
    m_list->clear();
    for (NodeId id : m_editor.symbols()) {
        const Node* n = m_editor.document().find(id);
        auto* item = new QListWidgetItem(tr("%1  (%2×)").arg(n->name().isEmpty() ? tr("Symbol") : n->name()).arg(m_editor.symbolUses(id)), m_list);
        item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(id));
        if (id == keep)
            m_list->setCurrentItem(item);
    }
}

} // namespace vek

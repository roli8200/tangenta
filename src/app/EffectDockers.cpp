#include "EffectDockers.h"

#include "Canvas.h"
#include "ExpressionSpinBox.h"
#include "ImageDialogs.h"
#include "ColorDialog.h"
#include "Icons.h"
#include "InteractiveTools.h"
#include "Options.h"
#include "PickTool.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCoreApplication>
#include <QPainter>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

namespace vek {

namespace {

void showColor(QPushButton* b, const Color& c)
{
    b->setText(c.toHex());
    b->setStyleSheet(QStringLiteral("background:%1;color:%2").arg(c.toQColor().name(), c.toQColor().lightness() < 128 ? QStringLiteral("white") : QStringLiteral("black")));
}

} // namespace

// --- Linse ------------------------------------------------------------------------------------

LensDocker::LensDocker(Editor& editor, QWidget* parent)
    : QWidget(parent), m_editor(editor)
{
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    layout->addLayout(form);
    m_type = new QComboBox(this);
    // Reihenfolge
    using T = Lens::Type;
    for (const auto& [t, text] : {std::pair{T::None, tr("Kein Linseneffekt")}, std::pair{T::Brighten, tr("Aufhellen")},
                                  std::pair{T::ColorAdd, tr("Farbe addieren")}, std::pair{T::ColorLimit, tr("Farbgrenze")},
                                  std::pair{T::CustomColorMap, tr("Benutzerdefinierte Farbzuordnung")}, std::pair{T::FishEye, tr("Fischauge")},
                                  std::pair{T::HeatMap, tr("Wärmebild")}, std::pair{T::Invert, tr("Invertieren")},
                                  std::pair{T::Magnify, tr("Vergrössern")}, std::pair{T::TintedGrayscale, tr("Getönte Graustufen")},
                                  std::pair{T::Transparency, tr("Transparenz")}, std::pair{T::Wireframe, tr("Drahtmodell")},
                                  std::pair{T::BitmapEffect, tr("Bitmap-Effekt")}})
        m_type->addItem(text, int(t));
    form->addRow(tr("Art:"), m_type);
    // Bitmap-Effekt: welcher Filter
    m_effect = new QComboBox(this);
    for (const auto& [id, name] : Lens::bitmapEffects())
        m_effect->addItem(QCoreApplication::translate("vek::Lens", name), id);
    m_effectLabel = new QLabel(tr("Effekt:"), this);
    form->addRow(m_effectLabel, m_effect);
    m_amount = new ExpressionSpinBox(this);
    m_amountLabel = new QLabel(tr("Stärke:"), this);
    form->addRow(m_amountLabel, m_amount);
    m_color = new QPushButton(this);
    m_colorLabel = new QLabel(tr("Farbe:"), this);
    form->addRow(m_colorLabel, m_color);
    m_color2 = new QPushButton(this);
    m_color2Label = new QLabel(tr("Nach:"), this);
    form->addRow(m_color2Label, m_color2);
    connect(m_color, &QPushButton::clicked, this, [this] {
        if (const std::optional<Color> c = ColorDialog::getColor(this, tr("Linsenfarbe"), m_c1)) {
            m_c1 = *c;
            showColor(m_color, m_c1);
        }
    });
    connect(m_color2, &QPushButton::clicked, this, [this] {
        if (const std::optional<Color> c = ColorDialog::getColor(this, tr("Linsenfarbe"), m_c2)) {
            m_c2 = *c;
            showColor(m_color2, m_c2);
        }
    });
    m_frozen = new QCheckBox(tr("Fixiert"), this);
    m_frozen->setToolTip(tr("Was die Linse gerade zeigt, wird zu eigenen Objekten, die man verschieben kann"));
    layout->addWidget(m_frozen);
    m_viewpoint = new QCheckBox(tr("Blickpunkt"), this);
    m_viewpoint->setToolTip(tr("Die Linse zeigt, was um diesen Punkt liegt, statt was direkt darunter liegt"));
    layout->addWidget(m_viewpoint);
    auto* vp = new QFormLayout;
    m_vx = new ExpressionSpinBox(this);
    m_vy = new ExpressionSpinBox(this);
    for (QDoubleSpinBox* s : {m_vx, m_vy}) {
        s->setRange(-10000, 10000);
        s->setDecimals(2);
        s->setSuffix(QStringLiteral(" mm"));
    }
    vp->addRow(QStringLiteral("x:"), m_vx);
    vp->addRow(QStringLiteral("y:"), m_vy);
    layout->addLayout(vp);
    m_apply = new QPushButton(tr("Anwenden"), this);
    layout->addWidget(m_apply);
    layout->addStretch();

    connect(m_type, &QComboBox::currentIndexChanged, this, [this] { updateFields(); });
    connect(m_viewpoint, &QCheckBox::toggled, this, [this] { updateFields(); });
    connect(m_apply, &QPushButton::clicked, this, &LensDocker::apply);
    connect(m_frozen, &QCheckBox::toggled, this, [this](bool on) {
        if (on && !m_loading)
            apply();
    });
    connect(&m_editor, &Editor::selectionChanged, this, &LensDocker::load);
    load();
}

void LensDocker::setLensType(Lens::Type type)
{
    m_type->setCurrentIndex(m_type->findData(int(type)));
}

Lens LensDocker::lens() const
{
    Lens l;
    l.type = Lens::Type(m_type->currentData().toInt());
    l.amount = l.type == Lens::Type::Magnify ? m_amount->value() * 10.0 : m_amount->value();
    l.color = m_c1;
    l.color2 = m_c2;
    l.bitmapEffect = m_effect->currentData().toString();
    l.viewpoint = m_viewpoint->isChecked();
    if (l.viewpoint) {
        const QList<const DrawObject*> objects = m_editor.selectedObjects();
        const QPointF center = objects.isEmpty() ? QPointF() : objects.first()->boundingRect().center();
        l.viewOffset = QPointF(m_vx->value(), m_vy->value()) - center;
    }
    return l;
}

void LensDocker::load()
{
    const QList<const DrawObject*> objects = m_editor.selectedObjects();
    m_loading = true;
    m_frozen->setChecked(false);
    Lens l = objects.isEmpty() ? Lens() : objects.first()->lens();
    const QPointF center = objects.isEmpty() ? QPointF() : objects.first()->boundingRect().center();
    if (!l.isActive()) {
        // Vorgabe für eine neue Linse; Art bleibt, wie zuletzt gewählt
        l.type = Lens::Type(m_type->currentData().toInt());
    }
    setLensType(l.type);
    updateFields();
    if (l.isActive())
        m_amount->setValue(l.type == Lens::Type::Magnify ? l.amount / 10.0 : l.amount);
    m_c1 = l.color;
    m_c2 = l.color2;
    if (l.isActive())
        m_effect->setCurrentIndex(std::max(0, m_effect->findData(l.bitmapEffect)));
    showColor(m_color, m_c1);
    showColor(m_color2, m_c2);
    m_viewpoint->setChecked(l.viewpoint);
    m_vx->setValue(center.x() + l.viewOffset.x());
    m_vy->setValue(center.y() + l.viewOffset.y());
    m_apply->setEnabled(!objects.isEmpty());
    m_loading = false;
}

void LensDocker::updateFields()
{
    using T = Lens::Type;
    const T t = T(m_type->currentData().toInt());
    const QSignalBlocker block(m_amount);
    const double old = m_amount->value();
    switch (t) {
    case T::Magnify:
        m_amountLabel->setText(tr("Faktor:"));
        m_amount->setRange(1.0, 100.0);
        m_amount->setDecimals(1);
        m_amount->setSuffix(QStringLiteral(" ×"));
        if (old < 1.0 || old > 100.0)
            m_amount->setValue(2.0);
        break;
    case T::FishEye:
        m_amountLabel->setText(tr("Rate:"));
        m_amount->setRange(-1000, 1000);
        m_amount->setDecimals(0);
        m_amount->setSuffix(QStringLiteral(" %"));
        if (old == 0)
            m_amount->setValue(100);
        break;
    case T::HeatMap:
        m_amountLabel->setText(tr("Palettendrehung:"));
        m_amount->setRange(0, 100);
        m_amount->setDecimals(0);
        m_amount->setSuffix(QStringLiteral(" %"));
        break;
    default:
        m_amountLabel->setText(t == T::Transparency ? tr("Deckung:") : tr("Stärke:"));
        m_amount->setRange(t == T::Brighten ? -100 : 0, 100);
        m_amount->setDecimals(0);
        m_amount->setSuffix(QStringLiteral(" %"));
        break;
    }
    const bool hasAmount = t != T::None && t != T::Invert && t != T::TintedGrayscale && t != T::CustomColorMap && t != T::Wireframe;
    m_amount->setVisible(hasAmount);
    m_amountLabel->setVisible(hasAmount);
    m_effect->setVisible(t == T::BitmapEffect);
    m_effectLabel->setVisible(t == T::BitmapEffect);
    const bool color1 = t == T::ColorAdd || t == T::ColorLimit || t == T::CustomColorMap || t == T::TintedGrayscale
                        || t == T::Transparency || t == T::Wireframe;
    m_color->setVisible(color1);
    m_colorLabel->setVisible(color1);
    m_colorLabel->setText(t == T::CustomColorMap ? tr("Von:") : t == T::Wireframe ? tr("Kontur:") : tr("Farbe:"));
    const bool color2 = t == T::CustomColorMap || t == T::Wireframe;
    m_color2->setVisible(color2);
    m_color2Label->setVisible(color2);
    m_color2Label->setText(t == T::Wireframe ? tr("Füllung:") : tr("Nach:"));
    m_frozen->setEnabled(t != T::None && t != T::BitmapEffect);
    m_viewpoint->setEnabled(t != T::None);
    m_vx->setEnabled(m_viewpoint->isChecked() && t != T::None);
    m_vy->setEnabled(m_viewpoint->isChecked() && t != T::None);
}

void LensDocker::apply()
{
    const Lens l = lens();
    m_editor.setSelectionLens(l.isActive() ? l : Lens());
    if (l.isActive() && m_frozen->isChecked()) {
        // Fixiert: jede Linse der Auswahl wird zu Objekten
        QList<NodeId> result;
        for (NodeId id : m_editor.selection())
            if (const NodeId frozen = m_editor.freezeLens(id))
                result << frozen;
        m_editor.setSelection(result);
    }
}

// --- Hülle ------------------------------------------------------------------------------------

EnvelopeDocker::EnvelopeDocker(Editor& editor, Canvas& canvas, QWidget* parent)
    : QWidget(parent), m_editor(editor), m_canvas(canvas)
{
    auto* layout = new QVBoxLayout(this);
    auto* add = new QPushButton(icon(QStringLiteral("env-new")), tr("Neue Hülle"), this);
    add->setToolTip(tr("Rechteckige Hülle (über eine bestehende), danach mit den Griffen verformen"));
    layout->addWidget(add);
    auto* from = new QPushButton(icon(QStringLiteral("env-from")), tr("Hülle aus Objekt …"), this);
    from->setToolTip(tr("Die Form eines anderen Objekts als Hülle verwenden: danach das Objekt anklicken"));
    layout->addWidget(from);
    layout->addWidget(new QLabel(tr("Vorlage wählen:"), this));
    m_presets = new QListWidget(this);
    m_presets->setViewMode(QListView::IconMode);
    m_presets->setIconSize(QSize(48, 36));
    m_presets->setGridSize(QSize(58, 46));
    m_presets->setResizeMode(QListView::Adjust);
    m_presets->setMovement(QListView::Static);
    for (const auto& [name, shape] : EnvelopeTool::presets()) {
        auto* item = new QListWidgetItem(EnvelopeTool::presetIcon(shape), QString(), m_presets);
        item->setToolTip(name);
    }
    m_presets->setMinimumHeight(150);
    layout->addWidget(m_presets, 1);
    auto* modes = new QHBoxLayout;
    m_modes = new QButtonGroup(this);
    const QStringList icons{QStringLiteral("env-straight"), QStringLiteral("env-single"), QStringLiteral("env-double"), QStringLiteral("env-free")};
    const QStringList texts{tr("Modus Gerade"), tr("Modus Einfacher Bogen"), tr("Modus Doppelter Bogen"), tr("Modus Ohne Einschränkung")};
    for (int i = 0; i < 4; ++i) {
        auto* b = new QToolButton(this);
        b->setIcon(icon(icons[i]));
        b->setToolTip(texts[i]);
        b->setCheckable(true);
        b->setAutoRaise(true);
        b->setChecked(i == int(EnvelopeTool::mode()));
        m_modes->addButton(b, i);
        modes->addWidget(b);
    }
    modes->addStretch();
    layout->addLayout(modes);
    auto* mappingRow = new QHBoxLayout;
    mappingRow->addWidget(new QLabel(tr("Zuordnung:"), this));
    m_mapping = new QComboBox(this);
    using M = Envelope::Mapping;
    for (const auto& [m, text] : {std::pair{M::Standard, tr("Frei")}, std::pair{M::Horizontal, tr("Horizontal")},
                                  std::pair{M::Original, tr("Original")}, std::pair{M::Putty, tr("Kitt")},
                                  std::pair{M::Vertical, tr("Vertikal")}})
        m_mapping->addItem(text, int(m));
    m_mapping->setToolTip(tr("Wie das Objekt in die Hülle abgebildet wird"));
    m_mapping->setCurrentIndex(std::max(0, m_mapping->findData(int(mapping()))));
    mappingRow->addWidget(m_mapping, 1);
    layout->addLayout(mappingRow);
    m_addNew = new QCheckBox(tr("Über bestehende Hülle legen"), this);
    layout->addWidget(m_addNew);
    auto* buttons = new QHBoxLayout;
    auto* clear = new QPushButton(tr("Hülle löschen"), this);
    auto* applyButton = new QPushButton(tr("Anwenden"), this);
    buttons->addWidget(clear);
    buttons->addStretch();
    buttons->addWidget(applyButton);
    layout->addLayout(buttons);

    connect(m_modes, &QButtonGroup::idClicked, this, [this](int i) { setMode(m_editor, Envelope::Mode(i)); });
    connect(m_mapping, &QComboBox::activated, this,
            [this] { setMapping(m_editor, Envelope::Mapping(m_mapping->currentData().toInt())); });
    // Ausgewähltes Objekt mit Hülle: dessen Zuordnung zeigen
    connect(&m_editor, &Editor::selectionChanged, this, [this] {
        const QList<const DrawObject*> sel = m_editor.selectedObjects();
        if (sel.size() == 1 && sel.first()->hasEnvelope()) {
            const QSignalBlocker block(m_mapping);
            m_mapping->setCurrentIndex(std::max(0, m_mapping->findData(int(sel.first()->envelopes().last().mapping))));
        }
    });
    connect(add, &QPushButton::clicked, this, [this] {
        if (m_editor.applyEnvelopeShape({}, false, EnvelopeTool::mode(), true, mapping()))
            m_canvas.setTool(ToolId::Envelope);
    });
    connect(from, &QPushButton::clicked, this, &EnvelopeDocker::beginFromObject);
    connect(clear, &QPushButton::clicked, this, [this] { m_editor.clearSelectionEnvelopes(); });
    connect(applyButton, &QPushButton::clicked, this, &EnvelopeDocker::apply);
    connect(m_presets, &QListWidget::itemDoubleClicked, this, [this] { apply(); });
}

void EnvelopeDocker::setMode(Editor& editor, Envelope::Mode mode)
{
    Options::instance().set(QStringLiteral("ws/tools/envelope/mode"), int(mode));
    editor.setSelectionEnvelopeMode(mode);
}

Envelope::Mapping EnvelopeDocker::mapping()
{
    return Envelope::Mapping(std::clamp(Options::instance().integer(QStringLiteral("ws/tools/envelope/mapping")), 0,
                                        int(Envelope::Mapping::Vertical)));
}

void EnvelopeDocker::setMapping(Editor& editor, Envelope::Mapping mapping)
{
    Options::instance().set(QStringLiteral("ws/tools/envelope/mapping"), int(mapping));
    editor.setSelectionEnvelopeMapping(mapping);
}

bool EnvelopeDocker::apply()
{
    const int row = m_presets->currentRow();
    const QList<std::pair<QString, QPainterPath>> presets = EnvelopeTool::presets();
    const QPainterPath shape = row >= 0 && row < presets.size() ? presets[row].second : QPainterPath();
    if (!m_editor.applyEnvelopeShape(shape, false, EnvelopeTool::mode(), m_addNew->isChecked(), mapping()))
        return false;
    m_canvas.setTool(ToolId::Envelope);
    return true;
}

void EnvelopeDocker::beginFromObject()
{
    if (m_editor.selection().isEmpty())
        return;
    m_canvas.setTool(ToolId::Pick);
    m_canvas.pickTool().beginPickTarget([this](NodeId source) {
        const DrawObject* o = m_editor.document().findAs<DrawObject>(source);
        if (o && m_editor.applyEnvelopeShape(o->path(), true, EnvelopeTool::mode(), m_addNew->isChecked(), mapping()))
            m_canvas.setTool(ToolId::Envelope);
    });
}

// --- Effekte (Effektstapel) -------------------------------------------------------------------

namespace {

// Auge offen bzw. durchgestrichen
QIcon eyeIcon(bool open)
{
    QPixmap pm(20, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor c = open ? QColor(0x30, 0x30, 0x30) : QColor(0xa0, 0xa0, 0xa0);
    p.setPen(QPen(c, 1.4));
    QPainterPath lid;
    lid.moveTo(2, 8);
    lid.quadTo(10, 0, 18, 8);
    lid.quadTo(10, 16, 2, 8);
    p.drawPath(lid);
    p.setBrush(c);
    p.drawEllipse(QPointF(10, 8), 2.6, 2.6);
    if (!open)
        p.drawLine(QPointF(3, 14), QPointF(17, 2));
    return QIcon(pm);
}

} // namespace

BitmapEffectsDocker::BitmapEffectsDocker(Editor& editor, Canvas& canvas, QWidget* parent)
    : QWidget(parent), m_editor(editor), m_canvas(canvas)
{
    auto* layout = new QVBoxLayout(this);
    m_info = new QLabel(this);
    m_info->setWordWrap(true);
    layout->addWidget(m_info);
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("bitmapEffects"));
    m_list->setToolTip(tr("Häkchen (Auge): Effekt ein/aus; Doppelklick: Einstellungen"));
    layout->addWidget(m_list, 1);
    auto* row = new QHBoxLayout;
    m_edit = new QPushButton(icon(QStringLiteral("bitmap-edit")), tr("Bearbeiten …"), this);
    m_up = new QPushButton(icon(QStringLiteral("arrow-up")), QString(), this);
    m_up->setToolTip(tr("Nach oben (früher anwenden)"));
    m_down = new QPushButton(icon(QStringLiteral("arrow-down")), QString(), this);
    m_down->setToolTip(tr("Nach unten (später anwenden)"));
    m_remove = new QPushButton(icon(QStringLiteral("edit-delete")), QString(), this);
    m_remove->setToolTip(tr("Effekt löschen"));
    row->addWidget(m_edit, 1);
    row->addWidget(m_up);
    row->addWidget(m_down);
    row->addWidget(m_remove);
    layout->addLayout(row);
    m_apply = new QPushButton(tr("Effekte anwenden"), this);
    m_apply->setObjectName(QStringLiteral("applyBitmapEffects"));
    m_apply->setToolTip(tr("Objekt samt Effekten in ein Bitmap umwandeln"));
    layout->addWidget(m_apply);

    connect(m_list, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        if (!m_loading)
            setEffectEnabled(m_list->row(item), item->checkState() == Qt::Checked);
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) { editEffect(m_list->row(item)); });
    connect(m_list, &QListWidget::currentRowChanged, this, [this] {
        const int r = m_list->currentRow();
        m_edit->setEnabled(r >= 0);
        m_remove->setEnabled(r >= 0);
        m_up->setEnabled(r > 0);
        m_down->setEnabled(r >= 0 && r < m_list->count() - 1);
    });
    connect(m_edit, &QPushButton::clicked, this, [this] { editEffect(m_list->currentRow()); });
    connect(m_up, &QPushButton::clicked, this, [this] { moveEffect(m_list->currentRow(), -1); });
    connect(m_down, &QPushButton::clicked, this, [this] { moveEffect(m_list->currentRow(), 1); });
    connect(m_remove, &QPushButton::clicked, this, [this] { removeEffect(m_list->currentRow()); });
    connect(m_apply, &QPushButton::clicked, this, [this] { applyEffects(); });
    connect(&m_editor, &Editor::selectionChanged, this, &BitmapEffectsDocker::reload);
    connect(&m_editor, &Editor::documentChanged, this, &BitmapEffectsDocker::reload);
    connect(&m_editor, &Editor::documentReplaced, this, &BitmapEffectsDocker::reload);
    reload();
}

const DrawObject* BitmapEffectsDocker::target() const
{
    const QList<const DrawObject*> objects = m_editor.selectedObjects();
    return objects.isEmpty() ? nullptr : objects.first();
}

void BitmapEffectsDocker::reload()
{
    const DrawObject* o = target();
    const int current = m_list->currentRow();
    m_loading = true;
    m_list->clear();
    const QList<BitmapEffect> effects = o ? o->bitmapEffects() : QList<BitmapEffect>();
    for (const BitmapEffect& e : effects) {
        auto* item = new QListWidgetItem(eyeIcon(e.enabled), fx::displayName(e.id), m_list);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        item->setCheckState(e.enabled ? Qt::Checked : Qt::Unchecked);
    }
    m_loading = false;
    m_list->setCurrentRow(std::min(current, int(m_list->count()) - 1));
    if (m_list->currentRow() < 0 && m_list->count() > 0)
        m_list->setCurrentRow(0);
    if (!o)
        m_info->setText(tr("Kein Objekt ausgewählt"));
    else if (effects.isEmpty())
        m_info->setText(tr("Keine Effekte. Bitmap-Effekte aus dem Menü Bitmaps bleiben an Vektorobjekten und Text bearbeitbar."));
    else
        m_info->setText(tr("%n Effekt(e), von oben nach unten angewendet", nullptr, int(effects.size())));
    const int r = m_list->currentRow();
    m_edit->setEnabled(r >= 0);
    m_remove->setEnabled(r >= 0);
    m_up->setEnabled(r > 0);
    m_down->setEnabled(r >= 0 && r < m_list->count() - 1);
    m_apply->setEnabled(o && !effects.isEmpty());
}

void BitmapEffectsDocker::setEffectEnabled(int row, bool on)
{
    const DrawObject* o = target();
    if (!o || row < 0 || row >= o->bitmapEffects().size())
        return;
    QList<BitmapEffect> list = o->bitmapEffects();
    list[row].enabled = on;
    m_editor.setBitmapEffects(o->id(), list, on ? tr("Effekt einschalten") : tr("Effekt ausschalten"));
}

void BitmapEffectsDocker::moveEffect(int row, int delta)
{
    const DrawObject* o = target();
    if (!o)
        return;
    QList<BitmapEffect> list = o->bitmapEffects();
    const int to = row + delta;
    if (row < 0 || row >= list.size() || to < 0 || to >= list.size())
        return;
    list.move(row, to);
    m_editor.setBitmapEffects(o->id(), list, tr("Effektreihenfolge"));
    m_list->setCurrentRow(to);
}

void BitmapEffectsDocker::removeEffect(int row)
{
    const DrawObject* o = target();
    if (!o || row < 0 || row >= o->bitmapEffects().size())
        return;
    QList<BitmapEffect> list = o->bitmapEffects();
    list.removeAt(row);
    m_editor.setBitmapEffects(o->id(), list, tr("Effekt löschen"));
}

QImage BitmapEffectsDocker::dialogImage(const Canvas& canvas, const DrawObject& o, int count, double* referenceWidth)
{
    const QRectF box = o.boundingRect();
    const double side = std::max({box.width(), box.height(), 1e-3});
    const double ref = fx::referencePxPerMm();
    const double r = std::min(ref, 1200.0 / side);
    const Canvas::EffectRaster raster = canvas.rasterizeWithEffects(o, r, count);
    if (referenceWidth)
        *referenceWidth = raster.image.width() * ref / r;
    return raster.image;
}

bool BitmapEffectsDocker::editEffect(int row)
{
    const DrawObject* o = target();
    if (!o || row < 0 || row >= o->bitmapEffects().size())
        return false;
    const NodeId id = o->id();
    BitmapEffect effect = o->bitmapEffects()[row];
    double width = 1.0;
    // Eingang des Effekts: Objekt mit den Effekten davor
    const QImage image = dialogImage(m_canvas, *o, row, &width);
    if (!editBitmapEffect(this, image, width, effect))
        return false;
    const DrawObject* now = m_editor.document().findAs<DrawObject>(id);
    if (!now || row >= now->bitmapEffects().size())
        return false;
    QList<BitmapEffect> list = now->bitmapEffects();
    list[row] = effect;
    m_editor.setBitmapEffects(id, list, fx::displayName(effect.id));
    return true;
}

NodeId BitmapEffectsDocker::applyEffects()
{
    const DrawObject* o = target();
    if (!o || o->bitmapEffects().isEmpty())
        return InvalidNodeId;
    const double dpi = std::max(36, Options::instance().integer(QStringLiteral("doc/page/resolution")));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const Canvas::EffectRaster raster = m_canvas.rasterizeWithEffects(*o, dpi / 25.4);
    QImage pixels = raster.image;
    const double pxPerMm = pixels.width() / std::max(1e-6, raster.rect.width());
    pixels.setDotsPerMeterX(int(std::lround(pxPerMm * 1000)));
    pixels.setDotsPerMeterY(int(std::lround(pxPerMm * 1000)));
    const NodeId id = m_editor.bakeBitmapEffects(o->id(), pixels, raster.rect, tr("Effekte anwenden"));
    QApplication::restoreOverrideCursor();
    return id;
}

} // namespace vek

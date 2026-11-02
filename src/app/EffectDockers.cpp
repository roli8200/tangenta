#include "EffectDockers.h"

#include "Canvas.h"
#include "ColorDialog.h"
#include "Icons.h"
#include "InteractiveTools.h"
#include "Options.h"
#include "PickTool.h"

#include <QButtonGroup>
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
                                  std::pair{T::Transparency, tr("Transparenz")}, std::pair{T::Wireframe, tr("Drahtmodell")}})
        m_type->addItem(text, int(t));
    form->addRow(tr("Art:"), m_type);
    m_amount = new QDoubleSpinBox(this);
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
    m_vx = new QDoubleSpinBox(this);
    m_vy = new QDoubleSpinBox(this);
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
    const bool color1 = t == T::ColorAdd || t == T::ColorLimit || t == T::CustomColorMap || t == T::TintedGrayscale
                        || t == T::Transparency || t == T::Wireframe;
    m_color->setVisible(color1);
    m_colorLabel->setVisible(color1);
    m_colorLabel->setText(t == T::CustomColorMap ? tr("Von:") : t == T::Wireframe ? tr("Kontur:") : tr("Farbe:"));
    const bool color2 = t == T::CustomColorMap || t == T::Wireframe;
    m_color2->setVisible(color2);
    m_color2Label->setVisible(color2);
    m_color2Label->setText(t == T::Wireframe ? tr("Füllung:") : tr("Nach:"));
    m_frozen->setEnabled(t != T::None);
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
    connect(add, &QPushButton::clicked, this, [this] {
        if (m_editor.applyEnvelopeShape({}, false, EnvelopeTool::mode(), true))
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

bool EnvelopeDocker::apply()
{
    const int row = m_presets->currentRow();
    const QList<std::pair<QString, QPainterPath>> presets = EnvelopeTool::presets();
    const QPainterPath shape = row >= 0 && row < presets.size() ? presets[row].second : QPainterPath();
    if (!m_editor.applyEnvelopeShape(shape, false, EnvelopeTool::mode(), m_addNew->isChecked()))
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
        if (o && m_editor.applyEnvelopeShape(o->path(), true, EnvelopeTool::mode(), m_addNew->isChecked()))
            m_canvas.setTool(ToolId::Envelope);
    });
}

} // namespace vek

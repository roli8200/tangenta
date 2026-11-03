#include "OptionsDialog.h"
#include <QDir>
#include <QStandardPaths>

#include "ColorDialog.h"
#include "DimensionObject.h"
#include "Editor.h"
#include "Options.h"
#include "ImagePool.h"
#include "Language.h"
#include "Ai.h"
#include "ClaudeCode.h"
#include "ImageSearch.h"
#include "WebSearch.h"
#include "Labels.h"
#include "RectangleObject.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QFile>
#include <QPainter>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <cmath>

namespace vek {

namespace {

// Erst beim Aufruf übersetzen: eine globale Konstante entstünde vor dem Laden der Übersetzung
QString laterHint()
{
    return QObject::tr("Folgt in einer späteren Version von TANGENTA");
}

struct PageSize {
    const char* name;
    QSizeF size;
};
const PageSize kPageSizes[] = {{"A3", QSizeF(297, 420)}, {"A4", QSizeF(210, 297)}, {"A5", QSizeF(148, 210)},
                               {"A6", QSizeF(105, 148)}, {"Letter", QSizeF(215.9, 279.4)},
                               {"Legal", QSizeF(215.9, 355.6)}};

QString plain(QString text)
{
    text.remove(QLatin1Char('&'));
    const int tab = text.indexOf(QLatin1Char('\t'));
    if (tab >= 0)
        text.truncate(tab);
    return text.trimmed();
}

// Alle Befehle aus den Menüs: Menüname -> Aktionen (mit Untermenüs, ohne Trennlinien)
void collectActions(QMenu* menu, const QString& path, QList<std::pair<QString, QAction*>>& out)
{
    for (QAction* a : menu->actions()) {
        if (a->isSeparator())
            continue;
        if (a->menu())
            collectActions(a->menu(), path + QLatin1Char('/') + plain(a->text()), out);
        else
            out << std::pair{path + QLatin1Char('/') + plain(a->text()), a};
    }
}

} // namespace

OptionsDialog::OptionsDialog(Editor& editor, QMainWindow* window, QWidget* parent)
    : QDialog(parent), m_editor(editor), m_window(window)
{
    setWindowTitle(tr("Optionen"));
    resize(980, 660);
    auto* outer = new QVBoxLayout(this);
    auto* split = new QSplitter(this);
    m_tree = new QTreeWidget(split);
    m_tree->setHeaderHidden(true);
    m_tree->setMinimumWidth(230);
    m_stack = new QStackedWidget(split);
    split->setStretchFactor(1, 1);
    outer->addWidget(split, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help, this);
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Abbrechen"));
    buttons->button(QDialogButtonBox::Help)->setText(tr("Hilfe"));
    connect(buttons, &QDialogButtonBox::accepted, this, &OptionsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &OptionsDialog::reject);
    connect(buttons, &QDialogButtonBox::helpRequested, this, [this] {
        QMessageBox::information(this, tr("Hilfe"),
                                 tr("Die Einstellungen entsprechen dem Dialog Optionen. "
                                    "Ausgegraute Felder gehören zu Funktionen, die TANGENTA noch nicht hat."));
    });
    outer->addWidget(buttons);

    // Hilfslinien als Arbeitskopie: erst die globalen, dann die der Seite
    const int global = int(m_editor.document().masterPage().guides().size());
    const QList<Guide> all = m_editor.guides();
    for (int i = 0; i < all.size(); ++i)
        m_guides << std::pair{all[i], i >= global};

    buildWorkspace();
    buildText();
    buildTools();
    buildCustomization();
    buildDocument();
    buildGlobal();

    m_pageLayout->addStretch(1);
    m_tree->expandAll();
    connect(m_tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
        if (item)
            m_stack->setCurrentIndex(item->data(0, Qt::UserRole).toInt());
    });
    selectPage(tr("Arbeitsbereich"));
}

// --- Bausteine --------------------------------------------------------------------------------

QWidget* OptionsDialog::page(const QString& path, const QString& title)
{
    // Baumeintrag (Eltern werden bei Bedarf angelegt, ohne eigene Seite: zeigen die erste Unterseite)
    // "|" im Namen steht für einen Schrägstrich, der zum Namen gehört ("Freihand|Bezier")
    QStringList parts = path.split(QLatin1Char('/'));
    for (QString& part : parts)
        part.replace(QLatin1Char('|'), QLatin1Char('/'));
    if (m_pageLayout)
        m_pageLayout->addStretch(1); // vorige Seite nach oben schieben
    QTreeWidgetItem* parent = nullptr;
    QString sofar;
    for (int i = 0; i < parts.size(); ++i) {
        sofar += (i ? QStringLiteral("/") : QString()) + parts[i];
        QTreeWidgetItem* item = m_items.value(sofar);
        if (!item) {
            item = parent ? new QTreeWidgetItem(parent, {parts[i]}) : new QTreeWidgetItem(m_tree, {parts[i]});
            item->setData(0, Qt::UserRole, -1);
            m_items.insert(sofar, item);
        }
        parent = item;
    }
    auto* scroll = new QScrollArea(m_stack);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_pageWidget = new QWidget(scroll);
    scroll->setWidget(m_pageWidget);
    m_pageLayout = new QVBoxLayout(m_pageWidget);
    auto* heading = new QLabel(title, m_pageWidget);
    heading->setStyleSheet(QStringLiteral("color: #1f4e9a; font-size: 13pt;"));
    m_pageLayout->addWidget(heading);
    m_layout = m_pageLayout;
    const int index = m_stack->addWidget(scroll);
    parent->setData(0, Qt::UserRole, index);
    // Elternknoten ohne eigene Seite zeigen diese Seite
    for (QTreeWidgetItem* up = parent->parent(); up && up->data(0, Qt::UserRole).toInt() < 0; up = up->parent())
        up->setData(0, Qt::UserRole, index);
    m_laterMark.clear();
    return m_pageWidget;
}

void OptionsDialog::group(const QString& title)
{
    auto* box = new QGroupBox(title, m_pageWidget);
    auto* l = new QVBoxLayout(box);
    m_pageLayout->addWidget(box);
    m_layout = l;
}

void OptionsDialog::endGroup()
{
    m_layout = m_pageLayout;
}

QBoxLayout* OptionsDialog::row()
{
    auto* r = new QHBoxLayout;
    m_layout->addLayout(r);
    return r;
}

QVariant OptionsDialog::current(const QString& key) const
{
    return m_pending.contains(key) ? m_pending.value(key) : Options::instance().value(key);
}

void OptionsDialog::setPending(const QString& key, const QVariant& value)
{
    m_pending.insert(key, value);
}

QCheckBox* OptionsDialog::check(const QString& key, const QString& text)
{
    auto* box = new QCheckBox(text, m_pageWidget);
    box->setChecked(current(key).toBool());
    connect(box, &QCheckBox::toggled, this, [this, key](bool on) { setPending(key, on); });
    m_layout->addWidget(box);
    m_fields.insert(key, box);
    m_laterMark << box;
    return box;
}

QDoubleSpinBox* OptionsDialog::number(const QString& key, const QString& label, double min, double max, int decimals,
                                      const QString& suffix)
{
    QBoxLayout* r = row();
    auto* l = new QLabel(label, m_pageWidget);
    l->setMinimumWidth(170);
    r->addWidget(l);
    auto* spin = new QDoubleSpinBox(m_pageWidget);
    spin->setRange(min, max);
    spin->setDecimals(decimals);
    spin->setSuffix(suffix);
    spin->setValue(current(key).toDouble());
    const bool integer = Options::instance().defaultValue(key).typeId() == QMetaType::Int;
    connect(spin, &QDoubleSpinBox::valueChanged, this, [this, key, integer](double v) {
        setPending(key, integer ? QVariant(int(std::lround(v))) : QVariant(v));
    });
    r->addWidget(spin);
    r->addStretch(1);
    m_fields.insert(key, spin);
    m_laterMark << l << spin;
    return spin;
}

QComboBox* OptionsDialog::choice(const QString& key, const QString& label, const QStringList& items)
{
    QBoxLayout* r = row();
    auto* l = new QLabel(label, m_pageWidget);
    l->setMinimumWidth(170);
    r->addWidget(l);
    auto* combo = new QComboBox(m_pageWidget);
    combo->addItems(items);
    if (!items.isEmpty())
        combo->setCurrentIndex(std::clamp(current(key).toInt(), 0, int(items.size()) - 1));
    connect(combo, &QComboBox::currentIndexChanged, this, [this, key](int i) { setPending(key, i); });
    r->addWidget(combo);
    r->addStretch(1);
    m_fields.insert(key, combo);
    m_laterMark << l << combo;
    return combo;
}

void OptionsDialog::radios(const QString& key, const QString& label, const QStringList& items, bool vertical)
{
    if (!label.isEmpty()) {
        auto* l = new QLabel(label, m_pageWidget);
        m_layout->addWidget(l);
        m_laterMark << l;
    }
    auto* holder = new QWidget(m_pageWidget);
    QBoxLayout* l = vertical ? static_cast<QBoxLayout*>(new QVBoxLayout(holder)) : new QHBoxLayout(holder);
    l->setContentsMargins(12, 0, 0, 0);
    auto* group = new QButtonGroup(holder);
    for (int i = 0; i < items.size(); ++i) {
        auto* b = new QRadioButton(items[i], holder);
        group->addButton(b, i);
        l->addWidget(b);
    }
    if (!vertical)
        l->addStretch(1);
    if (QAbstractButton* b = group->button(current(key).toInt()))
        b->setChecked(true);
    connect(group, &QButtonGroup::idClicked, this, [this, key](int id) { setPending(key, id); });
    m_layout->addWidget(holder);
    m_fields.insert(key, holder);
    m_laterMark << holder;
}

void OptionsDialog::colorField(const QString& key, const QString& label)
{
    QBoxLayout* r = row();
    auto* l = new QLabel(label, m_pageWidget);
    l->setMinimumWidth(170);
    r->addWidget(l);
    auto* b = new QPushButton(m_pageWidget);
    b->setFixedSize(48, 22);
    auto paint = [b](const QColor& c) { b->setStyleSheet(QStringLiteral("background:%1; border:1px solid #555;").arg(c.name())); };
    paint(QColor(current(key).toString()));
    connect(b, &QPushButton::clicked, this, [this, key, paint] {
        const QColor now(current(key).toString());
        if (const std::optional<Color> c = ColorDialog::getColor(this, tr("Farbe"), Color::fromQColor(now))) {
            setPending(key, c->toQColor().name());
            paint(c->toQColor());
        }
    });
    r->addWidget(b);
    r->addStretch(1);
    m_fields.insert(key, b);
    m_laterMark << l << b;
}

QBoxLayout* OptionsDialog::textField(const QString& key, const QString& label)
{
    QBoxLayout* r = row();
    auto* l = new QLabel(label, m_pageWidget);
    l->setMinimumWidth(170);
    r->addWidget(l);
    auto* edit = new QLineEdit(current(key).toString(), m_pageWidget);
    edit->setMaximumWidth(220);
    connect(edit, &QLineEdit::textChanged, this, [this, key](const QString& t) { setPending(key, t); });
    r->addWidget(edit);
    r->addStretch(1);
    m_fields.insert(key, edit);
    m_laterMark << l << edit;
    return r;
}

void OptionsDialog::slider(const QString& key, const QString& label, int min, int max, const QString& unit)
{
    QBoxLayout* r = row();
    auto* l = new QLabel(label, m_pageWidget);
    l->setMinimumWidth(170);
    r->addWidget(l);
    auto* s = new QSlider(Qt::Horizontal, m_pageWidget);
    s->setRange(min, max);
    s->setFixedWidth(160);
    auto* v = new QLabel(m_pageWidget);
    const double factor = Options::instance().defaultValue(key).typeId() == QMetaType::Double && max <= 1000 && min >= 0
                                  && Options::instance().defaultValue(key).toDouble() < 10.0
                              ? 100.0
                              : 1.0; // Sekunden mit zwei Stellen
    s->setValue(int(std::lround(current(key).toDouble() * factor)));
    auto show = [v, factor, unit](int x) { v->setText(QString::number(x / factor, 'f', factor > 1 ? 2 : 0) + unit); };
    show(s->value());
    connect(s, &QSlider::valueChanged, this, [this, key, factor, show](int x) {
        show(x);
        setPending(key, factor > 1 ? QVariant(x / factor) : QVariant(x));
    });
    r->addWidget(s);
    r->addWidget(v);
    r->addStretch(1);
    m_fields.insert(key, s);
    m_laterMark << l << s << v;
}

QListWidget* OptionsDialog::checkList(const QString& key, const QList<std::pair<QString, QString>>& items, bool inverted)
{
    auto* list = new QListWidget(m_pageWidget);
    list->setMaximumHeight(200);
    const QStringList on = current(key).toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const auto& [id, text] : items) {
        auto* item = new QListWidgetItem(text, list);
        item->setData(Qt::UserRole, id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(on.contains(id) != inverted ? Qt::Checked : Qt::Unchecked);
    }
    connect(list, &QListWidget::itemChanged, this, [this, key, list, inverted] {
        QStringList ids;
        for (int i = 0; i < list->count(); ++i) {
            if ((list->item(i)->checkState() == Qt::Checked) != inverted)
                ids << list->item(i)->data(Qt::UserRole).toString();
        }
        setPending(key, ids.join(QLatin1Char(',')));
    });
    m_layout->addWidget(list);
    m_fields.insert(key, list);
    m_laterMark << list;
    return list;
}

void OptionsDialog::note(const QString& text)
{
    auto* l = new QLabel(text, m_pageWidget);
    l->setWordWrap(true);
    l->setStyleSheet(QStringLiteral("color: #444;"));
    m_layout->addWidget(l);
}

QWidget* OptionsDialog::button(const QString& text, std::function<void()> onClick)
{
    auto* b = new QPushButton(text, m_pageWidget);
    if (onClick)
        connect(b, &QPushButton::clicked, this, std::move(onClick));
    QBoxLayout* r = row();
    r->addWidget(b);
    r->addStretch(1);
    m_laterMark << b;
    return b;
}

void OptionsDialog::later(QWidget* w)
{
    w->setEnabled(false);
    w->setToolTip(laterHint());
}

void OptionsDialog::markLaterStart()
{
    m_laterMark.clear();
}

void OptionsDialog::laterRest()
{
    for (QWidget* w : std::as_const(m_laterMark))
        later(w);
    m_laterMark.clear();
}

// --- Arbeitsbereich ---------------------------------------------------------------------------

void OptionsDialog::buildWorkspace()
{
    const QString ws = tr("Arbeitsbereich");
    page(ws + tr("/Allgemein"), tr("Allgemein"));
    group(tr("Erste Schritte"));
    choice(QStringLiteral("ws/general/startup"), tr("Beim Starten von TANGENTA:"),
           {tr("Begrüssungsbildschirm"), tr("Nichts"), tr("Neues Dokument"), tr("Neues Dokument aus Vorlage"),
            tr("Dokument öffnen"), tr("Letztes bearbeitetes Dokument öffnen")});
    check(QStringLiteral("ws/general/newDocDialog"), tr("Dialogfeld 'Neue Dokumente' anzeigen"));
    note(tr("Einen Begrüssungsbildschirm hat TANGENTA nicht; dann startet es mit einem leeren Dokument."));
    endGroup();
    group(tr("Anzahl der Rückgängig-Schritte"));
    number(QStringLiteral("ws/general/undo"), tr("Normal:"), 1, 99999, 0);
    markLaterStart();
    number(QStringLiteral("ws/general/undoBitmap"), tr("Bitmapeffekte:"), 1, 99, 0);
    laterRest();
    endGroup();
    group(tr("Benutzeroberfläche"));
    {
        // Sprache steht in QSettings "ui/language" (wie Ansicht > Sprache), nicht unter options/
        QBoxLayout* r = row();
        auto* l = new QLabel(tr("Sprache (Language):"), m_pageWidget);
        l->setMinimumWidth(170);
        r->addWidget(l);
        auto* combo = new QComboBox(m_pageWidget);
        combo->setObjectName(QStringLiteral("language"));
        combo->addItem(tr("Wie das System"), QString());
        for (const Language& language : languages())
            combo->addItem(language.name, language.code);
        combo->setCurrentIndex(std::max(0, combo->findData(languageSetting())));
        r->addWidget(combo);
        r->addStretch(1);
        m_appliers << [combo] { setLanguageSetting(combo->currentData().toString()); };
    }
    note(tr("Die Sprache gilt ab dem nächsten Start."));
    check(QStringLiteral("ws/general/centerDialogs"), tr("Dialogfelder beim Anzeigen zentrieren"));
    markLaterStart();
    check(QStringLiteral("ws/general/dockerTitles"), tr("Titel in verschiebbaren Andockfenstern anzeigen"));
    check(QStringLiteral("ws/general/autoPopup"), tr("Popup-Menüs mit einem Eintrag automatisch ausführen"));
    check(QStringLiteral("ws/general/sounds"), tr("Klänge aktivieren"));
    laterRest();
    endGroup();

    page(ws + tr("/Anzeige"), tr("Anzeige"));
    markLaterStart();
    check(QStringLiteral("ws/display/proof"), tr("Farben standardmässig überprüfen"));
    group(tr("Sonstige Steuerelemente"));
    check(QStringLiteral("ws/display/interruptRedraw"), tr("Neuaufbau unterbrechen"));
    check(QStringLiteral("ws/display/manualRefresh"), tr("Manuell aktualisieren"));
    check(QStringLiteral("ws/display/offscreen"), tr("Offscreen-Bild verwenden"));
    check(QStringLiteral("ws/display/autopan"), tr("Ausschnitt automatisch verschieben"));
    laterRest();
    check(QStringLiteral("ws/display/tooltips"), tr("Kurzinfos anzeigen"));
    markLaterStart();
    check(QStringLiteral("ws/display/nodeTracking"), tr("Knotenverfolgung anzeigen"));
    check(QStringLiteral("ws/display/postscript"), tr("PostScript-Füllungen in erweiterter Ansicht anzeigen"));
    check(QStringLiteral("ws/display/antialiasBitmaps"), tr("Antialias-Bitmaps in erweiterter Ansicht"));
    check(QStringLiteral("ws/display/hideCurveBox"), tr("Begrenzungsrahmen für Kurven-Hilfsmittel ausblenden"));
    check(QStringLiteral("ws/display/highlightOutline"), tr("Umriss für ausgewählte Objekte markieren"));
    laterRest();
    endGroup();
    group(tr("Ganzseitenvorschau"));
    markLaterStart();
    radios(QStringLiteral("ws/display/fullPreview"), QString(), {tr("Entwurfsansicht"), tr("Erweiterte Ansicht")}, false);
    laterRest();
    check(QStringLiteral("doc/page/showBorder"), tr("Seitenrand anzeigen"));
    endGroup();
    markLaterStart();
    number(QStringLiteral("ws/display/gradientSteps"), tr("Farbverlaufsstreifen in Vorschau:"), 2, 999, 0);
    laterRest();
    choice(QStringLiteral("ws/display/wheel"), tr("Standardaktion für Mausrädchen:"), {tr("Zoom"), tr("Bildlauf")});

    page(ws + tr("/Bearbeiten"), tr("Bearbeiten"));
    group(tr("Allgemein"));
    number(QStringLiteral("ws/edit/angleStep"), tr("Winkelschrittweite:"), 1, 90, 1, tr(" Grad"));
    markLaterStart();
    number(QStringLiteral("ws/edit/precision"), tr("Zeichenpräzision:"), 0, 10, 0, tr(" Dezimalstellen"));
    number(QStringLiteral("ws/edit/extrudeFacet"), tr("Min. Extrusionsfacettengrösse:"), 0.01, 100, 2, QStringLiteral(" mm"));
    choice(QStringLiteral("ws/edit/nodeSize"), tr("Knotengrösse:"), {tr("Klein"), tr("Mittel"), tr("Gross")});
    check(QStringLiteral("ws/edit/saveFacet"), tr("Facettengrösse mit Dokument speichern"));
    laterRest();
    check(QStringLiteral("ws/edit/dblClickBitmap"), tr("Doppelklicken, um Bitmaps zu bearbeiten"));
    markLaterStart();
    check(QStringLiteral("ws/edit/curveDirection"), tr("Kurvenrichtung beim Bearbeiten anzeigen"));
    laterRest();
    endGroup();
    group(tr("SuperClip-Rahmen"));
    radios(QStringLiteral("ws/edit/superclipCenter"), tr("Neuen SuperClip-Rahmeninhalt automatisch zentrieren:"),
           {tr("Wenn Inhalt ganz ausserhalb des Rahmens liegt"), tr("Immer"), tr("Nie")});
    endGroup();

    page(ws + tr("/An Objekten ausrichten"), tr("An Objekten ausrichten"));
    check(QStringLiteral("ws/snap/enabled"), tr("An Objekten ausrichten aktivieren (Alt+Z)"));
    number(QStringLiteral("ws/snap/radius"), tr("Ausrichtungsradius:"), 1, 50, 0, tr(" Pixel"));
    check(QStringLiteral("ws/snap/markers"), tr("Ausrichtungs-Positionsmarkierungen anzeigen"));
    check(QStringLiteral("ws/snap/screentip"), tr("Bildschirmtipp"));
    check(QStringLiteral("ws/snap/page"), tr("An Seite ausrichten"));
    QListWidget* modes = checkList(QStringLiteral("ws/snap/modes"),
                                   {{QStringLiteral("node"), tr("Knoten")}, {QStringLiteral("intersection"), tr("Schnittpunkt")},
                                    {QStringLiteral("midpoint"), tr("Mittelpunkt")}, {QStringLiteral("quadrant"), tr("Quadrant")},
                                    {QStringLiteral("tangent"), tr("Tangente")}, {QStringLiteral("perpendicular"), tr("Senkrecht")},
                                    {QStringLiteral("edge"), tr("Rand")}, {QStringLiteral("center"), tr("Mitte")},
                                    {QStringLiteral("baseline"), tr("Textbasislinie")}});
    for (int i = 0; i < modes->count(); ++i) {
        const QString id = modes->item(i)->data(Qt::UserRole).toString();
        if (id != QLatin1String("node") && id != QLatin1String("edge") && id != QLatin1String("center")) {
            modes->item(i)->setFlags(modes->item(i)->flags() & ~Qt::ItemIsEnabled);
            modes->item(i)->setToolTip(laterHint());
        }
    }
    {
        QBoxLayout* r = row();
        auto* all = new QPushButton(tr("Alle auswählen"), m_pageWidget);
        auto* none = new QPushButton(tr("Auswahl aufheben"), m_pageWidget);
        connect(all, &QPushButton::clicked, this, [modes] {
            for (int i = 0; i < modes->count(); ++i)
                modes->item(i)->setCheckState(Qt::Checked);
        });
        connect(none, &QPushButton::clicked, this, [modes] {
            for (int i = 0; i < modes->count(); ++i)
                modes->item(i)->setCheckState(Qt::Unchecked);
        });
        r->addWidget(all);
        r->addWidget(none);
        r->addStretch(1);
    }

    page(ws + tr("/Dynamische Hilfslinien"), tr("Dynamische Hilfslinien"));
    check(QStringLiteral("ws/dynguides/enabled"), tr("Dynamische Hilfslinien aktivieren (Alt+Umschalt+D)"));
    check(QStringLiteral("ws/dynguides/snapToMarks"), tr("An Ausrichtungsmarken ausrichten"));
    check(QStringLiteral("ws/dynguides/angleTip"), tr("Winkel-Bildschirmtipp"));
    number(QStringLiteral("ws/dynguides/distance"), tr("Abstand zwischen Ausrichtungsmarken:"), 0.1, 100, 1, QStringLiteral(" mm"));
    check(QStringLiteral("ws/dynguides/distanceTip"), tr("Abstand-Bildschirmtipp"));
    {
        QList<std::pair<QString, QString>> angles;
        for (const QString& a : current(QStringLiteral("ws/dynguides/angleList")).toString().split(QLatin1Char(',')))
            angles << std::pair{a, a + QStringLiteral("°")};
        checkList(QStringLiteral("ws/dynguides/angles"), angles);
    }
    markLaterStart();
    check(QStringLiteral("ws/dynguides/extendSegment"), tr("An Segment verlängern"));
    laterRest();
    note(tr("Dynamische Hilfslinien sind temporäre Hilfslinien, die aus den Ausrichtungspunkten von Objekten "
            "gezogen werden können."));

    page(ws + tr("/Ausrichtungshilfslinien"), tr("Ausrichtungshilfslinien"));
    check(QStringLiteral("ws/alignguides/enabled"), tr("Ausrichtungshilfslinien auf (Alt+Umschalt+A)"));
    colorField(QStringLiteral("ws/alignguides/color"), tr("Farbe:"));
    radios(QStringLiteral("ws/alignguides/scope"), tr("Ausrichtungshilfslinien anzeigen für:"),
           {tr("Begrenzungsrahmen einer Gruppe"), tr("Einzelne Objekte einer Gruppe")});
    check(QStringLiteral("ws/alignguides/centerEdges"), tr("Kanten am Mittelpunkt ausrichten"));
    check(QStringLiteral("ws/alignguides/useMargins"), tr("Ränder als Ausrichtungshilfslinien verwenden"));
    number(QStringLiteral("ws/alignguides/margin"), tr("Rand:"), 0, 100, 1, QStringLiteral(" mm"));
    radios(QStringLiteral("ws/alignguides/marginScope"), tr("Anzeigen für:"), {tr("Nur Ränder"), tr("Ränder und Objekt")});

    page(ws + tr("/Warnungen"), tr("Warnungen"));
    note(tr("Warnungen anzeigen bei:"));
    markLaterStart();
    checkList(QStringLiteral("ws/warnings/off"),
              {{QStringLiteral("spot"), tr("Anwenden von Transparenz auf Schmuckfarbe")},
               {QStringLiteral("pattern"), tr("Füllmuster nicht im RGB-Modus")},
               {QStringLiteral("duotone"), tr("Duoton-Bitmaps")},
               {QStringLiteral("lens"), tr("Anwenden von Linseneffekten")},
               {QStringLiteral("distort"), tr("Zu komplexe Verzerrung")},
               {QStringLiteral("mask"), tr("Weiche Maske")},
               {QStringLiteral("textfx"), tr("Effekte auf Text mit Grafiken")},
               {QStringLiteral("paste"), tr("Text einfügen und importieren")},
               {QStringLiteral("spreads"), tr("Doppelseiten importieren")},
               {QStringLiteral("singlepage"), tr("Zieldateiformat unterstützt nur einfache Seiten")},
               {QStringLiteral("guides"), tr("Ändern vorhandener Hilfslinien")}},
              true);
    laterRest();

    page(ws + tr("/Skripte"), tr("Skripte (JavaScript)"));
    textField(QStringLiteral("ws/scripts/folder"), tr("Skriptordner:"));
    note(tr("Leer = %1").arg(QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                                                            + QStringLiteral("/Skripte"))));
    number(QStringLiteral("ws/scripts/timeout"), tr("Skript abbrechen nach:"), 1, 3600, 0, tr(" Sekunden"));
    note(tr("Skripte sind JavaScript (ECMAScript 7) und arbeiten über das Objekt 'tg' mit dem Dokument. Jeder "
            "Lauf ist ein Rückgängig-Schritt; bei einem Fehler bleibt das Dokument unverändert. Andockfenster "
            "'Skripte' bzw. Extras > Skripte. Skripte haben keinen Zugriff auf Dateien oder Netz."));

    page(ws + tr("/Speichern"), tr("Speichern"));
    group(tr("Automatische Sicherungskopie"));
    check(QStringLiteral("ws/save/autosave"), tr("Automatische Sicherung"));
    choice(QStringLiteral("ws/save/autosaveMinutes"), tr("alle (Minuten):"), {});
    if (auto* combo = qobject_cast<QComboBox*>(m_fields.value(QStringLiteral("ws/save/autosaveMinutes")))) {
        const QSignalBlocker block(combo);
        const QList<int> minutes{1, 2, 5, 10, 15, 20, 30, 45, 60};
        for (int m : minutes)
            combo->addItem(QString::number(m), m);
        combo->setCurrentIndex(std::max(0, combo->findData(current(QStringLiteral("ws/save/autosaveMinutes")).toInt())));
        disconnect(combo, &QComboBox::currentIndexChanged, this, nullptr);
        connect(combo, &QComboBox::currentIndexChanged, this,
                [this, combo] { setPending(QStringLiteral("ws/save/autosaveMinutes"), combo->currentData().toInt()); });
    }
    radios(QStringLiteral("ws/save/folderMode"), tr("Standardordner:"), {tr("Temporärer Benutzerordner"), tr("Bestimmter Ordner")});
    textField(QStringLiteral("ws/save/folder"), tr("Ordner:"));
    button(tr("Durchsuchen …"), [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Ordner für Sicherungskopien"));
        if (dir.isEmpty())
            return;
        if (auto* edit = qobject_cast<QLineEdit*>(m_fields.value(QStringLiteral("ws/save/folder"))))
            edit->setText(dir);
        setPending(QStringLiteral("ws/save/folderMode"), 1);
        if (auto* holder = m_fields.value(QStringLiteral("ws/save/folderMode"))) {
            const QList<QRadioButton*> buttons = holder->findChildren<QRadioButton*>();
            if (buttons.size() > 1)
                buttons[1]->setChecked(true);
        }
    });
    endGroup();
    group(tr("Text in vorheriger Version speichern"));
    markLaterStart();
    radios(QStringLiteral("ws/save/textMode"), QString(), {tr("Textdarstellung beibehalten"), tr("Text bearbeitbar lassen")});
    check(QStringLiteral("ws/save/askBeforeSave"), tr("Vor dem Speichern fragen"));
    laterRest();
    endGroup();
    check(QStringLiteral("ws/save/backupOnSave"), tr("Sicherungskopie beim Speichern erstellen"));

    page(ws + tr("/SuperTrace"), tr("SuperTrace"));
    choice(QStringLiteral("ws/trace/quick"), tr("Blitzvektorisierung:"),
           {tr("Zuletzt benutzt"), tr("Strichgrafik"), tr("Logo"), tr("Detailliertes Logo"), tr("ClipArt"),
            tr("Bild niedriger Qualität"), tr("Bild hoher Qualität")});
    slider(QStringLiteral("ws/trace/performance"), tr("Leistung (bessere Leistung … höhere Qualität):"), 0, 100, QString());
    radios(QStringLiteral("ws/trace/mergeMode"), tr("Farben mischen:"),
           {tr("Durchschnittliche Farbmischung"), tr("Mit zuerst ausgewählter Farbe mischen")});

    page(ws + tr("/Plugins"), tr("Plugins"));
    note(tr("Plugin-Ordner:"));
    markLaterStart();
    auto* plugins = new QListWidget(m_pageWidget);
    plugins->setMaximumHeight(160);
    m_layout->addWidget(plugins);
    m_laterMark << plugins;
    button(tr("Hinzufügen …"), {});
    button(tr("Entfernen"), {});
    laterRest();
}

void OptionsDialog::buildText()
{
    const QString base = tr("Arbeitsbereich/Text");
    page(base + tr("/Mengentextrahmen"), tr("Mengentextrahmen"));
    check(QStringLiteral("ws/text/frames/showLinks"), tr("Textrahmenverknüpfung anzeigen"));
    check(QStringLiteral("ws/text/frames/showFrames"), tr("Textrahmen anzeigen"));
    markLaterStart();
    check(QStringLiteral("ws/text/frames/autoFit"), tr("Mengentextrahmen erweitern und reduzieren, um Text einzupassen"));
    radios(QStringLiteral("ws/text/frames/formatScope"), tr("Textrahmenformatierung bei der Bearbeitung anwenden auf:"),
           {tr("alle verknüpften Rahmen"), tr("nur ausgewählte Rahmen"), tr("ausgewählte und folgende Rahmen")});
    check(QStringLiteral("ws/text/frames/webCompatible"), tr("Alle neuen Mengentextrahmen Web-kompatibel machen"));
    laterRest();

    page(base + tr("/Schriften"), tr("Schriften"));
    group(tr("Schriftlisteninhalt"));
    check(QStringLiteral("ws/text/fonts/truetype"), tr("TrueType-Schriften anzeigen"));
    check(QStringLiteral("ws/text/fonts/ttSymbols"), tr("TrueType-Symbole anzeigen"));
    markLaterStart();
    check(QStringLiteral("ws/text/fonts/type1"), tr("Type 1-Schriften anzeigen"));
    check(QStringLiteral("ws/text/fonts/t1Symbols"), tr("Type 1-Symbole anzeigen"));
    laterRest();
    check(QStringLiteral("ws/text/fonts/docFonts"), tr("Nur Dokumentschriften anzeigen"));
    endGroup();
    markLaterStart();
    check(QStringLiteral("ws/text/fonts/sampleFlyout"), tr("Flyout mit Schriftbeispiel neben der Schriftliste"));
    check(QStringLiteral("ws/text/fonts/styleFlyout"), tr("Flyout mit verfügbaren Schriftstilen"));
    laterRest();
    check(QStringLiteral("ws/text/fonts/previewName"), tr("Schrift für die Anzeige des Schriftnamens verwenden"));
    number(QStringLiteral("ws/text/fonts/recent"), tr("Anzahl kürzlich verwendeter Schriften:"), 0, 20, 0);
    markLaterStart();
    radios(QStringLiteral("ws/text/fonts/matching"), tr("Schriftvergleich:"),
           {tr("Schriftkatalog nicht verwenden"), tr("Schriftkatalog beim Öffnen verwenden")});
    button(tr("Panose-Schriftabgleichung …"), {});
    laterRest();

    page(base + tr("/Rechtschreibprüfung"), tr("Rechtschreibprüfung"));
    check(QStringLiteral("ws/text/spell/auto"), tr("Automatische Rechtschreibprüfung durchführen"));
    radios(QStringLiteral("ws/text/spell/visibility"), tr("Sichtbarkeit von Fehlern:"),
           {tr("Fehler in allen Textrahmen anzeigen"), tr("Fehler nur im ausgewählten Textrahmen anzeigen")});
    number(QStringLiteral("ws/text/spell/suggestions"), tr("Anzeigen (Vorschläge für Schreibweise):"), 1, 50, 0);
    check(QStringLiteral("ws/text/spell/addToAutocorrect"), tr("Korrekturen der Blitzkorrektur hinzufügen"));
    check(QStringLiteral("ws/text/spell/showIgnored"), tr("Ignorierte Fehler anzeigen"));
    note(tr("Wörterbuch nach der Sprache unter Text > Blitzkorrektur (Deutsch Schweiz/Deutschland, Englisch, "
            "Französisch). Rechtsklick auf ein rot unterstrichenes Wort zeigt Vorschläge; Text > "
            "Rechtschreibprüfung (Strg+F12) prüft das ganze Dokument."));

    page(base + tr("/Blitzkorrektur"), tr("Blitzkorrektur"));
    check(QStringLiteral("ws/text/autocorrect/capSentence"), tr("Ersten Buchstaben von Sätzen grossschreiben"));
    check(QStringLiteral("ws/text/autocorrect/twoCaps"), tr("Zwei aufeinander folgende Anfangsgrossbuchstaben korrigieren"));
    check(QStringLiteral("ws/text/autocorrect/dayNames"), tr("Tagesnamen grossschreiben"));
    markLaterStart();
    check(QStringLiteral("ws/text/autocorrect/hyperlink"), tr("Automatisch Hyperlink setzen"));
    laterRest();
    choice(QStringLiteral("ws/text/autocorrect/language"), tr("Sprache:"),
           {tr("Deutsch (Schweiz)"), tr("Deutsch (Deutschland)"), tr("Englisch"), tr("Französisch")});
    check(QStringLiteral("ws/text/autocorrect/typoQuotes"), tr("Gerade in typografische Anführungszeichen ändern"));
    check(QStringLiteral("ws/text/autocorrect/numbersStraight"), tr("Nach Zahlen gerade Anführungszeichen verwenden"));
    textField(QStringLiteral("ws/text/autocorrect/singleOpen"), tr("Einfache: Öffnen"));
    textField(QStringLiteral("ws/text/autocorrect/singleClose"), tr("Einfache: Schliessen"));
    textField(QStringLiteral("ws/text/autocorrect/doubleOpen"), tr("Doppelte: Öffnen"));
    textField(QStringLiteral("ws/text/autocorrect/doubleClose"), tr("Doppelte: Schliessen"));
    check(QStringLiteral("ws/text/autocorrect/replace"), tr("Text bei der Eingabe ersetzen"));
    note(tr("Ersatztext (Ersetzen → Durch), je Zeile \"alt=neu\":"));
    textField(QStringLiteral("ws/text/autocorrect/replacements"), tr("Ersatztext:"));
    note(tr("Beispiel: \"zb=z. B.;mfg=Mit freundlichen Grüssen\" (Strichpunkt oder neue Zeile trennt die Einträge). "
            "Hyperlinks folgen, sobald Objekte Verknüpfungen tragen können."));
}

void OptionsDialog::buildTools()
{
    const QString base = tr("Arbeitsbereich/Hilfsmittelpalette");
    const QString hint = tr("Hinsichtlich der Text- und Umrisseigenschaften von Bemassungen werden die Standardwerte des "
                            "Textwerkzeugs für Grafiktext sowie die Standardwerte für Umrisse herangezogen.");
    page(base + tr("/Bemassung"), tr("Bemassung"));
    choice(QStringLiteral("ws/tools/dimension/style"), tr("Stil:"), {tr("Dezimal"), tr("Bruch")});
    choice(QStringLiteral("ws/tools/dimension/precision"), tr("Präzision:"), {QStringLiteral("0"), QStringLiteral("0.0"), QStringLiteral("0.00"), QStringLiteral("0.000")});
    choice(QStringLiteral("ws/tools/dimension/units"), tr("Einheiten:"), DimensionObject::lengthUnits());
    textField(QStringLiteral("ws/tools/dimension/prefix"), tr("Präfix:"));
    textField(QStringLiteral("ws/tools/dimension/suffix"), tr("Suffix:"));
    note(hint);

    page(base + tr("/Formerkennung"), tr("Formerkennung"));
    slider(QStringLiteral("ws/tools/shapeRecognition/delay"), tr("Verzögerung:"), 10, 1000, tr(" Sekunde(n)"));
    note(tr("Gilt für das Smart-Zeichnen (Umschalt+S): so lange nach dem Loslassen kann man weiterzeichnen, bevor die Form erkannt wird."));

    page(base + QLatin1Char('/') + tr("Freihand/Bezier").replace(QLatin1Char('/'), QLatin1Char('|')), tr("Freihand/Bezier"));
    slider(QStringLiteral("ws/tools/freehand/smoothing"), tr("Freihandglättung:"), 0, 100, QString());
    number(QStringLiteral("ws/tools/freehand/corner"), tr("Eckengrenzwert:"), 1, 10, 0, tr(" Pixel"));
    number(QStringLiteral("ws/tools/freehand/straight"), tr("Geradengrenzwert:"), 1, 10, 0, tr(" Pixel"));
    number(QStringLiteral("ws/tools/freehand/autoJoin"), tr("Autom. verbinden:"), 0, 20, 0, tr(" Pixel"));

    page(base + tr("/Hilfsmittel 'Auswahl'"), tr("Hilfsmittel 'Auswahl'"));
    check(QStringLiteral("ws/tools/pick/crosshair"), tr("Fadenkreuz"));
    check(QStringLiteral("ws/tools/pick/treatFilled"), tr("Alle Objekte als gefüllt betrachten"));
    group(tr("Verschieben und ändern"));
    check(QStringLiteral("ws/tools/pick/redrawComplex"), tr("Komplexe Objekte neu zeichnen"));
    number(QStringLiteral("ws/tools/pick/delay"), tr("Verzögerung:"), 0, 10, 1, tr(" Sekunden"));
    endGroup();
    group(tr("Strg- und Umschalttaste"));
    radios(QStringLiteral("ws/tools/pick/modifiers"), QString(),
           {tr("Klassisch: Strg = Beschränkung, Umschalt = von der Mitte aus ändern"),
            tr("Windows-Standard: Strg = Original duplizieren/beibehalten, Umschalt = Beschränkung")});
    endGroup();

    page(base + tr("/Hilfsmittel 'Ellipse'"), tr("Hilfsmittel 'Ellipse'"));
    radios(QStringLiteral("ws/tools/ellipse/kind"), QString(), {tr("Ellipse"), tr("Kreissektor"), tr("Bogen")}, false);
    number(QStringLiteral("ws/tools/ellipse/start"), tr("Startwinkel:"), -360, 360, 1, tr(" Grad"));
    number(QStringLiteral("ws/tools/ellipse/end"), tr("Endwinkel:"), -360, 360, 1, tr(" Grad"));
    radios(QStringLiteral("ws/tools/ellipse/clockwise"), QString(), {tr("Linksläufig"), tr("Rechtsläufig")}, false);

    page(base + tr("/Hilfsmittel 'Radierer'"), tr("Hilfsmittel 'Radierer'"));
    number(QStringLiteral("ws/tools/eraser/width"), tr("Dicke:"), 0.01, 100, 2, QStringLiteral(" mm"));
    check(QStringLiteral("ws/tools/eraser/reduceNodes"), tr("Knoten der resultierenden Objekte automatisch reduzieren"));

    page(base + tr("/Hilfsmittel 'Rechteck'"), tr("Hilfsmittel 'Rechteck'"));
    radios(QStringLiteral("ws/tools/rect/cornerType"), tr("Eckentyp:"), {tr("Abgerundet"), tr("Gewellt"), tr("Abgeschrägt")}, false);
    number(QStringLiteral("ws/tools/rect/radius"), tr("Rechteck-Ecken:"), 0, 1000, 1, QStringLiteral(" mm"));
    check(QStringLiteral("ws/tools/rect/scaleCorners"), tr("Ecken skalieren"));

    page(base + tr("/Hilfsmittel 'Tabelle'"), tr("Hilfsmittel 'Tabelle'"));
    number(QStringLiteral("ws/tools/table/rows"), tr("Zeilen:"), 1, 99, 0);
    number(QStringLiteral("ws/tools/table/cols"), tr("Spalten:"), 1, 99, 0);
    radios(QStringLiteral("ws/tools/table/tabMode"), tr("Beim Bearbeiten einer Tabelle kann man mithilfe der Tabulatortaste:"),
           {tr("Tabulatorzeichen in den Text einfügen"), tr("Zur nächsten Zelle springen")});
    choice(QStringLiteral("ws/tools/table/tabOrder"), tr("Tabulatorreihenfolge:"),
           {tr("Von links nach rechts, von oben nach unten"), tr("Von rechts nach links, von oben nach unten")});

    page(base + tr("/Hilfsmittel 'Verbindung'"), tr("Hilfsmittel 'Verbindung'"));
    check(QStringLiteral("ws/tools/connector/anchors"), tr("Geometrische Ankerpunkte als Ausrichtungspunkte verwenden"));
    number(QStringLiteral("ws/tools/connector/distance"), tr("Abstand der Linien zu Objekten:"), 0, 100, 1, QStringLiteral(" mm"));

    page(base + tr("/Maschenfüllung"), tr("Maschenfüllung"));
    number(QStringLiteral("ws/tools/mesh/cols"), tr("Spaltenanzahl:"), 1, 50, 0);
    number(QStringLiteral("ws/tools/mesh/rows"), tr("Anzahl Reihen:"), 1, 50, 0);

    page(base + tr("/Messer"), tr("Messer"));
    check(QStringLiteral("ws/tools/knife/keepOne"), tr("Als ein Objekt belassen"));
    check(QStringLiteral("ws/tools/knife/autoClose"), tr("Objekt automatisch schliessen"));

    page(base + tr("/Millimeterpapier"), tr("Millimeterpapier"));
    number(QStringLiteral("ws/tools/graph/cols"), tr("Zellenanzahl (Breite):"), 1, 99, 0);
    number(QStringLiteral("ws/tools/graph/rows"), tr("Zellenanzahl (Höhe):"), 1, 99, 0);

    page(base + tr("/Spirale"), tr("Spirale"));
    radios(QStringLiteral("ws/tools/spiral/type"), QString(), {tr("Symmetrisch"), tr("Logarithmisch")});
    number(QStringLiteral("ws/tools/spiral/revolutions"), tr("Anzahl der Umdrehungen:"), 1, 100, 0);
    slider(QStringLiteral("ws/tools/spiral/expansion"), tr("Ausdehnung:"), 1, 100, QString());

    page(base + tr("/Winkelmasse"), tr("Winkelmasse"));
    choice(QStringLiteral("ws/tools/angle/precision"), tr("Präzision:"), {QStringLiteral("0"), QStringLiteral("0.0"), QStringLiteral("0.00"), QStringLiteral("0.000")});
    choice(QStringLiteral("ws/tools/angle/units"), tr("Einheiten:"), {QStringLiteral("°"), tr("Radiant"), tr("Gon")});
    textField(QStringLiteral("ws/tools/angle/prefix"), tr("Präfix:"));
    textField(QStringLiteral("ws/tools/angle/suffix"), tr("Suffix:"));
    note(hint);

    page(base + QLatin1Char('/') + tr("Zoom/Schwenken").replace(QLatin1Char('/'), QLatin1Char('|')), tr("Zoom/Schwenken"));
    radios(QStringLiteral("ws/tools/zoom/rightZoom"), tr("Rechte Maustaste für Zoom:"), {tr("Verkleinern"), tr("Kontextmenü")});
    radios(QStringLiteral("ws/tools/zoom/rightPan"), tr("Rechte Maustaste für das Hilfsmittel 'Schwenken':"),
           {tr("Verkleinern"), tr("Kontextmenü")});
    check(QStringLiteral("ws/tools/zoom/relative"), tr("Zoom relativ zu 1:1"));
    check(QStringLiteral("ws/tools/zoom/wheelCenter"), tr("Beim Zoomen mit dem Mausrad Maus zentrieren"));
    button(tr("Lineale kalibrieren …"), [this] {
        // Ein Lineal am Bildschirm an ein echtes Lineal anpassen
        QDialog d(this);
        d.setWindowTitle(tr("Lineale kalibrieren"));
        auto* l = new QVBoxLayout(&d);
        auto* ruler = new QLabel(&d);
        auto* factor = new QDoubleSpinBox(&d);
        factor->setRange(10, 400);
        factor->setDecimals(1);
        factor->setSuffix(QStringLiteral(" %"));
        factor->setValue(current(QStringLiteral("ws/tools/zoom/calibration")).toDouble() * 100.0);
        auto draw = [=, &d] {
            const double pxPerMm = factor->value() / 100.0 * d.logicalDpiX() / 25.4;
            QPixmap pix(int(110 * pxPerMm) + 20, 46);
            pix.fill(Qt::white);
            QPainter p(&pix);
            p.setPen(Qt::black);
            for (int mm = 0; mm <= 100; ++mm) {
                const double x = 10 + mm * pxPerMm;
                p.drawLine(QPointF(x, 0), QPointF(x, mm % 10 == 0 ? 22 : mm % 5 == 0 ? 14 : 8));
                if (mm % 10 == 0)
                    p.drawText(QPointF(x - 6, 38), QString::number(mm / 10));
            }
            p.end();
            ruler->setPixmap(pix);
        };
        l->addWidget(new QLabel(tr("Halten Sie ein Lineal an den Bildschirm und stellen Sie die Grösse so ein, dass "
                                   "die Zentimeter übereinstimmen:"), &d));
        l->addWidget(ruler);
        l->addWidget(factor);
        connect(factor, &QDoubleSpinBox::valueChanged, &d, draw);
        draw();
        auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &d);
        box->button(QDialogButtonBox::Cancel)->setText(tr("Abbrechen"));
        connect(box, &QDialogButtonBox::accepted, &d, &QDialog::accept);
        connect(box, &QDialogButtonBox::rejected, &d, &QDialog::reject);
        l->addWidget(box);
        if (d.exec() == QDialog::Accepted)
            setPending(QStringLiteral("ws/tools/zoom/calibration"), factor->value() / 100.0);
    });
}

void OptionsDialog::buildCustomization()
{
    const QString base = tr("Arbeitsbereich/Anpassung");
    page(base + tr("/Befehlsleisten"), tr("Befehlsleisten"));
    check(QStringLiteral("ws/custom/bar/menu"), tr("Menüleiste"));
    check(QStringLiteral("ws/custom/bar/status"), tr("Statusleiste"));
    check(QStringLiteral("ws/custom/bar/standard"), tr("Standard"));
    check(QStringLiteral("ws/custom/bar/property"), tr("Eigenschaftsleiste"));
    check(QStringLiteral("ws/custom/bar/toolbox"), tr("Hilfsmittelpalette"));
    check(QStringLiteral("ws/custom/bar/rulers"), tr("Lineale"));
    group(tr("Grösse"));
    markLaterStart();
    choice(QStringLiteral("ws/custom/buttonSize"), tr("Schaltfläche:"), {tr("1 - Gross"), tr("2 - Mittel"), tr("3 - Klein")});
    number(QStringLiteral("ws/custom/buttonLimit"), tr("Grenze:"), 0, 10, 0);
    choice(QStringLiteral("ws/custom/buttonFormat"), tr("Standardformat für Schaltflächen:"),
           {tr("Standard"), tr("Nur Symbol"), tr("Symbol und Text"), tr("Nur Text")});
    check(QStringLiteral("ws/custom/barTitles"), tr("Titel in verschiebbaren Symbolleisten anzeigen"));
    check(QStringLiteral("ws/custom/lockBars"), tr("Symbolleisten fixieren"));
    choice(QStringLiteral("ws/custom/menuMode"), tr("Menüleistenmodus:"), {tr("Normaler Modus")});
    laterRest();
    endGroup();

    page(base + tr("/Befehle"), tr("Befehle"));
    m_layout->addWidget(buildCommandsPage());

    page(base + tr("/Farbpalette"), tr("Farbpalette"));
    markLaterStart();
    check(QStringLiteral("ws/custom/palette/autoUpdate"), tr("Dokumentpalette automatisch aktualisieren"));
    number(QStringLiteral("ws/custom/palette/rows"), tr("Höchstzahl der Palettenzeilen im angedockten Zustand:"), 1, 10, 0);
    check(QStringLiteral("ws/custom/palette/wideBorders"), tr("Breite Ränder"));
    check(QStringLiteral("ws/custom/palette/largeSwatches"), tr("Grosse Muster"));
    check(QStringLiteral("ws/custom/palette/noColor"), tr("Farbkästchen für 'Keine Farbe' anzeigen"));
    radios(QStringLiteral("ws/custom/palette/rightClick"), tr("Rechte Maustaste:"), {tr("Kontextmenü"), tr("Umrissfarbe einstellen")});
    laterRest();

    page(base + tr("/Anwendung"), tr("Anwendung"));
    markLaterStart();
    check(QStringLiteral("ws/custom/app/transparent"), tr("Benutzeroberfläche transparent machen"));
    check(QStringLiteral("ws/custom/app/bars"), tr("Befehlsleisten"));
    check(QStringLiteral("ws/custom/app/dockers"), tr("Andockfenster"));
    check(QStringLiteral("ws/custom/app/colorUi"), tr("Benutzeroberfläche mit Farbinformationen"));
    slider(QStringLiteral("ws/custom/app/degree"), tr("Grad der Transparenz:"), 0, 100, QStringLiteral(" %"));
    laterRest();
}

QWidget* OptionsDialog::buildCommandsPage()
{
    // Befehle aus allen Menüs, Tastenkürzel ändern (gespeichert als JSON Pfad -> Kürzel)
    auto* holder = new QWidget(m_pageWidget);
    auto* layout = new QHBoxLayout(holder);
    auto* left = new QVBoxLayout;
    auto* categories = new QComboBox(holder);
    auto* list = new QListWidget(holder);
    list->setMinimumWidth(240);
    left->addWidget(categories);
    left->addWidget(list);
    layout->addLayout(left);
    auto* tabs = new QTabWidget(holder);
    layout->addWidget(tabs, 1);

    QList<std::pair<QString, QAction*>> actions;
    if (m_window) {
        for (QAction* top : m_window->menuBar()->actions()) {
            if (top->menu()) {
                categories->addItem(plain(top->text()));
                collectActions(top->menu(), plain(top->text()), actions);
            }
        }
    }
    auto shortcuts = std::make_shared<QJsonObject>(
        QJsonDocument::fromJson(current(QStringLiteral("ws/custom/shortcuts")).toString().toUtf8()).object());

    // Allgemein
    auto* general = new QWidget(tabs);
    auto* gl = new QVBoxLayout(general);
    gl->addWidget(new QLabel(tr("Tooltip-Hilfe:"), general));
    auto* tip = new QLabel(general);
    tip->setWordWrap(true);
    tip->setFrameShape(QFrame::StyledPanel);
    gl->addWidget(tip);
    gl->addWidget(new QLabel(tr("Derzeit verfügbar für:"), general));
    auto* where = new QLabel(general);
    where->setFrameShape(QFrame::StyledPanel);
    gl->addWidget(where);
    gl->addStretch(1);
    tabs->addTab(general, tr("Allgemein"));

    // Tastaturbefehle
    auto* keys = new QWidget(tabs);
    auto* kl = new QVBoxLayout(keys);
    kl->addWidget(new QLabel(tr("Neuer Tastaturbefehl:"), keys));
    auto* seqRow = new QHBoxLayout;
    auto* sequence = new QKeySequenceEdit(keys);
    auto* assign = new QPushButton(tr("Zuweisen"), keys);
    seqRow->addWidget(sequence);
    seqRow->addWidget(assign);
    kl->addLayout(seqRow);
    kl->addWidget(new QLabel(tr("Zurzeit zugewiesen zu:"), keys));
    auto* conflict = new QLabel(keys);
    conflict->setFrameShape(QFrame::StyledPanel);
    kl->addWidget(conflict);
    kl->addWidget(new QLabel(tr("Aktuelle Tastaturbefehle:"), keys));
    auto* currentKeys = new QListWidget(keys);
    currentKeys->setMaximumHeight(80);
    kl->addWidget(currentKeys);
    auto* keyButtons = new QHBoxLayout;
    auto* remove = new QPushButton(tr("Löschen"), keys);
    auto* resetAll = new QPushButton(tr("Alle zurücksetzen"), keys);
    keyButtons->addWidget(remove);
    keyButtons->addWidget(resetAll);
    keyButtons->addStretch(1);
    kl->addLayout(keyButtons);
    kl->addStretch(1);
    tabs->addTab(keys, tr("Tastaturbefehle"));

    // Erscheinungsbild
    auto* look = new QWidget(tabs);
    auto* ll = new QVBoxLayout(look);
    ll->addWidget(new QLabel(tr("Titel:"), look));
    auto* title = new QLineEdit(look);
    title->setReadOnly(true);
    ll->addWidget(title);
    auto* image = new QLabel(look);
    image->setMinimumSize(64, 64);
    ll->addWidget(image);
    ll->addStretch(1);
    later(title);
    tabs->addTab(look, tr("Erscheinungsbild"));

    auto shortcutFor = [shortcuts](const QString& path, QAction* a) {
        return shortcuts->contains(path) ? QKeySequence(shortcuts->value(path).toString())
                                         : QKeySequence(a->property("defaultShortcut").toString().isEmpty()
                                                            ? a->shortcut().toString()
                                                            : a->property("defaultShortcut").toString());
    };
    auto store = [this, shortcuts] {
        setPending(QStringLiteral("ws/custom/shortcuts"), QString::fromUtf8(QJsonDocument(*shortcuts).toJson(QJsonDocument::Compact)));
    };
    auto showDetails = [=, this] {
        QListWidgetItem* item = list->currentItem();
        currentKeys->clear();
        if (!item)
            return;
        const int i = item->data(Qt::UserRole).toInt();
        const auto& [path, a] = actions[i];
        tip->setText(a->statusTip().isEmpty() ? a->toolTip() : a->statusTip());
        where->setText(tr("Menüleiste/%1").arg(path));
        title->setText(plain(a->text()));
        image->setPixmap(a->icon().pixmap(48));
        const QKeySequence s = shortcutFor(path, a);
        if (!s.isEmpty())
            currentKeys->addItem(s.toString(QKeySequence::NativeText));
    };
    auto fill = [=, this] {
        list->clear();
        const QString category = categories->currentText();
        for (int i = 0; i < actions.size(); ++i) {
            if (!actions[i].first.startsWith(category + QLatin1Char('/')))
                continue;
            auto* item = new QListWidgetItem(actions[i].second->icon(), actions[i].first.mid(category.size() + 1), list);
            item->setData(Qt::UserRole, i);
            item->setToolTip(tr("Zum Ändern des Tastaturbefehls rechts unter 'Tastaturbefehle'"));
        }
        list->setCurrentRow(0);
    };
    connect(categories, &QComboBox::currentIndexChanged, this, fill);
    connect(list, &QListWidget::currentItemChanged, this, showDetails);
    connect(sequence, &QKeySequenceEdit::keySequenceChanged, this, [=, this](const QKeySequence& s) {
        QString owner;
        for (const auto& [path, a] : actions) {
            if (!s.isEmpty() && shortcutFor(path, a) == s)
                owner = path;
        }
        conflict->setText(owner);
    });
    connect(assign, &QPushButton::clicked, this, [=, this] {
        QListWidgetItem* item = list->currentItem();
        if (!item || sequence->keySequence().isEmpty())
            return;
        // Kürzel ist eindeutig: beim bisherigen Besitzer entfernen
        for (const auto& [path, a] : actions) {
            if (shortcutFor(path, a) == sequence->keySequence())
                shortcuts->insert(path, QString());
        }
        shortcuts->insert(actions[item->data(Qt::UserRole).toInt()].first, sequence->keySequence().toString());
        store();
        sequence->clear();
        showDetails();
    });
    connect(remove, &QPushButton::clicked, this, [=, this] {
        if (QListWidgetItem* item = list->currentItem()) {
            shortcuts->insert(actions[item->data(Qt::UserRole).toInt()].first, QString());
            store();
            showDetails();
        }
    });
    connect(resetAll, &QPushButton::clicked, this, [=, this] {
        *shortcuts = QJsonObject();
        store();
        showDetails();
    });
    fill();
    m_fields.insert(QStringLiteral("ws/custom/shortcuts"), holder);
    return holder;
}

// --- Dokument ---------------------------------------------------------------------------------

void OptionsDialog::buildDocument()
{
    const QString doc = tr("Dokument");
    page(doc + tr("/Allgemein"), tr("Allgemein"));
    group(tr("Grundeinstellungen"));
    check(QStringLiteral("doc/general/fillOpenCurves"), tr("Offene Kurven füllen"));
    markLaterStart();
    check(QStringLiteral("doc/general/autoConvertBitmaps"), tr("Automatisch alle Bitmaps für Bitmap-Effekte umwandeln"));
    laterRest();
    choice(QStringLiteral("doc/general/viewMode"), tr("Ansichtsmodus:"),
           {tr("Entwurf"), tr("Erweitert"), tr("Einfacher Umriss"), tr("Umriss"), tr("Normal"), tr("Pixel")});
    endGroup();
    group(tr("Duplikatsabstand"));
    {
        auto* h = number(QStringLiteral("doc/general/duplicateX"), tr("Horizontal:"), -1000, 1000, 2, QStringLiteral(" mm"));
        auto* v = number(QStringLiteral("doc/general/duplicateY"), tr("Vertikal:"), -1000, 1000, 2, QStringLiteral(" mm"));
        // Steht schon auf der Masterseite (wie in der Eigenschaftsleiste ohne Auswahl)
        m_pending.remove(QStringLiteral("doc/general/duplicateX"));
        m_pending.remove(QStringLiteral("doc/general/duplicateY"));
        for (QDoubleSpinBox* s : {h, v})
            disconnect(s, &QDoubleSpinBox::valueChanged, this, nullptr);
        h->setValue(m_editor.duplicateOffset().x());
        v->setValue(m_editor.duplicateOffset().y());
        m_appliers << [this, h, v] { m_editor.setDuplicateOffset(QPointF(h->value(), v->value())); };
    }
    endGroup();
    group(tr("Farben"));
    markLaterStart();
    choice(QStringLiteral("doc/general/colorMode"), tr("Standard-Farbmodus:"), {QStringLiteral("RGB"), QStringLiteral("CMYK")});
    laterRest();
    note(tr("TANGENTA arbeitet in RGB; Farbbilder werden intern mit 16 Bit je Kanal gerechnet."));
    endGroup();

    page(doc + tr("/Seitengrösse"), tr("Seitengrösse"));
    group(tr("Grösse und Ausrichtung"));
    {
        QBoxLayout* r = row();
        r->addWidget(new QLabel(tr("Grösse:"), m_pageWidget));
        auto* sizes = new QComboBox(m_pageWidget);
        sizes->addItem(tr("Benutzerdefiniert"));
        for (const PageSize& p : kPageSizes)
            sizes->addItem(QLatin1String(p.name), p.size);
        r->addWidget(sizes);
        r->addStretch(1);
        QBoxLayout* r2 = row();
        r2->addWidget(new QLabel(tr("Breite:"), m_pageWidget));
        auto* w = new QDoubleSpinBox(m_pageWidget);
        w->setRange(1, 10000);
        w->setDecimals(1);
        w->setSuffix(QStringLiteral(" mm"));
        r2->addWidget(w);
        r2->addWidget(new QLabel(tr("Höhe:"), m_pageWidget));
        auto* h = new QDoubleSpinBox(m_pageWidget);
        h->setRange(1, 10000);
        h->setDecimals(1);
        h->setSuffix(QStringLiteral(" mm"));
        r2->addWidget(h);
        auto* portrait = new QPushButton(tr("Hochformat"), m_pageWidget);
        auto* landscape = new QPushButton(tr("Querformat"), m_pageWidget);
        r2->addWidget(portrait);
        r2->addWidget(landscape);
        r2->addStretch(1);
        auto* currentOnly = new QCheckBox(tr("Grösse nur auf aktuelle Seite anwenden"), m_pageWidget);
        m_layout->addWidget(currentOnly);
        const QSizeF size = m_editor.currentPage().size();
        w->setValue(size.width());
        h->setValue(size.height());
        auto matchPreset = [=, this] {
            const QSignalBlocker block(sizes);
            sizes->setCurrentIndex(0);
            for (int i = 1; i < sizes->count(); ++i) {
                const QSizeF s = sizes->itemData(i).toSizeF();
                if ((std::abs(s.width() - w->value()) < 0.05 && std::abs(s.height() - h->value()) < 0.05)
                    || (std::abs(s.height() - w->value()) < 0.05 && std::abs(s.width() - h->value()) < 0.05))
                    sizes->setCurrentIndex(i);
            }
        };
        matchPreset();
        connect(sizes, &QComboBox::currentIndexChanged, this, [=, this](int i) {
            if (i <= 0)
                return;
            QSizeF s = sizes->itemData(i).toSizeF();
            if (w->value() > h->value())
                s.transpose(); // Querformat beibehalten
            w->setValue(s.width());
            h->setValue(s.height());
        });
        connect(w, &QDoubleSpinBox::valueChanged, this, matchPreset);
        connect(h, &QDoubleSpinBox::valueChanged, this, matchPreset);
        connect(portrait, &QPushButton::clicked, this, [=, this] {
            if (w->value() > h->value()) {
                const double t = w->value();
                w->setValue(h->value());
                h->setValue(t);
            }
        });
        connect(landscape, &QPushButton::clicked, this, [=, this] {
            if (h->value() > w->value()) {
                const double t = w->value();
                w->setValue(h->value());
                h->setValue(t);
            }
        });
        m_fields.insert(QStringLiteral("page/width"), w);
        m_fields.insert(QStringLiteral("page/height"), h);
        m_fields.insert(QStringLiteral("page/currentOnly"), currentOnly);
        m_appliers << [this, w, h, currentOnly] {
            const QSizeF want(w->value(), h->value());
            if (want != m_editor.currentPage().size())
                m_editor.setPageSize(want, !currentOnly->isChecked());
        };
    }
    check(QStringLiteral("doc/page/showBorder"), tr("Seitenrand anzeigen"));
    button(tr("Seitenrahmen hinzufügen"), [this] {
        auto frame = std::make_unique<RectangleObject>(m_editor.currentPage().size());
        m_editor.addObject(std::move(frame), tr("Seitenrahmen hinzufügen"));
    });
    endGroup();
    group(tr("Auflösung"));
    number(QStringLiteral("doc/page/resolution"), tr("Wiedergabeauflösung:"), 72, 2400, 0, QStringLiteral(" DPI"));
    note(tr("Auflösung für Bitmap-Export und Rohbilder ohne eigene Auflösung."));
    endGroup();
    group(tr("Randanschnitt"));
    number(QStringLiteral("doc/page/bleed"), tr("Randanschnitt:"), 0, 100, 1, QStringLiteral(" mm"));
    check(QStringLiteral("doc/page/showBleed"), tr("Randanschnittbereich anzeigen"));
    endGroup();

    page(doc + tr("/Layout"), tr("Layout"));
    {
        // Seiten je Bogen: Spalten x Zeilen je Layout
        static const QList<QSize> kSplit = {QSize(1, 1), QSize(2, 1), QSize(2, 1), QSize(1, 2), QSize(2, 2), QSize(2, 2), QSize(3, 1)};
        QComboBox* type = choice(QStringLiteral("doc/layout/type"), tr("Layout:"),
                                 {tr("Ganze Seite"), tr("Buch"), tr("Heft"), tr("Klappkarte"), tr("Faltkarte horizontal"),
                                  tr("Faltkarte vertikal"), tr("Parallelfalz")});
        auto* info = new QLabel(m_pageWidget);
        auto* preview = new QLabel(m_pageWidget);
        preview->setFixedSize(220, 120);
        preview->setFrameShape(QFrame::StyledPanel);
        preview->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(info);
        m_layout->addWidget(preview);
        const int before = Options::instance().integer(QStringLiteral("doc/layout/type"));
        // Bogengrösse: bei "Ganze Seite" die Seite selbst, sonst gemerkt
        const QSizeF current = m_editor.currentPage().size();
        const QSize oldSplit = kSplit.value(before, QSize(1, 1));
        const QSizeF sheet = before == 0 ? current : QSizeF(current.width() * oldSplit.width(), current.height() * oldSplit.height());
        auto refresh = [=, this] {
            const QSize split = kSplit.value(type->currentIndex(), QSize(1, 1));
            const QSizeF panel(sheet.width() / split.width(), sheet.height() / split.height());
            const int perSheet = split.width() * split.height();
            info->setText(tr("Breite: %1 mm   Höhe: %2 mm\n%3")
                              .arg(panel.width(), 0, 'f', 1)
                              .arg(panel.height(), 0, 'f', 1)
                              .arg(perSheet == 1 ? tr("Eine Seite pro Blatt") : tr("%1 Seiten pro Blatt").arg(perSheet)));
            QPixmap pix(200, 110);
            pix.fill(Qt::white);
            QPainter painter(&pix);
            const double s = std::min(180.0 / sheet.width(), 100.0 / sheet.height());
            const QRectF r(100 - sheet.width() * s / 2, 55 - sheet.height() * s / 2, sheet.width() * s, sheet.height() * s);
            painter.setPen(QPen(Qt::black, 1));
            painter.drawRect(r);
            painter.setPen(QPen(QColor(0x2b, 0x5c, 0xd6), 1, Qt::DashLine));
            for (int i = 1; i < split.width(); ++i)
                painter.drawLine(QPointF(r.left() + r.width() * i / split.width(), r.top()), QPointF(r.left() + r.width() * i / split.width(), r.bottom()));
            for (int i = 1; i < split.height(); ++i)
                painter.drawLine(QPointF(r.left(), r.top() + r.height() * i / split.height()), QPointF(r.right(), r.top() + r.height() * i / split.height()));
            painter.end();
            preview->setPixmap(pix);
        };
        connect(type, &QComboBox::currentIndexChanged, this, refresh);
        refresh();
        check(QStringLiteral("doc/layout/facing"), tr("Doppelseiten anzeigen"));
        choice(QStringLiteral("doc/layout/startOn"), tr("Beginnen bei:"), {tr("Linke Seite"), tr("Rechte Seite")});
        m_appliers << [=, this] {
            const int now = type->currentIndex();
            if (now == before)
                return;
            const QSize split = kSplit.value(now, QSize(1, 1));
            m_editor.setPageSize(QSizeF(sheet.width() / split.width(), sheet.height() / split.height()), true);
        };
    }
    note(tr("Wenn 'Doppelseiten' aktiviert ist, wird der Inhalt auf einer Seite zusammengeführt. Ein Teil des Inhalts "
            "kann je nach Ebenenstruktur auf eine weitere Seite gelangen. Ein Klick auf die zweite Seite macht sie zur "
            "aktuellen."));

    page(doc + tr("/Etikett"), tr("Etikett"));
    {
        radios(QStringLiteral("doc/label/labels"), QString(), {tr("Normales Papier"), tr("Etiketten")}, false);
        auto* holder = new QWidget(m_pageWidget);
        auto* l = new QHBoxLayout(holder);
        auto* tree = new QTreeWidget(holder);
        tree->setHeaderLabel(tr("Etikettenarten"));
        tree->setMinimumHeight(200);
        auto* preview = new QLabel(holder);
        preview->setFixedSize(170, 230);
        preview->setFrameShape(QFrame::StyledPanel);
        preview->setAlignment(Qt::AlignCenter);
        l->addWidget(tree, 1);
        l->addWidget(preview);
        m_layout->addWidget(holder);
        auto stock = std::make_shared<LabelStock>();
        const QString currentName = current(QStringLiteral("doc/label/name")).toString();
        QHash<QString, QTreeWidgetItem*> makers;
        const QList<LabelStock> stocks = labelStocks();
        for (int i = 0; i < stocks.size(); ++i) {
            QTreeWidgetItem*& maker = makers[stocks[i].maker];
            if (!maker)
                maker = new QTreeWidgetItem(tree, {stocks[i].maker});
            auto* item = new QTreeWidgetItem(maker, {stocks[i].name});
            item->setData(0, Qt::UserRole, i);
            if (stocks[i].maker + QLatin1Char(' ') + stocks[i].name == currentName) {
                *stock = stocks[i];
                tree->setCurrentItem(item);
            }
        }
        tree->expandAll();
        // Gespeicherten Bogen (auch angepassten) wiederherstellen
        if (!currentName.isEmpty()) {
            const Options& o = Options::instance();
            stock->label = QSizeF(o.number(QStringLiteral("doc/label/w")), o.number(QStringLiteral("doc/label/h")));
            stock->columns = o.integer(QStringLiteral("doc/label/cols"));
            stock->rows = o.integer(QStringLiteral("doc/label/rows"));
            stock->left = o.number(QStringLiteral("doc/label/left"));
            stock->top = o.number(QStringLiteral("doc/label/top"));
            stock->hGap = o.number(QStringLiteral("doc/label/hgap"));
            stock->vGap = o.number(QStringLiteral("doc/label/vgap"));
            stock->name = currentName;
        }
        auto show = [=] {
            QPixmap pix(160, 220);
            pix.fill(Qt::white);
            QPainter painter(&pix);
            const double s = std::min(150.0 / stock->sheet.width(), 210.0 / stock->sheet.height());
            const QPointF o(80 - stock->sheet.width() * s / 2, 110 - stock->sheet.height() * s / 2);
            painter.setPen(Qt::black);
            painter.drawRect(QRectF(o, stock->sheet * s));
            painter.setPen(QColor(0x2b, 0x5c, 0xd6));
            if (!stock->label.isEmpty())
                for (int i = 0; i < stock->perSheet(); ++i)
                    painter.drawRect(QRectF(o + stock->labelOrigin(i) * s, stock->label * s));
            painter.end();
            preview->setPixmap(pix);
        };
        auto store = [=, this] {
            setPending(QStringLiteral("doc/label/name"), stock->name);
            setPending(QStringLiteral("doc/label/w"), stock->label.width());
            setPending(QStringLiteral("doc/label/h"), stock->label.height());
            setPending(QStringLiteral("doc/label/cols"), stock->columns);
            setPending(QStringLiteral("doc/label/rows"), stock->rows);
            setPending(QStringLiteral("doc/label/left"), stock->left);
            setPending(QStringLiteral("doc/label/top"), stock->top);
            setPending(QStringLiteral("doc/label/hgap"), stock->hGap);
            setPending(QStringLiteral("doc/label/vgap"), stock->vGap);
        };
        connect(tree, &QTreeWidget::currentItemChanged, this, [=](QTreeWidgetItem* item) {
            if (!item || !item->data(0, Qt::UserRole).isValid())
                return;
            const LabelStock chosen = labelStocks().value(item->data(0, Qt::UserRole).toInt());
            *stock = chosen;
            stock->name = chosen.maker + QLatin1Char(' ') + chosen.name;
            store();
            show();
        });
        show();
        button(tr("Etikett anpassen …"), [=, this] {
            QDialog d(this);
            d.setWindowTitle(tr("Etikett anpassen"));
            auto* form = new QFormLayout(&d);
            auto spin = [&](double v, double max, int decimals) {
                auto* sp = new QDoubleSpinBox(&d);
                sp->setRange(0, max);
                sp->setDecimals(decimals);
                sp->setValue(v);
                return sp;
            };
            auto* w = spin(stock->label.width(), 1000, 2);
            auto* h = spin(stock->label.height(), 1000, 2);
            auto* cols = spin(stock->columns, 50, 0);
            auto* rows = spin(stock->rows, 100, 0);
            auto* left = spin(stock->left, 500, 2);
            auto* top = spin(stock->top, 500, 2);
            auto* hgap = spin(stock->hGap, 500, 2);
            auto* vgap = spin(stock->vGap, 500, 2);
            form->addRow(tr("Etikettbreite (mm):"), w);
            form->addRow(tr("Etiketthöhe (mm):"), h);
            form->addRow(tr("Spalten:"), cols);
            form->addRow(tr("Zeilen:"), rows);
            form->addRow(tr("Rand links (mm):"), left);
            form->addRow(tr("Rand oben (mm):"), top);
            form->addRow(tr("Spaltenabstand (mm):"), hgap);
            form->addRow(tr("Zeilenabstand (mm):"), vgap);
            auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &d);
            box->button(QDialogButtonBox::Cancel)->setText(tr("Abbrechen"));
            connect(box, &QDialogButtonBox::accepted, &d, &QDialog::accept);
            connect(box, &QDialogButtonBox::rejected, &d, &QDialog::reject);
            form->addRow(box);
            if (d.exec() != QDialog::Accepted)
                return;
            stock->label = QSizeF(w->value(), h->value());
            stock->columns = std::max(1, int(cols->value()));
            stock->rows = std::max(1, int(rows->value()));
            stock->left = left->value();
            stock->top = top->value();
            stock->hGap = hgap->value();
            stock->vGap = vgap->value();
            if (stock->name.isEmpty())
                stock->name = tr("Benutzerdefiniert");
            store();
            show();
        });
        m_appliers << [=, this] {
            // Etikettenmodus: eine Seite ist ein Etikett
            if (Options::instance().integer(QStringLiteral("doc/label/labels")) == 1 && !stock->label.isEmpty()
                && stock->label != m_editor.currentPage().size())
                m_editor.setPageSize(stock->label, true);
        };
    }

    page(doc + tr("/Hintergrund"), tr("Hintergrund"));
    radios(QStringLiteral("doc/background/type"), QString(), {tr("Kein Hintergrund"), tr("Durchgezogen"), tr("Bitmaps")}, false);
    colorField(QStringLiteral("doc/background/color"), tr("Farbe:"));
    {
        QBoxLayout* r = row();
        r->addWidget(new QLabel(tr("Bitmap:"), m_pageWidget));
        auto* path = new QLineEdit(m_pageWidget);
        path->setReadOnly(true);
        const QString source = current(QStringLiteral("doc/background/bitmap")).toString();
        path->setText(source.startsWith(QLatin1String("pool:")) ? tr("(eingebettet)") : source);
        r->addWidget(path, 1);
        auto* browse = new QPushButton(tr("Durchsuchen …"), m_pageWidget);
        r->addWidget(browse);
        connect(browse, &QPushButton::clicked, this, [this, path] {
            const QString file = QFileDialog::getOpenFileName(this, tr("Hintergrund-Bitmap"), QString(),
                                                              tr("Bilder (*.png *.jpg *.jpeg *.tif *.tiff *.bmp *.webp)"));
            if (file.isEmpty())
                return;
            path->setText(file);
            setPending(QStringLiteral("doc/background/bitmap"), file);
            setPending(QStringLiteral("doc/background/type"), 2);
            if (auto* holder = m_fields.value(QStringLiteral("doc/background/type"))) {
                const QList<QRadioButton*> bs = holder->findChildren<QRadioButton*>();
                if (bs.size() == 3)
                    bs[2]->setChecked(true);
            }
        });
    }
    radios(QStringLiteral("doc/background/linked"), tr("Quelle:"), {tr("Verknüpft"), tr("Eingebettet")});
    check(QStringLiteral("doc/background/customSize"), tr("Angepasste Grösse (sonst Standardgrösse, gekachelt)"));
    number(QStringLiteral("doc/background/width"), tr("H:"), 0, 10000, 1, QStringLiteral(" mm"));
    number(QStringLiteral("doc/background/height"), tr("V:"), 0, 10000, 1, QStringLiteral(" mm"));
    check(QStringLiteral("doc/background/keepRatio"), tr("Seitenverhältnis beibehalten"));
    check(QStringLiteral("doc/background/printExport"), tr("Hintergrund drucken und exportieren"));
    m_appliers.prepend([this] {
        // Eingebettet: Datei in den Bildpool (wird mit dem Dokument gespeichert)
        const QString source = m_pending.value(QStringLiteral("doc/background/bitmap")).toString();
        if (source.isEmpty() || source.startsWith(QLatin1String("pool:")))
            return;
        if (current(QStringLiteral("doc/background/linked")).toInt() == 1) {
            QFile file(source);
            if (file.open(QIODevice::ReadOnly)) {
                const QString key = imagepool::add(file.readAll());
                if (!key.isEmpty())
                    Options::instance().set(QStringLiteral("doc/background/bitmap"), QStringLiteral("pool:") + key);
            }
        }
    });

    buildGuidePages();

    page(doc + tr("/Gitter"), tr("Gitter"));
    group(tr("Dokumentgitter"));
    choice(QStringLiteral("doc/grid/mode"), tr("Angabe:"), {tr("Gitterlinien pro Millimeter"), tr("Abstand in Millimetern")});
    number(QStringLiteral("doc/grid/h"), tr("Horizontal:"), 0.001, 1000, 3);
    number(QStringLiteral("doc/grid/v"), tr("Vertikal:"), 0.001, 1000, 3);
    check(QStringLiteral("doc/grid/snap"), tr("An Gitter ausrichten"));
    check(QStringLiteral("doc/grid/show"), tr("Gitter anzeigen"));
    radios(QStringLiteral("doc/grid/dots"), QString(), {tr("Als Linien"), tr("Als Punkte")}, false);
    endGroup();
    group(tr("Basisliniengitter"));
    number(QStringLiteral("doc/grid/baseline/spacing"), tr("Abstand:"), 1, 200, 1, QStringLiteral(" pt"));
    colorField(QStringLiteral("doc/grid/baseline/color"), tr("Farbe:"));
    number(QStringLiteral("doc/grid/baseline/top"), tr("Von oben beginnen:"), 0, 1000, 1, QStringLiteral(" mm"));
    check(QStringLiteral("doc/grid/baseline/snap"), tr("An Gitter ausrichten"));
    check(QStringLiteral("doc/grid/baseline/show"), tr("Gitter anzeigen"));
    endGroup();
    group(tr("Pixelgitter"));
    slider(QStringLiteral("doc/grid/pixel/opacity"), tr("Deckkraft:"), 0, 100, QString());
    colorField(QStringLiteral("doc/grid/pixel/color"), tr("Farbe:"));
    check(QStringLiteral("doc/grid/pixel/show"), tr("Gitter anzeigen (auf mind. 800 % vergrössern)"));
    check(QStringLiteral("doc/grid/pixel/snap"), tr("An Pixel ausrichten"));
    endGroup();

    page(doc + tr("/Lineale"), tr("Lineale"));
    group(tr("Schrittweise verschieben"));
    {
        auto* nudge = number(QStringLiteral("doc/rulers/nudge"), tr("Schrittweite:"), 0.001, 1000, 3, QStringLiteral(" mm"));
        m_pending.remove(QStringLiteral("doc/rulers/nudge"));
        disconnect(nudge, &QDoubleSpinBox::valueChanged, this, nullptr);
        nudge->setValue(m_editor.nudge());
        m_appliers << [this, nudge] { m_editor.setNudge(nudge->value()); };
    }
    number(QStringLiteral("doc/rulers/super"), tr("Grössere Schrittweite (Umschalt): ×"), 1, 100, 0);
    number(QStringLiteral("doc/rulers/micro"), tr("Kleinere Schrittweite (Strg): ÷"), 1, 100, 0);
    markLaterStart();
    check(QStringLiteral("doc/rulers/sameUnits"), tr("Gleiche Einheiten für Duplikatentfernung, Schrittweite und Lineale"));
    laterRest();
    endGroup();
    group(tr("Einheiten"));
    choice(QStringLiteral("doc/rulers/units"), tr("Horizontal:"), Options::unitNames());
    choice(QStringLiteral("doc/rulers/unitsV"), tr("Vertikal:"), Options::unitNames());
    check(QStringLiteral("doc/rulers/sameHV"), tr("Gleiche Einheiten für horizontales und vertikales Lineal"));
    endGroup();
    group(tr("Ursprung"));
    number(QStringLiteral("doc/rulers/originX"), tr("Horizontal:"), -10000, 10000, 3, QStringLiteral(" mm"));
    number(QStringLiteral("doc/rulers/originY"), tr("Vertikal:"), -10000, 10000, 3, QStringLiteral(" mm"));
    note(tr("Tipp: Aus der Ecke zwischen den Linealen auf die Zeichenfläche ziehen setzt den Ursprung dort."));
    endGroup();
    group(tr("Unterteilungen"));
    number(QStringLiteral("doc/rulers/ticks"), tr("Unterteilungen pro Einheit:"), 1, 100, 0);
    check(QStringLiteral("doc/rulers/moreTicks"), tr("Weitere Unterteilungen anzeigen"));
    endGroup();
    check(QStringLiteral("ws/custom/bar/rulers"), tr("Lineale anzeigen"));
    group(tr("Zeichnungsmassstab (Skalierung bearbeiten)"));
    number(QStringLiteral("doc/rulers/scale"), tr("1 mm auf der Seite entspricht (mm):"), 0.0001, 1000000, 4);
    note(tr("Beispiel 1:100 – 1 mm auf der Seite sind 100 mm in Wirklichkeit; die Lineale und die Statusleiste zeigen "
            "dann die wirklichen Masse."));
    endGroup();

    page(doc + tr("/Speichern"), tr("Speichern"));
    markLaterStart();
    check(QStringLiteral("doc/save/cmx"), tr("Präsentationsaustausch speichern (CMX)"));
    laterRest();
    note(tr("Dateioptimierung:"));
    markLaterStart();
    check(QStringLiteral("doc/save/bitmapCompression"), tr("Bitmap-Komprimierung verwenden"));
    check(QStringLiteral("doc/save/graphicCompression"), tr("Grafikobjekt-Komprimierung verwenden"));
    radios(QStringLiteral("doc/save/patterns"), tr("Füllmuster:"),
           {tr("Füllmuster mit Datei speichern"), tr("Füllmuster beim Öffnen der Datei neu aufbauen")});
    radios(QStringLiteral("doc/save/blends"), tr("Überblendungen und Extrusionen:"),
           {tr("Mit der Datei speichern"), tr("Beim Öffnen der Datei neu aufbauen")});
    laterRest();
    note(tr("TANGENTA-Dateien (.vek) sind ZIP-Archive; Bilder werden darin unverändert abgelegt."));

    page(doc + tr("/HTML exportieren"), tr("HTML exportieren"));
    markLaterStart();
    number(QStringLiteral("doc/html/tolerance"), tr("Positionstoleranz:"), 0, 100, 0);
    number(QStringLiteral("doc/html/whitespace"), tr("Weisser Leerraum im Bild:"), 0, 1000, 0);
    number(QStringLiteral("doc/html/whitespacePos"), tr("Weissen Leerraum positionieren:"), 0, 100, 0);
    check(QStringLiteral("doc/html/client"), tr("Imagemap: Client"));
    check(QStringLiteral("doc/html/server"), tr("Imagemap: Server"));
    radios(QStringLiteral("doc/html/serverType"), QString(), {QStringLiteral("NCSA"), QStringLiteral("CERN")}, false);
    laterRest();
}

void OptionsDialog::buildGuidePages()
{
    const QString base = tr("Dokument/Hilfslinien");
    page(base, tr("Hilfslinien"));
    check(QStringLiteral("doc/guides/show"), tr("Hilfslinien anzeigen"));
    check(QStringLiteral("doc/guides/snap"), tr("An Hilfslinien ausrichten"));
    colorField(QStringLiteral("doc/guides/color"), tr("Standardfarbe für Hilfslinien:"));
    colorField(QStringLiteral("doc/guides/presetColor"), tr("Standardfarbe für voreingestellte Hilfslinien:"));

    // Waagrechte, senkrechte und schräge Hilfslinien als Listen der Arbeitskopie
    enum class Kind { Horizontal, Vertical, Slanted };
    auto listPage = [this](Kind kind, const QString& path, const QString& title) {
        page(path, title);
        auto* holder = new QWidget(m_pageWidget);
        auto* l = new QHBoxLayout(holder);
        auto* left = new QVBoxLayout;
        auto* x = new QDoubleSpinBox(holder);
        x->setRange(-100000, 100000);
        x->setDecimals(3);
        x->setSuffix(QStringLiteral(" mm"));
        auto* y = new QDoubleSpinBox(holder);
        y->setRange(-100000, 100000);
        y->setDecimals(3);
        y->setSuffix(QStringLiteral(" mm"));
        auto* angle = new QDoubleSpinBox(holder);
        angle->setRange(-360, 360);
        angle->setDecimals(3);
        angle->setSuffix(tr(" Grad"));
        auto* onPage = new QCheckBox(tr("Nur auf dieser Seite"), holder);
        if (kind == Kind::Slanted) {
            auto* f = new QHBoxLayout;
            f->addWidget(new QLabel(QStringLiteral("X:"), holder));
            f->addWidget(x);
            f->addWidget(new QLabel(QStringLiteral("Y:"), holder));
            f->addWidget(y);
            f->addWidget(new QLabel(tr("Winkel:"), holder));
            f->addWidget(angle);
            left->addLayout(f);
        } else {
            left->addWidget(kind == Kind::Horizontal ? y : x);
            x->setVisible(kind == Kind::Vertical);
            y->setVisible(kind == Kind::Horizontal);
            angle->hide();
        }
        left->addWidget(onPage);
        auto* list = new QListWidget(holder);
        left->addWidget(list);
        l->addLayout(left, 1);
        auto* buttons = new QVBoxLayout;
        auto* add = new QPushButton(tr("Hinzufügen"), holder);
        auto* move = new QPushButton(tr("Verschieben"), holder);
        auto* del = new QPushButton(tr("Löschen"), holder);
        auto* clear = new QPushButton(tr("Alle löschen"), holder);
        for (QPushButton* b : {add, move, del, clear})
            buttons->addWidget(b);
        buttons->addStretch(1);
        l->addLayout(buttons);
        m_layout->addWidget(holder);

        auto matches = [kind](const Guide& g) {
            return kind == Kind::Horizontal ? g.isHorizontal() : kind == Kind::Vertical ? g.isVertical() : !g.isHorizontal() && !g.isVertical();
        };
        auto refresh = [=, this] {
            const int row = list->currentRow();
            list->clear();
            for (int i = 0; i < m_guides.size(); ++i) {
                const auto& [g, local] = m_guides[i];
                if (!matches(g))
                    continue;
                QString text = kind == Kind::Horizontal ? QString::number(g.anchor.y(), 'f', 3)
                               : kind == Kind::Vertical ? QString::number(g.anchor.x(), 'f', 3)
                                                        : QStringLiteral("(%1, %2) %3°").arg(g.anchor.x(), 0, 'f', 2).arg(g.anchor.y(), 0, 'f', 2).arg(g.angle, 0, 'f', 2);
                if (local)
                    text += tr("  (nur diese Seite)");
                auto* item = new QListWidgetItem(text, list);
                item->setData(Qt::UserRole, i);
            }
            list->setCurrentRow(std::min(row, list->count() - 1));
        };
        m_guideRefreshers << refresh;
        auto made = [=, this] {
            if (kind == Kind::Horizontal)
                return Guide::horizontal(y->value(), 0.0);
            if (kind == Kind::Vertical)
                return Guide::vertical(x->value(), 0.0);
            return Guide{QPointF(x->value(), y->value()), angle->value()};
        };
        auto changed = [this] {
            m_guidesChanged = true;
            for (const auto& r : std::as_const(m_guideRefreshers))
                r();
        };
        connect(list, &QListWidget::currentItemChanged, this, [=, this](QListWidgetItem* item) {
            if (!item)
                return;
            const auto& [g, local] = m_guides[item->data(Qt::UserRole).toInt()];
            x->setValue(g.anchor.x());
            y->setValue(g.anchor.y());
            angle->setValue(g.angle);
            onPage->setChecked(local);
        });
        connect(add, &QPushButton::clicked, this, [=, this] {
            m_guides << std::pair{made(), onPage->isChecked()};
            changed();
        });
        connect(move, &QPushButton::clicked, this, [=, this] {
            if (QListWidgetItem* item = list->currentItem()) {
                m_guides[item->data(Qt::UserRole).toInt()] = std::pair{made(), onPage->isChecked()};
                changed();
            }
        });
        connect(del, &QPushButton::clicked, this, [=, this] {
            if (QListWidgetItem* item = list->currentItem()) {
                m_guides.removeAt(item->data(Qt::UserRole).toInt());
                changed();
            }
        });
        connect(clear, &QPushButton::clicked, this, [=, this] {
            m_guides.removeIf([&](const std::pair<Guide, bool>& e) { return matches(e.first); });
            changed();
        });
        refresh();
        if (kind != Kind::Slanted) {
            check(QStringLiteral("doc/guides/show"), tr("Hilfslinien anzeigen"));
            check(QStringLiteral("doc/guides/snap"), tr("An Hilfslinien ausrichten"));
        }
        m_fields.insert(QStringLiteral("guides/") + title, holder);
    };
    listPage(Kind::Horizontal, base + tr("/Horizontal"), tr("Horizontal"));
    listPage(Kind::Vertical, base + tr("/Vertikal"), tr("Vertikal"));
    listPage(Kind::Slanted, base + tr("/Hilfslinien"), tr("Hilfslinien"));

    page(base + tr("/Voreinstellungen"), tr("Voreinstellungen"));
    radios(QStringLiteral("doc/guides/presetMode"), QString(), {tr("Standard-Voreinstellungen"), tr("Benutzerdefinierte Voreinstellungen")}, false);
    checkList(QStringLiteral("doc/guides/presets"),
              {{QStringLiteral("cm"), tr("Ein-Zentimeter-Ränder")}, {QStringLiteral("bleed"), tr("Randanschnittbereich")},
               {QStringLiteral("page"), tr("Seitenränder")}, {QStringLiteral("printable"), tr("Druckbarer Bereich")},
               {QStringLiteral("columns"), tr("Dreispaltiges Rundschreiben")}, {QStringLiteral("grid"), tr("Grundgitter")},
               {QStringLiteral("origin"), tr("Ausgangspunkt für Gitter (obere linke Ecke)")}});
    group(tr("Benutzerdefiniert"));
    number(QStringLiteral("doc/guides/custom/margin"), tr("Ränder:"), 0, 1000, 2, QStringLiteral(" mm"));
    number(QStringLiteral("doc/guides/custom/columns"), tr("Spalten:"), 1, 50, 0);
    number(QStringLiteral("doc/guides/custom/gutter"), tr("Spaltenabstand:"), 0, 1000, 2, QStringLiteral(" mm"));
    number(QStringLiteral("doc/guides/custom/gridH"), tr("Raster waagrecht (mit 'Grundgitter'):"), 0, 1000, 2, QStringLiteral(" mm"));
    number(QStringLiteral("doc/guides/custom/gridV"), tr("Raster senkrecht:"), 0, 1000, 2, QStringLiteral(" mm"));
    endGroup();
    button(tr("Voreinstellung zuweisen"), [this] {
        // Erst die gewählten Werte übernehmen, dann die Hilfslinien in die Arbeitskopie (Farbe: voreingestellt)
        Options::instance().apply(m_pending);
        for (const Guide& g : Options::instance().presetGuides(m_editor.currentPage().size()))
            m_guides << std::pair{g, false};
        m_guidesChanged = true;
        for (const auto& r : std::as_const(m_guideRefreshers))
            r();
    });

    m_appliers << [this] {
        if (!m_guidesChanged)
            return;
        QList<Guide> global;
        QList<Guide> local;
        for (const auto& [g, onPage] : std::as_const(m_guides))
            (onPage ? local : global) << g;
        m_editor.replaceGuides(global, local);
    };
}

// --- Global -----------------------------------------------------------------------------------

void OptionsDialog::buildGlobal()
{
    const QString base = tr("Global");
    page(base + tr("/Drucken"), tr("Drucken"));
    choice(QStringLiteral("global/print/style"), tr("Standarddruckstil:"), {tr("TANGENTA-Standard")});
    check(QStringLiteral("global/print/currentOnly"), tr("Standardmässig nur aktuelle Seite drucken"));
    note(tr("Etiketten (Dokument > Etikett) und Layouts wie 'Buch' werden beim Drucken auf die Bögen verteilt."));

    page(base + tr("/Drucken/Treiberkompatibilität"), tr("Treiberkompatibilität"));
    markLaterStart();
    check(QStringLiteral("global/print/textAsGraphics"), tr("Gesamten Text als Grafik"));
    check(QStringLiteral("global/print/softwareClip"), tr("Software-Schneidung für Füllungen verwenden"));
    check(QStringLiteral("global/print/bitmapChunks"), tr("Bitmaps in 64-KB-Stücken ausgeben"));
    check(QStringLiteral("global/print/beziers"), tr("Bézier-Kurven und Strecken an Treiber senden"));
    check(QStringLiteral("global/print/bands"), tr("Streifen an Treiber senden"));
    laterRest();
    check(QStringLiteral("global/print/pageSize"), tr("Drucker kann Dokumentseitengrössen anpassen"));
    note(tr("Ohne diese Option wird der Bogen mittig auf das Papier des Druckers gesetzt und wenn nötig "
            "verkleinert. Text wird immer als Kurven gedruckt; die übrigen Schalter betreffen alte "
            "Windows-Druckertreiber und haben in TANGENTA keine Wirkung."));

    page(base + tr("/Bitmap Effects"), tr("Bitmap Effects"));
    radios(QStringLiteral("global/bitmapfx/preview"), tr("Vorschaumethode beim Öffnen:"),
           {tr("Vollbild"), tr("Vorher/Nachher"), tr("Nur Ergebnis"), tr("Zuletzt benutzt")});
    check(QStringLiteral("global/bitmapfx/lastValues"), tr("In den Eingabefeldern zuletzt verwendete Werte anzeigen"));
    note(tr("Gilt für Effekte > Anpassen und Korrektur. 'Vollbild' zeigt wie 'Nur Ergebnis' die ganze "
            "Vorschau; in jedem Dialog lässt sich die Vorschau unter dem Bild umschalten."));

    page(base + tr("/Filter"), tr("Filter"));
    note(tr("Importieren kann TANGENTA alle Bildformate von Qt (PNG, JPEG, TIFF 16 Bit, WebP, GIF, BMP, …) und "
            "Kamera-Rohdaten (NEF, CR2, CR3, ARW, DNG, RAF, ORF, …)."));
    markLaterStart();
    button(tr("Zurücksetzen"), {});
    laterRest();

    page(base + tr("/Filter/Verknüpfen"), tr("Verknüpfen"));
#ifndef Q_OS_WIN
    markLaterStart();
#endif
    checkList(QStringLiteral("global/filters/associate"),
              {{QStringLiteral("vek"), tr("VEK (TANGENTA)")}, {QStringLiteral("svg"), tr("SVG (Scalable Vector Graphics)")},
               {QStringLiteral("nef"), tr("NEF (Nikon-Rohdaten)")}, {QStringLiteral("png"), tr("PNG")}});
#ifndef Q_OS_WIN
    laterRest();
    note(tr("Unter Linux ordnet die Desktop-Datei (tangenta.desktop) die Dateitypen zu."));
#else
    note(tr("VEK-Dateien öffnen sich danach per Doppelklick in TANGENTA. Die übrigen Typen erscheinen im "
            "Explorer unter 'Öffnen mit', ohne das bisherige Standardprogramm zu ersetzen."));
#endif

    page(base + tr("/Farbe"), tr("Farbe"));
    markLaterStart();
    choice(QStringLiteral("global/color/spotPalette"), tr("Standard-Schmuckfarbenpalette:"), {tr("Keine (nur RGB)")});
    choice(QStringLiteral("global/color/pantone"), tr("PANTONE-Farben:"), {tr("Keine")});
    check(QStringLiteral("global/color/paletteDialog"), tr("Dialogfeld 'Farbpalette auswählen' anzeigen"));
    laterRest();

    // Bildersuche (Datei > Bildersuche): Schlüssel bleiben lokal beim Benutzer
    page(base + tr("/Bildersuche"), tr("Bildersuche"));
    note(tr("Wikimedia Commons, Openverse und Icons8 (kleine PNG) funktionieren ohne Schlüssel. Für die übrigen "
            "Archive hier je einen API-Schlüssel eintragen; leere Felder = Archiv wird nicht gefragt. Die Schlüssel "
            "bleiben lokal in den Einstellungen dieses Benutzers."));
    auto* tester = new ImageSearch(this);
    for (const imagesearch::Source& s : imagesearch::sources()) {
        if (s.key == QLatin1String("commons") || s.key == QLatin1String("openverse") || s.key == QLatin1String("flaticon"))
            continue;
        const QString key = imagesearch::optionKey(s.key);
        QBoxLayout* r = textField(key, s.key == QLatin1String("magnific") ? tr("Magnific / Flaticon:") : s.name + QLatin1Char(':'));
        auto* edit = qobject_cast<QLineEdit*>(field(key));
        if (!edit)
            continue;
        edit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
        edit->setMaximumWidth(320);
        edit->setToolTip(s.hint);
        // Test: kleiner echter Abruf mit dem Schlüssel im Feld (auch ungespeichert), Ergebnis darunter
        auto* test = new QPushButton(tr("Test"), m_pageWidget);
        test->setObjectName(QStringLiteral("test-") + s.key);
        test->setToolTip(tr("Schlüssel mit einer kleinen Probesuche prüfen (lädt nichts herunter)"));
        r->insertWidget(2, test);
        auto* result = new QLabel(m_pageWidget);
        result->setObjectName(QStringLiteral("result-") + s.key);
        result->setWordWrap(true);
        result->setTextInteractionFlags(Qt::TextSelectableByMouse);
        result->setContentsMargins(176, 0, 0, 4);
        result->hide();
        m_layout->addWidget(result);
        const QString sourceKey = s.key;
        connect(test, &QPushButton::clicked, this, [tester, sourceKey, edit, test, result] {
            test->setEnabled(false);
            result->setStyleSheet(QString());
            result->setText(tr("Prüfe …"));
            result->show();
            QPointer<QLabel> label(result);
            QPointer<QPushButton> button(test);
            tester->testKey(sourceKey, edit->text(), [label, button](bool ok, const QString& message) {
                if (button)
                    button->setEnabled(true);
                if (!label)
                    return;
                label->setStyleSheet(ok ? QStringLiteral("color: #1e7b34;") : QStringLiteral("color: #b3261e;"));
                label->setText((ok ? QStringLiteral("✔ ") : QStringLiteral("✘ ")) + message);
            });
        });
        connect(edit, &QLineEdit::textChanged, result, &QLabel::hide);
    }
    note(tr("Flaticon gehört zu Magnific (früher Freepik) und nimmt denselben Schlüssel. Magnific rechnet die "
            "Schnittstelle getrennt vom Abonnement ab. Getty, iStock und Adobe Stock liefern nur Vorschauen mit "
            "Wasserzeichen."));

    // KI (Ansicht > Andockfenster > KI): Zugänge zu den Anbietern, lokal je Benutzer
    page(base + tr("/KI"), tr("KI"));
    note(tr("Zugänge zu KI-Anbietern. Am günstigsten ist „Claude (Abo über Claude Code)“: es nutzt das installierte "
            "Claude Code mit dem eigenen Claude-Abo, ohne API-Kosten. Die übrigen Zugänge rechnen nach Verbrauch ab. "
            "Der mit ★ markierte Zugang ist die Vorgabe im KI-Fenster. Schlüssel bleiben lokal bei diesem Benutzer."));
    {
        auto state = std::make_shared<QList<ai::Account>>(ai::accountsFromJson(
            QJsonDocument::fromJson(current(QStringLiteral("global/ai/accounts")).toString().toUtf8()).array()));
        auto chosen = std::make_shared<QString>(current(QStringLiteral("global/ai/account")).toString());
        auto* list = new QListWidget(m_pageWidget);
        list->setObjectName(QStringLiteral("ai-accounts"));
        list->setMinimumHeight(140);
        m_layout->addWidget(list);
        auto* result = new QLabel(m_pageWidget);
        result->setObjectName(QStringLiteral("ai-result"));
        result->setWordWrap(true);
        result->setTextInteractionFlags(Qt::TextSelectableByMouse);
        result->hide();
        auto refill = [this, state, chosen, list] {
            const int row = list->currentRow();
            list->clear();
            if (!state->isEmpty() && std::none_of(state->begin(), state->end(), [&](const ai::Account& a) { return a.id == *chosen; }))
                *chosen = state->first().id;
            for (const ai::Account& a : std::as_const(*state)) {
                const ai::Kind* k = ai::kind(a.kind);
                const QString model = a.model.isEmpty() && k ? k->defaultModel : a.model;
                const QString kindName = k ? k->name : a.kind;
                const QString name = a.label.trimmed().isEmpty() ? kindName : a.label.trimmed() + QStringLiteral("  —  ") + kindName;
                auto* item = new QListWidgetItem((a.id == *chosen ? QStringLiteral("★ ") : QStringLiteral("    ")) + name +
                                                     (model.isEmpty() ? QString() : QStringLiteral(" · ") + model),
                                                 list);
                item->setData(Qt::UserRole, a.id);
            }
            list->setCurrentRow(std::clamp(row, 0, int(list->count()) - 1));
            setPending(QStringLiteral("global/ai/accounts"), QString::fromUtf8(QJsonDocument(ai::toJson(*state)).toJson(QJsonDocument::Compact)));
            setPending(QStringLiteral("global/ai/account"), *chosen);
        };
        refill();
        auto* buttons = new QHBoxLayout;
        auto add = [&](const QString& text, const QString& name) {
            auto* b = new QPushButton(text, m_pageWidget);
            b->setObjectName(name);
            buttons->addWidget(b);
            return b;
        };
        QPushButton* create = add(tr("Neu …"), QStringLiteral("ai-new"));
        QPushButton* edit = add(tr("Bearbeiten …"), QStringLiteral("ai-edit"));
        QPushButton* remove = add(tr("Entfernen"), QStringLiteral("ai-remove"));
        QPushButton* standard = add(tr("Als Vorgabe"), QStringLiteral("ai-default"));
        QPushButton* test = add(tr("Test"), QStringLiteral("ai-test"));
        QPushButton* login = add(tr("Anmelden"), QStringLiteral("ai-login"));
        buttons->addStretch(1);
        m_layout->addLayout(buttons);
        m_layout->addWidget(result);
        auto selected = [state, list]() -> ai::Account* {
            const QString id = list->currentItem() ? list->currentItem()->data(Qt::UserRole).toString() : QString();
            for (ai::Account& a : *state)
                if (a.id == id)
                    return &a;
            return nullptr;
        };
        auto updateButtons = [=] {
            const ai::Account* a = selected();
            edit->setEnabled(a);
            remove->setEnabled(a);
            standard->setEnabled(a);
            test->setEnabled(a);
            login->setEnabled(a && a->kind == QLatin1String("claude_code"));
        };
        connect(list, &QListWidget::currentRowChanged, this, [=] {
            updateButtons();
            result->hide();
        });
        updateButtons();
        connect(create, &QPushButton::clicked, this, [=, this] {
            ai::Account a;
            a.id = ai::newAccountId();
            a.kind = state->isEmpty() || std::none_of(state->begin(), state->end(), [](const ai::Account& x) { return x.kind == QLatin1String("claude_code"); })
                         ? QStringLiteral("claude_code")
                         : QStringLiteral("claude");
            if (!editAiAccount(a))
                return;
            state->append(a);
            if (state->size() == 1)
                *chosen = a.id;
            refill();
            list->setCurrentRow(int(list->count()) - 1);
        });
        connect(edit, &QPushButton::clicked, this, [=, this] {
            if (ai::Account* a = selected()) {
                ai::Account copy = *a;
                if (editAiAccount(copy)) {
                    *a = copy;
                    refill();
                }
            }
        });
        connect(list, &QListWidget::itemDoubleClicked, edit, &QPushButton::click);
        connect(remove, &QPushButton::clicked, this, [=] {
            if (const ai::Account* a = selected()) {
                const QString id = a->id;
                state->erase(std::remove_if(state->begin(), state->end(), [&](const ai::Account& x) { return x.id == id; }), state->end());
                refill();
                updateButtons();
            }
        });
        connect(standard, &QPushButton::clicked, this, [=] {
            if (const ai::Account* a = selected()) {
                *chosen = a->id;
                refill();
            }
        });
        connect(test, &QPushButton::clicked, this, [=, this] {
            const ai::Account* a = selected();
            if (!a)
                return;
            const ai::Account account = *a;
            test->setEnabled(false);
            result->setStyleSheet(QString());
            result->setText(tr("Prüfe …"));
            result->show();
            QPointer<QLabel> label(result);
            QPointer<QPushButton> button(test);
            auto show = [label, button](bool ok, const QString& message) {
                if (button)
                    button->setEnabled(true);
                if (!label)
                    return;
                label->setStyleSheet(ok ? QStringLiteral("color: #1e7b34;") : QStringLiteral("color: #b3261e;"));
                label->setText((ok ? QStringLiteral("✔ ") : QStringLiteral("✘ ")) + message);
            };
            if (account.kind == QLatin1String("claude_code")) {
                ClaudeCode::status(this, [this, account, show](bool found, bool loggedIn, const QString& info) {
                    if (!found || !loggedIn)
                        return show(false, info);
                    auto* runner = new ClaudeCode(this);
                    const ai::Kind* k = ai::kind(account.kind);
                    runner->test(account.model.isEmpty() ? k->defaultModel : account.model, [runner, info, show](bool ok, const QString& message) {
                        show(ok, ok ? info + QLatin1Char('\n') + message : message);
                        runner->deleteLater();
                    });
                });
                return;
            }
            ai::Provider* provider = ai::Provider::create(account, this);
            provider->test([provider, show](bool ok, const QString& message) {
                show(ok, message);
                provider->deleteLater();
            });
        });
        connect(login, &QPushButton::clicked, this, [=] {
            result->show();
            if (!ClaudeCode::login()) {
                result->setStyleSheet(QStringLiteral("color: #b3261e;"));
                result->setText(tr("✘ Claude Code ist nicht installiert oder nicht gefunden. Installation: claude.com/claude-code; "
                                   "der Pfad lässt sich unten angeben."));
                return;
            }
            result->setStyleSheet(QString());
            result->setText(tr("Die Anmeldung läuft in einem eigenen Fenster und im Browser. Danach hier „Test“ wählen."));
        });
    }
    check(QStringLiteral("global/ai/mayWrite"), tr("KI darf das Dokument ändern (Vorgabe im KI-Fenster)"));
    textField(QStringLiteral("global/ai/claudePath"), tr("Pfad zu Claude Code:"));
    if (auto* path = qobject_cast<QLineEdit*>(field(QStringLiteral("global/ai/claudePath")))) {
        path->setMinimumWidth(420);
        path->setPlaceholderText(ClaudeCode::executable().isEmpty() ? tr("nicht gefunden") : ClaudeCode::executable());
        path->setToolTip(tr("Leer = automatisch suchen (PATH, npm, ~/.local/bin)"));
    }

    // Websuche der KI
    page(base + tr("/Websuche"), tr("Websuche"));
    note(tr("Womit die KI im Web sucht. SearXNG ist eine freie Metasuchmaschine auf einem eigenen Server (in dessen "
            "settings.yml muss unter search > formats „json“ stehen). Brave Search ist ein Dienst mit eigenem "
            "Schlüssel (api-dashboard.search.brave.com, kostenloses Kontingent)."));
    radios(QStringLiteral("global/web/provider"), tr("Suchdienst:"), {tr("SearXNG (eigener Server)"), tr("Brave Search API")}, false);
    textField(QStringLiteral("global/web/searxng"), tr("SearXNG-Adresse:"));
    if (auto* e = qobject_cast<QLineEdit*>(field(QStringLiteral("global/web/searxng"))))
        e->setMinimumWidth(320);
    textField(QStringLiteral("global/web/searxngKey"), tr("SearXNG-Schlüssel (falls verlangt):"));
    textField(QStringLiteral("global/web/braveKey"), tr("Brave-Schlüssel:"));
    for (const char* k : {"global/web/searxngKey", "global/web/braveKey"})
        if (auto* e = qobject_cast<QLineEdit*>(field(QLatin1String(k)))) {
            e->setEchoMode(QLineEdit::PasswordEchoOnEdit);
            e->setMaximumWidth(320);
        }
    {
        auto* searcher = new WebSearch(this);
        auto* result = new QLabel(m_pageWidget);
        result->setObjectName(QStringLiteral("web-result"));
        result->setWordWrap(true);
        result->hide();
        QWidget* testButton = button(tr("Test"), {});
        testButton->setObjectName(QStringLiteral("web-test"));
        m_layout->addWidget(result);
        connect(static_cast<QPushButton*>(testButton), &QPushButton::clicked, this, [this, searcher, result, testButton] {
            testButton->setEnabled(false);
            result->setStyleSheet(QString());
            result->setText(tr("Prüfe …"));
            result->show();
            QPointer<QLabel> label(result);
            QPointer<QWidget> button(testButton);
            searcher->test(current(QStringLiteral("global/web/provider")).toInt(), current(QStringLiteral("global/web/searxng")).toString(),
                           current(current(QStringLiteral("global/web/provider")).toInt() == 1 ? QStringLiteral("global/web/braveKey")
                                                                                              : QStringLiteral("global/web/searxngKey")).toString(),
                           [label, button](bool ok, const QString& message) {
                               if (button)
                                   button->setEnabled(true);
                               if (!label)
                                   return;
                               label->setStyleSheet(ok ? QStringLiteral("color: #1e7b34;") : QStringLiteral("color: #b3261e;"));
                               label->setText((ok ? QStringLiteral("✔ ") : QStringLiteral("✘ ")) + message);
                           });
        });
    }
}

bool OptionsDialog::editAiAccount(ai::Account& account)
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("KI-Zugang"));
    auto* form = new QFormLayout(&dialog);
    auto* kind = new QComboBox(&dialog);
    kind->setObjectName(QStringLiteral("kind"));
    for (const ai::Kind& k : ai::kinds())
        kind->addItem(k.name, k.key);
    kind->setCurrentIndex(std::max(0, kind->findData(account.kind)));
    form->addRow(tr("Art:"), kind);
    auto* hint = new QLabel(&dialog);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #555;"));
    hint->setMinimumWidth(420);
    form->addRow(hint);
    auto* label = new QLineEdit(account.label, &dialog);
    label->setObjectName(QStringLiteral("label"));
    label->setPlaceholderText(tr("frei, z.B. „Claude privat“"));
    form->addRow(tr("Bezeichnung:"), label);
    auto* key = new QLineEdit(account.key, &dialog);
    key->setObjectName(QStringLiteral("key"));
    key->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    form->addRow(tr("API-Schlüssel:"), key);
    auto* url = new QLineEdit(account.baseUrl, &dialog);
    url->setObjectName(QStringLiteral("url"));
    form->addRow(tr("Adresse:"), url);
    auto* modelRow = new QHBoxLayout;
    auto* model = new QComboBox(&dialog);
    model->setObjectName(QStringLiteral("model"));
    model->setEditable(true);
    model->setMinimumWidth(260);
    auto* load = new QPushButton(tr("Modelle laden"), &dialog);
    modelRow->addWidget(model, 1);
    modelRow->addWidget(load);
    form->addRow(tr("Modell:"), modelRow);
    auto* status = new QLabel(&dialog);
    status->setWordWrap(true);
    form->addRow(status);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto fillModels = [model](const QStringList& ids, const QString& keep) {
        model->clear();
        model->addItems(ids);
        model->setCurrentText(keep);
    };
    auto updateKind = [&, kind, key, url, hint, fillModels](bool first) {
        const ai::Kind* k = ai::kind(kind->currentData().toString());
        hint->setText(k->hint);
        key->setEnabled(k->key != QLatin1String("claude_code"));
        form->setRowVisible(key, k->key != QLatin1String("claude_code"));
        form->setRowVisible(url, k->key != QLatin1String("claude_code"));
        url->setPlaceholderText(k->needsUrl ? tr("z.B. %1").arg(k->baseUrl.isEmpty() ? QStringLiteral("http://server:4000/v1") : k->baseUrl)
                                            : tr("leer = %1").arg(k->baseUrl));
        key->setPlaceholderText(k->needsKey ? QString() : tr("optional"));
        fillModels(k->fallbackModels, first && !account.model.isEmpty() ? account.model : k->defaultModel);
    };
    updateKind(true);
    connect(kind, &QComboBox::currentIndexChanged, &dialog, [updateKind] { updateKind(false); });
    connect(load, &QPushButton::clicked, &dialog, [&, kind, key, url, model, status, fillModels] {
        ai::Account probe{account.id, kind->currentData().toString(), label->text(), url->text(), key->text(), model->currentText()};
        const ai::Kind* k = ai::kind(probe.kind);
        if (probe.kind == QLatin1String("claude_code")) {
            fillModels(k->fallbackModels, model->currentText());
            status->setText(tr("Claude Code nimmt die Kurznamen opus, sonnet, haiku oder eine Modell-ID."));
            return;
        }
        ai::Provider* provider = ai::Provider::create(probe, &dialog);
        status->setText(tr("Lade Modelle …"));
        const QString keep = model->currentText();
        provider->models([provider, status, fillModels, keep](const QStringList& ids, const QString& error) {
            fillModels(ids, keep);
            status->setText(error.isEmpty() ? tr("%n Modelle geladen.", nullptr, int(ids.size())) : error);
            provider->deleteLater();
        });
    });
    if (dialog.exec() != QDialog::Accepted)
        return false;
    account.kind = kind->currentData().toString();
    account.label = label->text().trimmed();
    account.key = account.kind == QLatin1String("claude_code") ? QString() : key->text().trimmed();
    account.baseUrl = account.kind == QLatin1String("claude_code") ? QString() : url->text().trimmed();
    account.model = model->currentText().trimmed();
    return true;
}

// --- Seiten -----------------------------------------------------------------------------------

bool OptionsDialog::selectPage(const QString& path)
{
    QTreeWidgetItem* item = m_items.value(path);
    if (!item)
        return false;
    m_tree->setCurrentItem(item);
    m_stack->setCurrentIndex(item->data(0, Qt::UserRole).toInt());
    return true;
}

QStringList OptionsDialog::pagePaths() const
{
    return m_items.keys();
}

void OptionsDialog::accept()
{
    Options::instance().apply(m_pending);
    for (const auto& apply : std::as_const(m_appliers))
        apply();
    QDialog::accept();
}

} // namespace vek

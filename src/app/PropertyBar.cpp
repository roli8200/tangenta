#include "PropertyBar.h"

#include "Canvas.h"
#include "CurveObject.h"
#include "ColorDialog.h"
#include "Editor.h"
#include "ExpressionSpinBox.h"
#include "ArtisticMediaTool.h"
#include "MediaDocker.h"
#include "MediaLibrary.h"
#include "CutTools.h"
#include "Outline.h"
#include "PathOps.h"
#include "ShapeTool.h"
#include "TextTool.h"
#include "ImageObject.h"
#include "Vectorizer.h"
#include "GradientDialog.h"
#include "Icons.h"
#include "FontBox.h"
#include "MeshTool.h"
#include "PipetteTools.h"
#include "InteractiveTools.h"
#include "Options.h"
#include "EffectTool.h"
#include "EffectDockers.h"
#include "PickTool.h"
#include "PerfectShapeTool.h"
#include "PatternDialog.h"
#include "ColorPalette.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>
#include <tuple>

namespace vek {

namespace {

struct PagePreset {
    const char* name;
    QSizeF size;
};
const PagePreset kPagePresets[] = {
    {"A3", QSizeF(297, 420)}, {"A4", QSizeF(210, 297)}, {"A5", QSizeF(148, 210)},
    {"A6", QSizeF(105, 148)}, {"Letter", QSizeF(215.9, 279.4)}, {"Legal", QSizeF(215.9, 355.6)},
};

// Konturbreiten in mm; 0 = keine Kontur
// Liste
const double kOutlineWidths[] = {0.0, 0.076, 0.1, 0.2, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0, 10.0};
// dazu die üblichen Stufen in Punkt
const double kOutlineWidthsPt[] = {0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0, 8.0, 10.0, 12.0, 16.0, 24.0, 36.0};
constexpr int kUnitPt = 4;

// Konturbreite w (mm) in der Einheit unit
QString formatWidth(double w, int unit = 0)
{
    if (w <= 0.0)
        return PropertyBar::tr("Keine");
    if (std::abs(w - 0.076) < 1e-6)
        return PropertyBar::tr("Haarlinie");
    const double v = w / Options::unitInMm(unit);
    int decimals = v < 0.1 ? 3 : 2;
    if (unit == 3 || unit == 1)
        decimals = 3; // Zoll, cm
    QString text = QLocale().toString(v, 'f', decimals);
    if (unit == kUnitPt || unit == 5) {
        // Punkt/Pixel: unnötige Nullen weg (0,75 pt, 2 pt)
        text = QLocale().toString(std::round(v * 1000.0) / 1000.0, 'g', 6);
    }
    return QStringLiteral("%1 %2").arg(text, Options::unitSymbol(unit));
}

double normalizedCcw(double clockwise)
{
    double a = std::fmod(-clockwise, 360.0);
    if (a < 0)
        a += 360.0;
    if (std::abs(a - 360.0) < 1e-9)
        a = 0.0;
    return a;
}

} // namespace

PropertyBar::PropertyBar(Editor& editor, Canvas& canvas, QWidget* parent)
    : QToolBar(tr("Eigenschaftsleiste"), parent), m_editor(editor), m_canvas(canvas)
{
    setObjectName(QStringLiteral("propertyBar"));
    setMovable(false);
    setIconSize(QSize(20, 20));
    m_frame = new QWidget(this);
    auto* frameLayout = new QHBoxLayout(m_frame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->setSpacing(0);
    addWidget(m_frame);

    // Neu aufbauen nur, wenn sich die Zusammensetzung der Leiste ändert; sonst nur die Werte
    // auffrischen. Der Aufbau kostet ein Vielfaches und fiele sonst bei jedem Klick an.
    for (auto signal : {&Editor::selectionChanged, &Editor::documentReplaced, &Editor::documentChanged})
        connect(&m_editor, signal, this, &PropertyBar::sync);
    connect(&m_editor, &Editor::guideSelectionChanged, this, &PropertyBar::sync);
    connect(&m_canvas, &Canvas::toolChanged, this, &PropertyBar::sync);
    connect(&m_canvas, &Canvas::toolStateChanged, this, &PropertyBar::sync);
    // Neue eigene Striche erscheinen gleich in den Listen
    connect(&MediaLibrary::instance(), &MediaLibrary::changed, this, [this] {
        if (m_canvas.toolId() == ToolId::ArtisticMedia)
            refresh();
    }, Qt::QueuedConnection);
    connect(&m_editor, &Editor::currentPageChanged, this, &PropertyBar::refresh);
    rebuild();
}

QString PropertyBar::layoutSignature() const
{
    if (m_editor.selectedGuide() >= 0)
        return QStringLiteral("guide");
    if (m_canvas.toolId() == ToolId::Crop && m_canvas.cropTool().frame())
        return QStringLiteral("crop");
    if (const ToolId t = m_canvas.toolId(); t == ToolId::Smear || t == ToolId::Smudge || t == ToolId::Roughen || t == ToolId::Twirl
                                            || t == ToolId::Attract || t == ToolId::Repel)
        return QStringLiteral("shaping");
    if (m_canvas.toolId() == ToolId::FreeTransform)
        return QStringLiteral("freetransform");
    if (m_canvas.toolId() == ToolId::SmartFill)
        return QStringLiteral("smartfill");
    if (const ToolId t = m_canvas.toolId(); t >= ToolId::PerfectBasic && t <= ToolId::PerfectCallout)
        return QStringLiteral("perfect:%1").arg(int(t));
    if (m_canvas.toolId() == ToolId::Eraser)
        return QStringLiteral("eraser");
    if (m_canvas.toolId() == ToolId::AttributePipette)
        return QStringLiteral("attrpipette");
    if (m_canvas.toolId() == ToolId::Contour)
        return QStringLiteral("contour");
    if (m_canvas.toolId() == ToolId::Extrude)
        return QStringLiteral("extrude");
    if (m_canvas.toolId() == ToolId::BlockShadow)
        return QStringLiteral("blockshadow");
    if (m_canvas.toolId() == ToolId::Distort)
        return QStringLiteral("distort:%1").arg(Options::instance().integer(QStringLiteral("ws/tools/distort/mode")));
    if (m_canvas.toolId() == ToolId::Envelope)
        return QStringLiteral("envelope");
    if (m_canvas.toolId() == ToolId::DropShadow && !m_editor.selection().isEmpty()) {
        const QList<const DrawObject*> sel = m_editor.selectedObjects();
        return QStringLiteral("shadow:%1").arg(!sel.isEmpty() && sel.first()->shadow().active ? (sel.first()->shadow().perspective ? 2 : 1) : 0);
    }
    if (m_canvas.toolId() == ToolId::ColorPipette)
        return QStringLiteral("colorpipette");
    if (m_canvas.toolId() == ToolId::ArtisticMedia)
        return QStringLiteral("media:%1").arg(int(m_canvas.mediaTool().media().kind));
    const ToolId tool = m_canvas.toolId();
    if (tool == ToolId::Text) {
        if (m_canvas.textTool().editing())
            return QStringLiteral("text:edit");
        if (m_editor.selection().isEmpty())
            return QStringLiteral("text:defaults");
    }
    if (m_editor.selection().isEmpty()
        && (tool == ToolId::Ellipse || tool == ToolId::Polygon || tool == ToolId::Star || tool == ToolId::ComplexStar
            || tool == ToolId::Spiral || tool == ToolId::GraphPaper))
        return QStringLiteral("defaults:%1:%2").arg(int(tool)).arg(int(m_editor.shapeDefaults().ellipseKind));
    const QList<const DrawObject*> objects = m_editor.selectedObjects();
    if (objects.isEmpty())
        return QStringLiteral("page");
    if (m_canvas.toolId() == ToolId::InteractiveFill)
        return QStringLiteral("fill");
    if (tool == ToolId::Mesh)
        return QStringLiteral("mesh");
    if (tool == ToolId::Transparency || tool == ToolId::Blur)
        return QStringLiteral("effect:%1").arg(int(tool));
    if (m_canvas.toolId() == ToolId::Shape && m_canvas.shapeTool().curve())
        return QStringLiteral("nodes");
    // Text an Pfad bekommt eigene Felder
    if (objects.size() == 1)
        if (const auto* t = node_cast<TextObject>(objects.first()); t && t->onPath())
            return QStringLiteral("textpath");
    QString type = objects.first()->typeName();
    for (const DrawObject* o : objects) {
        if (o->typeName() != type)
            type = QStringLiteral("*");
    }
    QString ranges;
    if (objects.size() == 1) {
        for (const PropertyDescriptor& d : objects.first()->propertyDescriptors())
            ranges += QStringLiteral(":%1=%2").arg(d.id).arg(d.maximum);
    }
    // Pfeilspitzen nur bei Linien mit offenen Enden
    bool openEnds = false;
    for (const DrawObject* o : objects) {
        if (const auto* c = dynamic_cast<const CurveObject*>(o); c && !c->media().isActive()) {
            for (const SubPath& s : c->subPaths())
                openEnds = openEnds || !s.closed;
        }
    }
    return QStringLiteral("objects:%1:%2%3%4")
        .arg(type)
        .arg(objects.size() == 1 ? 1 : 2)
        .arg(ranges)
        .arg(openEnds ? QStringLiteral(":ends") : QString());
}

void PropertyBar::sync()
{
    if (layoutSignature() != m_signature)
        rebuild();
    else
        refresh();
}

void PropertyBar::rebuild()
{
    // Jede Zusammensetzung der Leiste wird nur einmal gebaut und danach aufbewahrt. Neue
    // Widgets erst zu gestalten (Stylesheet) kostet ein Vielfaches vom blossen Umschalten.
    m_signature = layoutSignature();
    if (m_host)
        m_host->hide();
    if (auto it = m_sections.find(m_signature); it != m_sections.end()) {
        m_host = it->host;
        m_row = it->row;
        m_refreshers = it->refreshers;
    } else {
        // Zu viele aufbewahrte Leisten (z.B. je Eckenzahl eines Sterns): die anderen verwerfen.
        // Erst später löschen, rebuild kann aus einem Signal genau dieser Widgets kommen.
        if (m_sections.size() >= 32) {
            for (const Section& old : std::as_const(m_sections))
                old.host->deleteLater();
            m_sections.clear();
        }
        m_host = new QWidget(m_frame);
        m_row = new QHBoxLayout(m_host);
        m_row->setContentsMargins(4, 4, 4, 4); // Luft über und unter den gestapelten Feldern
        m_row->setSpacing(3);
        m_refreshers.clear();
        buildSections();
        m_row->addStretch(1);
        m_frame->layout()->addWidget(m_host);
        m_sections.insert(m_signature, Section{m_host, m_row, m_refreshers});
    }
    m_host->show();
    // Die Leiste behält ihre grösste Höhe, sonst springt die Zeichenfläche bei jedem Wechsel
    m_frame->setMinimumHeight(std::max(m_frame->minimumHeight(), m_host->sizeHint().height()));
    refresh();
}

void PropertyBar::buildSections()
{
    const QList<const DrawObject*> objects = m_editor.selectedObjects();
    if (m_signature == QLatin1String("guide")) {
        buildGuideSection();
    } else if (m_signature == QLatin1String("fill")) {
        buildFillSection();
    } else if (m_signature == QLatin1String("mesh")) {
        buildMeshSection();
    } else if (m_signature.startsWith(QLatin1String("shadow:"))) {
        buildShadowSection();
    } else if (m_signature == QLatin1String("contour") || m_signature == QLatin1String("extrude") || m_signature == QLatin1String("blockshadow")
               || m_signature.startsWith(QLatin1String("distort:")) || m_signature == QLatin1String("envelope")) {
        buildInteractiveSection();
    } else if (m_signature == QLatin1String("shaping")) {
        // Spitze der Formgebungswerkzeuge
        QDoubleSpinBox* r = addField(m_row, QStringLiteral("tool-smear"), tr("Grösse der Spitze (Radius)"), QStringLiteral(" mm"), 1,
                                     [] { return Options::instance().number(QStringLiteral("ws/tools/shaping/radius")); },
                                     [this](double v) {
                                         Options::instance().set(QStringLiteral("ws/tools/shaping/radius"), v);
                                         m_canvas.update();
                                     });
        r->setRange(0.1, 500);
        QDoubleSpinBox* k = addField(m_row, QStringLiteral("blend-steps"), tr("Stärke"), QStringLiteral(" %"), 0,
                                     [] { return Options::instance().number(QStringLiteral("ws/tools/shaping/strength")); },
                                     [](double v) { Options::instance().set(QStringLiteral("ws/tools/shaping/strength"), v); });
        k->setRange(1, 100);
        auto* hint = new QLabel(tr("Über die ausgewählten Objekte ziehen bzw. die Maustaste halten"), m_host);
        hint->setEnabled(false);
        m_row->addWidget(hint);
    } else if (m_signature == QLatin1String("textpath")) {
        // Wie die Eigenschaftsleiste "Text an Pfad": Abstand vom Pfad, Versatz, Seite spiegeln, begradigen
        auto text = [this]() -> const TextObject* {
            const QList<const DrawObject*> sel = m_editor.selectedObjects();
            return sel.isEmpty() ? nullptr : node_cast<TextObject>(sel.first());
        };
        QDoubleSpinBox* dist = addField(m_row, QStringLiteral("contour-offset"), tr("Abstand vom Pfad"), QStringLiteral(" mm"), 2,
                                        [text] { return text() ? text()->pathDistance() : 0.0; },
                                        [this](double v) { m_editor.setTextPathParams(std::nullopt, v, std::nullopt); });
        dist->setRange(-1000, 1000);
        QDoubleSpinBox* off = addField(m_row, QStringLiteral("blend-steps"), tr("Versatz entlang des Pfads"), QStringLiteral(" mm"), 2,
                                       [text] { return text() ? text()->pathOffset() : 0.0; },
                                       [this](double v) { m_editor.setTextPathParams(v, std::nullopt, std::nullopt); });
        off->setRange(-10000, 10000);
        QToolButton* flip = addButton(QStringLiteral("text-path-flip"), tr("Seite spiegeln (Text auf die andere Seite)"), {});
        flip->setCheckable(true);
        connect(flip, &QToolButton::toggled, this, [this](bool on) { m_editor.setTextPathParams(std::nullopt, std::nullopt, on); });
        m_refreshers.append([flip, text] {
            const QSignalBlocker b(flip);
            flip->setChecked(text() && text()->pathFlip());
        });
        addSeparator();
        addButton(QStringLiteral("to-curves"), tr("Text begradigen"), [this] { m_editor.straightenSelectedText(); });
        addButton(QStringLiteral("to-curves"), tr("In Kurven konvertieren (Strg+Q)"), [this] { m_editor.convertSelectionToCurves(); });
    } else if (m_signature.startsWith(QLatin1String("perfect:"))) {
        // Perfekte Formen: Auswahl der Form (gilt für neue und für eine ausgewählte Form)
        const auto* tool = static_cast<const PerfectShapeTool*>(m_canvas.currentTool());
        const perfect::Category cat = tool->category();
        auto* shapes = new QComboBox(m_host);
        shapes->setToolTip(perfect::categoryName(cat));
        shapes->setIconSize(QSize(32, 24));
        const QList<int> kinds = perfect::kinds(cat);
        for (int k : kinds)
            shapes->addItem(PerfectShapeTool::icon(k), perfect::info(k).name, k);
        shapes->setCurrentIndex(std::max(0, int(kinds.indexOf(PerfectShapeTool::currentKind(cat)))));
        m_row->addWidget(shapes);
        connect(shapes, &QComboBox::activated, this, [this, shapes, cat](int i) {
            const int kind = shapes->itemData(i).toInt();
            PerfectShapeTool::setCurrentKind(cat, kind);
            const QList<const DrawObject*> sel = m_editor.selectedObjects();
            if (sel.size() == 1)
                if (const auto* s = node_cast<PerfectShapeObject>(sel.first()); s && perfect::info(s->kind()).category == cat)
                    m_editor.setObjectProperty(QStringLiteral("shape"), int(perfect::kinds(cat).indexOf(kind)), tr("Form"));
        });
        auto* hint = new QLabel(tr("Ziehen erstellt die Form; der rote Griff verändert sie"), m_host);
        hint->setEnabled(false);
        m_row->addWidget(hint);
    } else if (m_signature == QLatin1String("smartfill")) {
        // Smart-Füllung: Füllung und Kontur der neuen Fläche (Vorgabe, Angeben, Keine), Lücken
        auto modeBox = [this](const QString& key, const QString& tip) {
            auto* box = new QComboBox(m_host);
            box->setToolTip(tip);
            box->setObjectName(key.section(QLatin1Char('/'), -1)); // fillMode, outlineMode
            box->addItems({tr("Vorgabe"), tr("Angeben"), tr("Keine")});
            box->setCurrentIndex(std::clamp(Options::instance().integer(key), 0, 2));
            m_row->addWidget(box);
            return box;
        };
        m_row->addWidget(new QLabel(tr("Füllung:"), m_host));
        QComboBox* fillMode = modeBox(QStringLiteral("ws/tools/smartFill/fillMode"), tr("Füllung der neuen Fläche"));
        auto* fill = new ColorButton(tr("Füllfarbe der Smart-Füllung"), m_host);
        fill->setToolTip(tr("Füllfarbe"));
        fill->setColor(Color::fromHex(Options::instance().text(QStringLiteral("ws/tools/smartFill/fill"))).value_or(Color(255, 210, 77)));
        fill->setEnabled(fillMode->currentIndex() == 1);
        connect(fill, &ColorButton::colorChanged, this,
                [](const Color& c) { Options::instance().set(QStringLiteral("ws/tools/smartFill/fill"), c.toHex()); });
        connect(fillMode, &QComboBox::activated, this, [fill](int i) {
            Options::instance().set(QStringLiteral("ws/tools/smartFill/fillMode"), i);
            fill->setEnabled(i == 1);
        });
        m_row->addWidget(fill);
        addSeparator();
        m_row->addWidget(new QLabel(tr("Kontur:"), m_host));
        QComboBox* outlineMode = modeBox(QStringLiteral("ws/tools/smartFill/outlineMode"), tr("Kontur der neuen Fläche"));
        QDoubleSpinBox* width = addField(m_row, QStringLiteral("tool-outline"), tr("Konturbreite"), QStringLiteral(" mm"), 3,
                                         [] { return Options::instance().number(QStringLiteral("ws/tools/smartFill/outlineWidth")); },
                                         [](double v) { Options::instance().set(QStringLiteral("ws/tools/smartFill/outlineWidth"), v); });
        width->setRange(0.0, 100.0);
        auto* outline = new ColorButton(tr("Konturfarbe der Smart-Füllung"), m_host);
        outline->setToolTip(tr("Konturfarbe"));
        outline->setColor(Color::fromHex(Options::instance().text(QStringLiteral("ws/tools/smartFill/outlineColor"))).value_or(Color(0, 0, 0)));
        connect(outline, &ColorButton::colorChanged, this,
                [](const Color& c) { Options::instance().set(QStringLiteral("ws/tools/smartFill/outlineColor"), c.toHex()); });
        m_row->addWidget(outline);
        width->setEnabled(outlineMode->currentIndex() == 1);
        outline->setEnabled(outlineMode->currentIndex() == 1);
        connect(outlineMode, &QComboBox::activated, this, [width, outline](int i) {
            Options::instance().set(QStringLiteral("ws/tools/smartFill/outlineMode"), i);
            width->setEnabled(i == 1);
            outline->setEnabled(i == 1);
        });
        addSeparator();
        m_row->addWidget(new QLabel(tr("Lücke:"), m_host));
        QDoubleSpinBox* gap = addField(m_row, QString(), tr("Lücken bis zu dieser Breite gelten als geschlossen"), QStringLiteral(" mm"), 2,
                                       [] { return Options::instance().number(QStringLiteral("ws/tools/smartFill/gap")); },
                                       [](double v) { Options::instance().set(QStringLiteral("ws/tools/smartFill/gap"), v); });
        gap->setRange(0.0, 10.0);
        auto* hint = new QLabel(tr("In eine umschlossene Fläche klicken"), m_host);
        hint->setEnabled(false);
        m_row->addWidget(hint);
    } else if (m_signature == QLatin1String("freetransform")) {
        auto* group = new QButtonGroup(m_host);
        const QStringList icons{QStringLiteral("ft-rotate"), QStringLiteral("ft-mirror"), QStringLiteral("ft-scale"), QStringLiteral("ft-skew")};
        const QStringList texts{tr("Freies Drehen"), tr("Freies Spiegeln"), tr("Freies Skalieren"), tr("Freies Neigen")};
        const int mode = Options::instance().integer(QStringLiteral("ws/tools/freeTransform/mode"));
        for (int i = 0; i < 4; ++i) {
            QToolButton* b = addButton(icons[i], texts[i], {});
            b->setCheckable(true);
            b->setChecked(i == mode);
            group->addButton(b, i);
        }
        connect(group, &QButtonGroup::idClicked, this, [](int i) { Options::instance().set(QStringLiteral("ws/tools/freeTransform/mode"), i); });
        auto* hint = new QLabel(tr("Klick setzt den Drehpunkt, Ziehen transformiert die Auswahl"), m_host);
        hint->setEnabled(false);
        m_row->addWidget(hint);
    } else if (m_signature == QLatin1String("attrpipette")) {
        // Wie die Eigenschaftsleiste: welche Attribute die Pipette überträgt
        for (const auto& [key, iconName, text] : {std::tuple{QStringLiteral("ws/tools/attrPipette/properties"), QStringLiteral("attr-properties"), tr("Eigenschaften (Füllung, Kontur, Text)")},
                                                  std::tuple{QStringLiteral("ws/tools/attrPipette/transform"), QStringLiteral("attr-transform"), tr("Transformationen (Grösse, Drehung)")},
                                                  std::tuple{QStringLiteral("ws/tools/attrPipette/effects"), QStringLiteral("attr-effects"), tr("Effekte (Transparenz, Unschärfe)")}}) {
            QToolButton* b = addButton(iconName, text, {});
            b->setCheckable(true);
            b->setChecked(Options::instance().flag(key));
            connect(b, &QToolButton::toggled, this, [key](bool on) { Options::instance().set(key, on); });
        }
        auto* hint = new QLabel(tr("Objekt anklicken = Attribute aufnehmen, danach Klick = übertragen (Strg = neu aufnehmen, Esc)"), m_host);
        hint->setEnabled(false);
        m_row->addWidget(hint);
    } else if (m_signature == QLatin1String("colorpipette")) {
        auto* swatch = new QLabel(m_host);
        swatch->setFixedSize(28, 18);
        m_row->addWidget(swatch);
        auto* hint = new QLabel(m_host);
        hint->setEnabled(false);
        m_row->addWidget(hint);
        m_refreshers.append([this, swatch, hint] {
            const auto* pip = static_cast<const ColorPipetteTool*>(m_canvas.currentTool());
            const std::optional<Color> c = pip ? pip->color() : std::nullopt;
            swatch->setStyleSheet(c ? QStringLiteral("background:%1;border:1px solid #555;").arg(c->toQColor().name())
                                    : QStringLiteral("border:1px dashed #999;"));
            hint->setText(c ? tr("%1 – Klick ins Objekt: Füllung, auf den Rand oder mit Umschalt: Kontur (Strg = neu aufnehmen)").arg(c->toHex())
                            : tr("Klick auf die Zeichnung nimmt die Farbe auf"));
        });
        // Wie die Eigenschaftsleiste: Auswahl vom Desktop, zur Palette hinzufügen
        addButton(QStringLiteral("eyedropper"), tr("Auswahl vom Desktop (Farbe irgendwo auf dem Bildschirm aufnehmen)"), [this] {
            if (auto* pip = dynamic_cast<ColorPipetteTool*>(m_canvas.currentTool()))
                pip->pickFromDesktop();
        });
        QToolButton* add = addButton(QStringLiteral("color-palettes"), tr("Zur Palette hinzufügen"), [this] {
            if (const auto* pip = dynamic_cast<const ColorPipetteTool*>(m_canvas.currentTool()); pip && pip->color())
                ColorPalette::addCustomColor(*pip->color());
        });
        m_refreshers.append([this, add] {
            const auto* pip = dynamic_cast<const ColorPipetteTool*>(m_canvas.currentTool());
            add->setEnabled(pip && pip->color());
        });
    } else if (m_signature.startsWith(QLatin1String("effect:"))) {
        buildEffectSection(m_canvas.toolId() == ToolId::Blur ? Effect::Blur : Effect::Transparency);
    } else if (m_signature == QLatin1String("nodes")) {
        buildNodeSection();
    } else if (m_signature == QLatin1String("crop")) {
        buildCropSection();
    } else if (m_signature == QLatin1String("eraser")) {
        buildEraserSection();
    } else if (m_signature.startsWith(QLatin1String("media:"))) {
        buildMediaSection();
    } else if (m_signature.startsWith(QLatin1String("defaults:"))) {
        buildShapeDefaultsSection();
    } else if (m_signature.startsWith(QLatin1String("text:"))) {
        buildTextSection();
    } else if (m_signature.startsWith(QLatin1String("objects:image:1"))) {
        // Bitmap wie in der Eigenschaftsleiste
        buildGeometrySection();
        buildImageSection();
        addSeparator();
        buildBlurSlider();
    } else if (m_signature.startsWith(QLatin1String("objects:text:"))) {
        // Text wie in der Eigenschaftsleiste: Lage und Grösse, dann Schrift und Absatz
        buildGeometrySection();
        buildTextSection();
        addSeparator();
        buildBlurSlider();
        addSeparator();
        QToolButton* toCurves = addButton(QStringLiteral("to-curves"), tr("In Kurven konvertieren (Strg+Q)"),
                                          [this] { m_editor.convertSelectionToCurves(); });
        m_refreshers.append([this, toCurves] { toCurves->setEnabled(m_editor.canConvertSelectionToCurves()); });
    } else if (objects.isEmpty()) {
        buildPageSection();
    } else {
        buildGeometrySection();
        if (!m_signature.contains(QLatin1String(":*:")))
            buildTypeSection(objects.first()->propertyDescriptors());
        if (objects.size() >= 2) {
            addSeparator();
            addButton(QStringLiteral("align-dialog"), tr("Ausrichten und verteilen (Strg+Umschalt+A)"),
                      [this] { emit alignDialogRequested(); });
            buildShapingSection();
        }
        buildOutlineSection();
    }
}

void PropertyBar::refresh()
{
    for (const auto& r : std::as_const(m_refreshers))
        r();
}

// --- Bausteine -----------------------------------------------------------------------------

void PropertyBar::addSpinButtons(QDoubleSpinBox* spin, double step)
{
    spin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    spin->setSingleStep(step);
    spin->setFixedWidth(spin->width() + 16);
}

QBoxLayout* PropertyBar::addStack()
{
    auto* stack = new QVBoxLayout;
    stack->setSpacing(4);
    stack->setContentsMargins(0, 0, 0, 0);
    m_row->addLayout(stack);
    return stack;
}

QDoubleSpinBox* PropertyBar::addField(QBoxLayout* into, const QString& iconName, const QString& tooltip,
                                      const QString& suffix, int decimals, std::function<double()> get,
                                      std::function<void(double)> set)
{
    // Vorne ein Symbol oder ein kurzes "x:", Millimeter im Feld,
    // Prozent und Grad als Zeichen rechts daneben
    auto* line = new QHBoxLayout;
    line->setSpacing(3);
    if (iconName.endsWith(QLatin1Char(':'))) {
        auto* label = new QLabel(iconName, m_host);
        label->setFixedWidth(14);
        label->setToolTip(tooltip);
        line->addWidget(label);
    } else if (!iconName.isEmpty()) {
        auto* label = new QLabel(m_host);
        label->setPixmap(icon(iconName).pixmap(16));
        label->setFixedWidth(16);
        label->setToolTip(tooltip);
        line->addWidget(label);
    }
    const bool unitOutside = suffix == QStringLiteral(" %") || suffix == QStringLiteral(" °");
    auto* spin = new ExpressionSpinBox(m_host);
    spin->setToolTip(tooltip + tr(" – Rechnen erlaubt, z. B. 297/2"));
    spin->setDecimals(decimals);
    spin->setRange(-100000.0, 100000.0);
    spin->setSuffix(unitOutside ? QString() : suffix);
    spin->setKeyboardTracking(false);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setFixedWidth(unitOutside ? 52 : 84);
    // Längen in der Dokumenteinheit (Einheiten-Liste ohne Auswahl, Lineale)
    if (suffix == QStringLiteral(" mm"))
        spin->followDocumentUnit();
    // Übereinander gestapelte Felder (Lage, Grösse, Eckenradien) flacher, damit zwei Zeilen
    // nicht höher werden als nötig
    spin->setProperty("stacked", into != m_row);
    line->addWidget(spin);
    if (unitOutside)
        line->addWidget(new QLabel(suffix.trimmed(), m_host));
    into->addLayout(line);

    // Enter, Verlassen und jeder Pfeilschritt (ExpressionSpinBox::stepBy) übernehmen den Wert
    connect(spin, &QDoubleSpinBox::editingFinished, this, [spin, get, set] {
        if (std::abs(spin->value() - get()) > 1e-9)
            set(spin->value());
    });
    m_refreshers.append([spin, get] {
        const QSignalBlocker block(spin);
        spin->setValue(get());
    });
    return spin;
}

QToolButton* PropertyBar::addButton(const QString& iconName, const QString& tooltip, std::function<void()> onClick)
{
    auto* b = new QToolButton(m_host);
    b->setIcon(icon(iconName));
    b->setIconSize(QSize(18, 18));
    b->setToolTip(tooltip);
    b->setAutoRaise(true);
    if (onClick)
        connect(b, &QToolButton::clicked, this, std::move(onClick));
    m_row->addWidget(b);
    return b;
}

void PropertyBar::addSeparator()
{
    auto* line = new QFrame(m_host);
    line->setObjectName(QStringLiteral("barSeparator"));
    line->setFixedSize(1, 36);
    m_row->addSpacing(5);
    m_row->addWidget(line);
    m_row->addSpacing(5);
}

// --- Abschnitte ----------------------------------------------------------------------------

void PropertyBar::buildPageSection()
{
    auto* preset = new QComboBox(m_host);
    preset->setToolTip(tr("Papierformat"));
    for (const PagePreset& p : kPagePresets)
        preset->addItem(QString::fromLatin1(p.name), p.size);
    preset->addItem(tr("Benutzerdefiniert"));
    m_row->addWidget(preset);
    auto pageSize = [this] { return m_editor.currentPage().size(); };
    auto apply = [this](const QSizeF& s) {
        m_editor.setPageSize(s, m_editor.pageSizeForAllPages());
        m_canvas.fitPage();
    };
    connect(preset, &QComboBox::activated, this, [=, this](int index) {
        const QVariant v = preset->itemData(index);
        if (!v.isValid())
            return;
        QSizeF s = v.toSizeF();
        if (pageSize().width() > pageSize().height())
            s.transpose();
        apply(s);
    });
    m_refreshers.append([=, this] {
        const QSizeF s = pageSize();
        const QSizeF portrait = s.width() > s.height() ? s.transposed() : s;
        int found = preset->count() - 1;
        for (int i = 0; i < preset->count() - 1; ++i) {
            const QSizeF p = preset->itemData(i).toSizeF();
            if (std::abs(p.width() - portrait.width()) < 0.05 && std::abs(p.height() - portrait.height()) < 0.05)
                found = i;
        }
        const QSignalBlocker block(preset);
        preset->setCurrentIndex(found);
    });

    QBoxLayout* stack = addStack();
    addSpinButtons(addField(stack, QStringLiteral("width"), tr("Seitenbreite"), QStringLiteral(" mm"), 1,
                            [=, this] { return pageSize().width(); },
                            [=, this](double v) { apply(QSizeF(v, pageSize().height())); }),
                   1.0);
    addSpinButtons(addField(stack, QStringLiteral("height"), tr("Seitenhöhe"), QStringLiteral(" mm"), 1,
                            [=, this] { return pageSize().height(); },
                            [=, this](double v) { apply(QSizeF(pageSize().width(), v)); }),
                   1.0);
    addSeparator();

    QToolButton* portrait = addButton(QStringLiteral("page-portrait"), tr("Hochformat"), [=, this] {
        const QSizeF s = pageSize();
        if (s.width() > s.height())
            apply(s.transposed());
    });
    QToolButton* landscape = addButton(QStringLiteral("page-landscape"), tr("Querformat"), [=, this] {
        const QSizeF s = pageSize();
        if (s.height() > s.width())
            apply(s.transposed());
    });
    portrait->setCheckable(true);
    landscape->setCheckable(true);
    m_refreshers.append([=, this] {
        const bool isLandscape = pageSize().width() > pageSize().height();
        portrait->setChecked(!isLandscape);
        landscape->setChecked(isLandscape);
    });


    // Seitengrösse für alle Seiten oder nur für die aktuelle
    QToolButton* all = addButton(QStringLiteral("pages-all"), tr("Alle Seiten"), [this] {
        m_editor.setPageSizeForAllPages(true);
        refresh();
    });
    QToolButton* current = addButton(QStringLiteral("page-current"), tr("Aktuelle Seite"), [this] {
        m_editor.setPageSizeForAllPages(false);
        refresh();
    });
    all->setCheckable(true);
    current->setCheckable(true);
    m_refreshers.append([=, this] {
        all->setChecked(m_editor.pageSizeForAllPages());
        current->setChecked(!m_editor.pageSizeForAllPages());
    });

    addSeparator();
    m_row->addWidget(new QLabel(tr("Einheiten:"), m_host));
    // Einheit des Dokuments: Lineale und alle Längenfelder (Lage, Grösse, Abstände, Kontur)
    auto* units = new QComboBox(m_host);
    units->setObjectName(QStringLiteral("documentUnits"));
    units->addItems(Options::unitNames());
    units->setToolTip(tr("Masseinheit für Lineale und Zahlenfelder"));
    m_row->addWidget(units);
    connect(units, &QComboBox::activated, this, [](int index) {
        Options::instance().apply({{QStringLiteral("doc/rulers/units"), index}, {QStringLiteral("doc/rulers/unitsV"), index}});
    });
    m_refreshers.append([units] {
        const QSignalBlocker block(units);
        units->setCurrentIndex(Options::instance().rulerUnit(true));
    });

    addSeparator();
    QDoubleSpinBox* nudge = addField(m_row, QStringLiteral("nudge"), tr("Verschiebungsabstand (Pfeiltasten)"),
                                     QStringLiteral(" mm"), 2, [this] { return m_editor.nudge(); },
                                     [this](double v) { m_editor.setNudge(v); });
    nudge->setMinimum(0.01);
    addSpinButtons(nudge, 0.1);

    addSeparator();
    QBoxLayout* dup = addStack();
    addSpinButtons(addField(dup, QStringLiteral("duplicate-x"), tr("Duplikatabstand waagrecht"), QStringLiteral(" mm"),
                            1, [this] { return m_editor.duplicateOffset().x(); },
                            [this](double v) { m_editor.setDuplicateOffset(QPointF(v, m_editor.duplicateOffset().y())); }),
                   1.0);
    addSpinButtons(addField(dup, QStringLiteral("duplicate-y"), tr("Duplikatabstand senkrecht"), QStringLiteral(" mm"),
                            1, [this] { return m_editor.duplicateOffset().y(); },
                            [this](double v) { m_editor.setDuplicateOffset(QPointF(m_editor.duplicateOffset().x(), v)); }),
                   1.0);
}

void PropertyBar::buildCropSection()
{
    // Lage und Grösse des Zuschneiderahmens
    CropTool& crop = m_canvas.cropTool();
    auto frame = [&crop] { return crop.frame().value_or(QRectF()); };
    auto setFrame = [&crop](const QRectF& r) { crop.setFrame(r.normalized()); };
    QBoxLayout* pos = addStack();
    addField(pos, QStringLiteral("x:"), tr("Zuschneiderahmen links"), QStringLiteral(" mm"), 3,
             [=] { return frame().left(); }, [=](double v) { setFrame(frame().translated(v - frame().left(), 0)); });
    addField(pos, QStringLiteral("y:"), tr("Zuschneiderahmen oben"), QStringLiteral(" mm"), 3,
             [=] { return frame().top(); }, [=](double v) { setFrame(frame().translated(0, v - frame().top())); });
    QBoxLayout* size = addStack();
    addField(size, QStringLiteral("width"), tr("Breite des Zuschneiderahmens"), QStringLiteral(" mm"), 3,
             [=] { return frame().width(); }, [=](double v) {
                 QRectF r = frame();
                 r.setWidth(std::max(0.01, v));
                 setFrame(r);
             });
    addField(size, QStringLiteral("height"), tr("Höhe des Zuschneiderahmens"), QStringLiteral(" mm"), 3,
             [=] { return frame().height(); }, [=](double v) {
                 QRectF r = frame();
                 r.setHeight(std::max(0.01, v));
                 setFrame(r);
             });
    addSeparator();
    auto* clear = new QToolButton(m_host);
    clear->setText(tr("Zuschneide-Markierungsrahmen löschen"));
    clear->setAutoRaise(true);
    connect(clear, &QToolButton::clicked, this, [&crop] { crop.setFrame(std::nullopt); });
    m_row->addWidget(clear);
    auto* hint = new QLabel(tr("Doppelklick in den Rahmen oder Eingabe schneidet zu"), m_host);
    hint->setEnabled(false);
    m_row->addWidget(hint);
}

void PropertyBar::buildInteractiveSection()
{
    auto selected = [this]() -> const DrawObject* {
        const QList<const DrawObject*> sel = m_editor.selectedObjects();
        return sel.size() == 1 ? sel.first() : nullptr;
    };
    if (m_signature == QLatin1String("contour")) {
        auto current = [this, selected] {
            ContourEffect c = selected() && selected()->contour().active ? selected()->contour() : ContourEffect();
            if (!c.active)
                c.steps = Options::instance().integer(QStringLiteral("ws/tools/contour/steps"));
            return c;
        };
        auto change = [this, current](const std::function<void(ContourEffect&)>& fn) {
            ContourEffect c = current();
            c.active = true;
            fn(c);
            m_editor.setSelectionContour(c, tr("Kontur"));
        };
        auto* group = new QButtonGroup(m_host);
        const QStringList icons{QStringLiteral("contour-center"), QStringLiteral("contour-inside"), QStringLiteral("contour-outside")};
        const QStringList texts{tr("Zur Mitte"), tr("Konturlinie innen"), tr("Konturlinie aussen")};
        for (int i = 0; i < 3; ++i) {
            QToolButton* b = addButton(icons[i], texts[i], {});
            b->setCheckable(true);
            group->addButton(b, i);
        }
        connect(group, &QButtonGroup::idClicked, this, [change](int i) { change([i](ContourEffect& c) { c.direction = ContourEffect::Direction(i); }); });
        m_refreshers.append([group, current] {
            if (QAbstractButton* b = group->button(int(current().direction)))
                b->setChecked(true);
        });
        addSeparator();
        QDoubleSpinBox* steps = addField(m_row, QStringLiteral("blend-steps"), tr("Konturschritte"), QString(), 0,
                                         [current] { return double(current().steps); }, [change](double v) {
                                             Options::instance().set(QStringLiteral("ws/tools/contour/steps"), int(v));
                                             change([v](ContourEffect& c) { c.steps = int(v); });
                                         });
        steps->setRange(1, 999);
        QDoubleSpinBox* offset = addField(m_row, QStringLiteral("contour-offset"), tr("Konturabstand"), QStringLiteral(" mm"), 2,
                                          [current] { return current().offset; }, [change](double v) { change([v](ContourEffect& c) { c.offset = v; }); });
        offset->setRange(0.01, 500);
        addSeparator();
        addButton(QStringLiteral("fill-uniform"), tr("Füllfarbe"), [this, current, change] {
            if (const std::optional<Color> col = ColorDialog::getColor(m_host, tr("Füllfarbe der Kontur"), current().endFill))
                change([col](ContourEffect& c) { c.endFill = *col; });
        });
        addButton(QStringLiteral("tool-outline"), tr("Umrissfarbe"), [this, current, change] {
            if (const std::optional<Color> col = ColorDialog::getColor(m_host, tr("Umrissfarbe der Kontur"), current().endStroke))
                change([col](ContourEffect& c) { c.endStroke = *col; });
        });
        addSeparator();
        addButton(QStringLiteral("fx-clear"), tr("Kontur löschen"), [this] { m_editor.setSelectionContour(ContourEffect(), tr("Kontur löschen")); });
        return;
    }
    if (m_signature == QLatin1String("blockshadow")) {
        // Blockschatten: Tiefe, Richtung, Farbe, Vergrössern, Vereinfachen, Löcher entfernen
        auto current = [selected] {
            if (selected() && selected()->blockShadow().active)
                return selected()->blockShadow();
            BlockShadow s;
            if (const std::optional<Color> c = Color::fromHex(Options::instance().text(QStringLiteral("ws/tools/blockShadow/color"))))
                s.color = *c;
            return s;
        };
        auto change = [this, current](const std::function<void(BlockShadow&)>& fn) {
            BlockShadow s = current();
            s.active = true;
            fn(s);
            m_editor.setSelectionBlockShadow(s, tr("Blockschatten"));
        };
        QDoubleSpinBox* depth = addField(m_row, QStringLiteral("extrude-depth"), tr("Tiefe des Blockschattens"), QStringLiteral(" mm"), 2,
                                         [current] { return current().depth(); },
                                         [change](double v) { change([v](BlockShadow& s) { s.setPolar(v, s.angle()); }); });
        depth->setRange(0, 10000);
        QDoubleSpinBox* angle = addField(m_row, QStringLiteral("shadow-angle"), tr("Richtung des Blockschattens"), QStringLiteral(" °"), 1,
                                         [current] { return current().angle(); },
                                         [change](double v) { change([v](BlockShadow& s) { s.setPolar(s.depth(), v); }); });
        angle->setRange(-360, 360);
        addButton(QStringLiteral("shadow-color"), tr("Farbe des Blockschattens"), [this, current, change] {
            if (const std::optional<Color> col = ColorDialog::getColor(m_host, tr("Farbe des Blockschattens"), current().color)) {
                Options::instance().set(QStringLiteral("ws/tools/blockShadow/color"), col->toHex());
                change([col](BlockShadow& s) { s.color = *col; });
            }
        });
        addSeparator();
        QDoubleSpinBox* expand = addField(m_row, QStringLiteral("contour-offset"), tr("Blockschatten vergrössern"), QStringLiteral(" mm"), 2,
                                          [current] { return current().expand; },
                                          [change](double v) { change([v](BlockShadow& s) { s.expand = v; }); });
        expand->setRange(0, 500);
        auto toggle = [this, current, change](const QString& iconName, const QString& tip, bool BlockShadow::*member) {
            QToolButton* b = addButton(iconName, tip, {});
            b->setCheckable(true);
            connect(b, &QToolButton::toggled, this, [change, member](bool on) { change([on, member](BlockShadow& s) { s.*member = on; }); });
            m_refreshers.append([b, current, member] {
                const QSignalBlocker block(b);
                b->setChecked(current().*member);
            });
        };
        toggle(QStringLiteral("shape-simplify"), tr("Vereinfachen: Teil unter dem Objekt weglassen"), &BlockShadow::simplify);
        toggle(QStringLiteral("shape-boundary"), tr("Löcher entfernen"), &BlockShadow::removeHoles);
        toggle(QStringLiteral("outline-width"), tr("Kontur des Objekts einbeziehen"), &BlockShadow::useOutline);
        addSeparator();
        addButton(QStringLiteral("break-apart"), tr("Blockschatten trennen (Strg+K)"), [this] { m_editor.separateEffects(); });
        addButton(QStringLiteral("fx-clear"), tr("Blockschatten löschen"),
                  [this] { m_editor.setSelectionBlockShadow(BlockShadow(), tr("Blockschatten löschen")); });
        return;
    }
    if (m_signature == QLatin1String("extrude")) {
        auto current = [selected] { return selected() && selected()->extrusion().active ? selected()->extrusion() : Extrusion(); };
        auto change = [this, current](const std::function<void(Extrusion&)>& fn) {
            Extrusion e = current();
            e.active = true;
            fn(e);
            m_editor.setSelectionExtrusion(e, tr("Extrusion"));
        };
        auto* type = new QComboBox(m_host);
        type->addItems({tr("Klein hinten (Fluchtpunkt)"), tr("Parallel hinten")});
        type->setToolTip(tr("Extrusionstyp"));
        m_row->addWidget(type);
        connect(type, &QComboBox::activated, this, [change](int i) { change([i](Extrusion& e) { e.parallel = i == 1; }); });
        m_refreshers.append([type, current] {
            const QSignalBlocker b(type);
            type->setCurrentIndex(current().parallel ? 1 : 0);
        });
        QDoubleSpinBox* depth = addField(m_row, QStringLiteral("extrude-depth"), tr("Extrusionstiefe"), QStringLiteral(" %"), 0,
                                         [current] { return current().depth; }, [change](double v) { change([v](Extrusion& e) { e.depth = v; }); });
        depth->setRange(1, 99);
        addButton(QStringLiteral("fill-uniform"), tr("Extrusionsfarbe"), [this, current, change] {
            if (const std::optional<Color> col = ColorDialog::getColor(m_host, tr("Extrusionsfarbe"), current().color))
                change([col](Extrusion& e) { e.color = *col; });
        });
        QToolButton* shade = addButton(QStringLiteral("fx-bevel"), tr("Beleuchtung (Schattierung der Seiten)"), {});
        shade->setCheckable(true);
        connect(shade, &QToolButton::toggled, this, [change](bool on) { change([on](Extrusion& e) { e.shade = on; }); });
        m_refreshers.append([shade, current] {
            const QSignalBlocker b(shade);
            shade->setChecked(current().shade);
        });
        addSeparator();
        // Drehung im Raum: Kugel zum Ziehen und Winkel x/y/z
        auto rotateChange = [this, current](const std::function<void(Extrusion&)>& fn) {
            Extrusion e = current();
            e.active = true;
            fn(e);
            m_editor.setSelectionExtrusion(e, tr("Extrusion drehen"));
        };
        auto* ball = new ExtrusionTrackball(m_host);
        ball->setToolTip(tr("Extrusion drehen: Kugel ziehen (Umschalt: um die Blickachse)"));
        m_row->addWidget(ball);
        connect(ball, &ExtrusionTrackball::rotationChanged, this, [rotateChange](double x, double y, double z) {
            rotateChange([x, y, z](Extrusion& e) {
                e.rotX = x;
                e.rotY = y;
                e.rotZ = z;
            });
        });
        m_refreshers.append([ball, current] { ball->setRotation(current().rotX, current().rotY, current().rotZ); });
        QBoxLayout* angles = addStack();
        QDoubleSpinBox* rx = addField(angles, tr("X:"), tr("Drehung um die waagrechte Achse (kippen)"), QStringLiteral(" °"), 1,
                                      [current] { return current().rotX; },
                                      [rotateChange](double v) { rotateChange([v](Extrusion& e) { e.rotX = v; }); });
        QDoubleSpinBox* ry = addField(angles, tr("Y:"), tr("Drehung um die senkrechte Achse (schwenken)"), QStringLiteral(" °"), 1,
                                      [current] { return current().rotY; },
                                      [rotateChange](double v) { rotateChange([v](Extrusion& e) { e.rotY = v; }); });
        QDoubleSpinBox* rz = addField(m_row, tr("Z:"), tr("Drehung in der Ebene"), QStringLiteral(" °"), 1,
                                      [current] { return current().rotZ; },
                                      [rotateChange](double v) { rotateChange([v](Extrusion& e) { e.rotZ = v; }); });
        for (QDoubleSpinBox* a : {rx, ry, rz})
            a->setRange(-360, 360);
        addButton(QStringLiteral("frame-reset"), tr("Drehung zurücksetzen"), [rotateChange] {
            rotateChange([](Extrusion& e) { e.rotX = e.rotY = e.rotZ = 0.0; });
        });
        addSeparator();
        addButton(QStringLiteral("fx-clear"), tr("Extrusion löschen"), [this] { m_editor.setSelectionExtrusion(Extrusion(), tr("Extrusion löschen")); });
        return;
    }
    if (m_signature.startsWith(QLatin1String("distort:"))) {
        auto* group = new QButtonGroup(m_host);
        const QStringList icons{QStringLiteral("distort-pushpull"), QStringLiteral("distort-zipper"), QStringLiteral("distort-twister")};
        const QStringList texts{tr("Stossen und Ziehen"), tr("Zickzack"), tr("Wirbel")};
        const int mode = Options::instance().integer(QStringLiteral("ws/tools/distort/mode"));
        for (int i = 0; i < 3; ++i) {
            QToolButton* b = addButton(icons[i], texts[i], {});
            b->setCheckable(true);
            b->setChecked(i == mode);
            group->addButton(b, i);
        }
        connect(group, &QButtonGroup::idClicked, this, [](int i) { Options::instance().set(QStringLiteral("ws/tools/distort/mode"), i); });
        if (mode == 1) {
            QDoubleSpinBox* f = addField(m_row, QStringLiteral("distort-zipper"), tr("Zickzackfrequenz"), QString(), 0,
                                         [] { return Options::instance().number(QStringLiteral("ws/tools/distort/frequency")); },
                                         [](double v) { Options::instance().set(QStringLiteral("ws/tools/distort/frequency"), int(v)); });
            f->setRange(2, 100);
            // Zickzack-Optionen wie gewohnt: Zufällig, Glatt, Lokal
            const QList<std::pair<QString, QString>> toggles{
                {QStringLiteral("random"), tr("Zufällig")}, {QStringLiteral("smooth"), tr("Glatt")}, {QStringLiteral("local"), tr("Lokal")}};
            const QStringList tips{tr("Zufällige Verzerrung: die Amplitude schwankt"), tr("Glatte Verzerrung: Wellen statt Zacken"),
                                   tr("Lokale Verzerrung: die Amplitude nimmt vom Mittelpunkt nach aussen ab")};
            for (int i = 0; i < toggles.size(); ++i) {
                const QString key = QStringLiteral("ws/tools/distort/") + toggles[i].first;
                QToolButton* b = addButton(QString(), tips[i], {});
                b->setObjectName(QStringLiteral("distort-") + toggles[i].first);
                b->setText(toggles[i].second);
                b->setToolButtonStyle(Qt::ToolButtonTextOnly);
                b->setCheckable(true);
                b->setChecked(Options::instance().flag(key));
                connect(b, &QToolButton::toggled, this, [key](bool on) { Options::instance().set(key, on); });
            }
        }
        auto* hint = new QLabel(tr("Vom Objekt wegziehen; die Länge bestimmt die Stärke"), m_host);
        hint->setEnabled(false);
        m_row->addWidget(hint);
        return;
    }
    // Hülle wie die Eigenschaftsleiste: Vorlagen, Modus, Neue Hülle, Hülle aus Objekt, Hülle löschen
    auto* presets = new QComboBox(m_host);
    presets->setToolTip(tr("Vorlagen"));
    presets->setIconSize(QSize(32, 24));
    presets->addItem(tr("Vorlage …"));
    const QList<std::pair<QString, QPainterPath>> shapes = EnvelopeTool::presets();
    for (const auto& [name, shape] : shapes)
        presets->addItem(EnvelopeTool::presetIcon(shape), name);
    m_row->addWidget(presets);
    connect(presets, &QComboBox::activated, this, [this, shapes, presets](int i) {
        if (i > 0 && i - 1 < shapes.size())
            m_editor.applyEnvelopeShape(shapes[i - 1].second, false, EnvelopeTool::mode(), false, EnvelopeDocker::mapping());
        presets->setCurrentIndex(0);
    });
    addSeparator();
    auto* group = new QButtonGroup(m_host);
    const QStringList icons{QStringLiteral("env-straight"), QStringLiteral("env-single"), QStringLiteral("env-double"), QStringLiteral("env-free")};
    const QStringList texts{tr("Modus Gerade"), tr("Modus Einfacher Bogen"), tr("Modus Doppelter Bogen"), tr("Modus Ohne Einschränkung")};
    for (int i = 0; i < 4; ++i) {
        QToolButton* b = addButton(icons[i], texts[i], {});
        b->setCheckable(true);
        b->setChecked(i == int(EnvelopeTool::mode()));
        group->addButton(b, i);
    }
    connect(group, &QButtonGroup::idClicked, this, [this](int i) { EnvelopeDocker::setMode(m_editor, Envelope::Mode(i)); });
    // Zuordnungsmodus
    auto* mapping = new QComboBox(m_host);
    mapping->setToolTip(tr("Zuordnungsmodus"));
    using M = Envelope::Mapping;
    for (const auto& [m, text] : {std::pair{M::Standard, tr("Frei")}, std::pair{M::Horizontal, tr("Horizontal")},
                                  std::pair{M::Original, tr("Original")}, std::pair{M::Putty, tr("Kitt")},
                                  std::pair{M::Vertical, tr("Vertikal")}})
        mapping->addItem(text, int(m));
    m_row->addWidget(mapping);
    connect(mapping, &QComboBox::activated, this,
            [this, mapping] { EnvelopeDocker::setMapping(m_editor, Envelope::Mapping(mapping->currentData().toInt())); });
    m_refreshers.append([this, mapping] {
        const QList<const DrawObject*> sel = m_editor.selectedObjects();
        const Envelope::Mapping m = sel.size() == 1 && sel.first()->hasEnvelope() ? sel.first()->envelopes().last().mapping
                                                                                  : EnvelopeDocker::mapping();
        const QSignalBlocker block(mapping);
        mapping->setCurrentIndex(std::max(0, mapping->findData(int(m))));
    });
    addSeparator();
    addButton(QStringLiteral("env-new"), tr("Neue Hülle hinzufügen"), [this] {
        m_editor.applyEnvelopeShape({}, false, EnvelopeTool::mode(), true, EnvelopeDocker::mapping());
    });
    addButton(QStringLiteral("env-from"), tr("Hülle aus Objekt erstellen"), [this] {
        if (m_editor.selection().isEmpty())
            return;
        m_canvas.setTool(ToolId::Pick);
        m_canvas.pickTool().beginPickTarget([this](NodeId source) {
            if (const DrawObject* o = m_editor.document().findAs<DrawObject>(source))
                m_editor.applyEnvelopeShape(o->path(), true, EnvelopeTool::mode(), false, EnvelopeDocker::mapping());
            m_canvas.setTool(ToolId::Envelope);
        });
    });
    addButton(QStringLiteral("fx-clear"), tr("Hülle löschen"), [this] { m_editor.clearSelectionEnvelopes(); });
    auto* hint = new QLabel(tr("Griffe ziehen verformt die Hülle; der Text bleibt bearbeitbar"), m_host);
    hint->setEnabled(false);
    m_row->addWidget(hint);
}

void PropertyBar::buildShadowSection()
{
    // Wie die Eigenschaftsleiste des Hilfsmittels 'Hinterlegter Schatten'
    auto current = [this]() {
        const QList<const DrawObject*> sel = m_editor.selectedObjects();
        DropShadow s = sel.isEmpty() ? DropShadow() : sel.first()->shadow();
        if (!s.active)
            s = DropShadow();
        return s;
    };
    auto change = [this, current](const std::function<void(DropShadow&)>& fn, const QString& label) {
        DropShadow s = current();
        s.active = true;
        fn(s);
        m_editor.setSelectionShadow(s, label);
    };
    auto* presets = new QComboBox(m_host);
    presets->setToolTip(tr("Voreinstellungen"));
    presets->addItems({tr("Voreinstellung …"), tr("Flach unten rechts"), tr("Flach unten links"), tr("Leuchten (Mitte)"),
                       tr("Perspektive unten rechts"), tr("Perspektive unten links"), tr("Perspektive oben"), tr("Innenkante"),
                       tr("Inneres Leuchten"), tr("Innen oben links"), tr("Innen oben rechts"), tr("Innen unten links"),
                       tr("Innen unten rechts")});
    m_row->addWidget(presets);
    connect(presets, &QComboBox::activated, this, [change, presets](int i) {
        if (i == 0)
            return;
        if (i >= 7) {
            // Innenschatten: rundum (Kante, Leuchten) oder an zwei Innenkanten
            change([i](DropShadow& s) {
                s.inner = true;
                s.perspective = false;
                s.fade = 0;
                const double d = 1.5;
                s.offset = i == 9 ? QPointF(d, d) : i == 10 ? QPointF(-d, d) : i == 11 ? QPointF(d, -d) : i == 12 ? QPointF(-d, -d) : QPointF(0, 0);
                s.feather = i == 7 ? 2.0 : i == 8 ? 5.0 : 1.5;
                s.opacity = i == 8 ? 60.0 : 50.0;
            }, tr("Innenschatten"));
            presets->setCurrentIndex(0);
            return;
        }
        change([i](DropShadow& s) {
            s.inner = false;
            s.perspective = i >= 4;
            s.fromTop = i == 6;
            s.offset = i == 1 ? QPointF(2, 2) : i == 2 ? QPointF(-2, 2) : QPointF(0, 0);
            if (i == 3)
                s.feather = 4.0;
            s.angle = i == 4 ? -45 : i == 5 ? 45 : 30;
            s.stretch = i == 6 ? 40 : 60;
            s.fade = s.perspective ? 50 : 0;
        }, tr("Hinterlegter Schatten"));
        presets->setCurrentIndex(0);
    });
    // Art: hinterlegt (unter dem Objekt) oder innen (im Objekt)
    auto* kind = new QComboBox(m_host);
    kind->setToolTip(tr("Schattenart"));
    kind->addItems({tr("Hinterlegter Schatten"), tr("Innenschatten")});
    m_row->addWidget(kind);
    connect(kind, &QComboBox::activated, this, [change](int i) {
        change([i](DropShadow& s) {
            s.inner = i == 1;
            if (s.inner)
                s.perspective = false;
        }, i == 1 ? tr("Innenschatten") : tr("Hinterlegter Schatten"));
    });
    m_refreshers.append([kind, current] {
        const QSignalBlocker b(kind);
        kind->setCurrentIndex(current().inner ? 1 : 0);
    });
    addSeparator();
    const bool perspective = m_signature == QLatin1String("shadow:2");
    QBoxLayout* stack = addStack();
    if (perspective) {
        addField(stack, QStringLiteral("shadow-angle"), tr("Schattenwinkel"), QStringLiteral(" °"), 0,
                 [current] { return current().angle; }, [change](double v) { change([v](DropShadow& s) { s.angle = v; }, tr("Schattenwinkel")); });
        addField(stack, QStringLiteral("shadow-stretch"), tr("Schattenstreckung"), QStringLiteral(" %"), 0,
                 [current] { return current().stretch; }, [change](double v) { change([v](DropShadow& s) { s.stretch = v; }, tr("Schattenstreckung")); });
        addField(m_row, QStringLiteral("shadow-fade"), tr("Schattenausblendung"), QStringLiteral(" %"), 0,
                 [current] { return current().fade; }, [change](double v) { change([v](DropShadow& s) { s.fade = v; }, tr("Schattenausblendung")); });
    } else {
        addField(stack, QStringLiteral("pos-x"), tr("Schattenversatz waagrecht"), QStringLiteral(" mm"), 2,
                 [current] { return current().offset.x(); }, [change](double v) { change([v](DropShadow& s) { s.offset.setX(v); }, tr("Schattenversatz")); });
        addField(stack, QStringLiteral("pos-y"), tr("Schattenversatz senkrecht"), QStringLiteral(" mm"), 2,
                 [current] { return current().offset.y(); }, [change](double v) { change([v](DropShadow& s) { s.offset.setY(v); }, tr("Schattenversatz")); });
    }
    addSeparator();
    QDoubleSpinBox* opacity = addField(m_row, QStringLiteral("shadow-opacity"), tr("Schattendeckung"), QStringLiteral(" %"), 0,
                                       [current] { return current().opacity; },
                                       [change](double v) { change([v](DropShadow& s) { s.opacity = v; }, tr("Schattendeckung")); });
    opacity->setRange(0, 100);
    QDoubleSpinBox* feather = addField(m_row, QStringLiteral("shadow-feather"), tr("Schattenfederung"), QStringLiteral(" mm"), 2,
                                       [current] { return current().feather; },
                                       [change](double v) { change([v](DropShadow& s) { s.feather = v; }, tr("Schattenfederung")); });
    feather->setRange(0, 100);
    addButton(QStringLiteral("shadow-color"), tr("Schattenfarbe"), [this, current, change] {
        if (const std::optional<Color> c = ColorDialog::getColor(m_host, tr("Schattenfarbe"), current().color))
            change([c](DropShadow& s) { s.color = *c; }, tr("Schattenfarbe"));
    });
    QComboBox* mode = createBlendModeCombo(m_host, tr("Mischmodus des Schattens"));
    m_row->addWidget(mode);
    connect(mode, &QComboBox::activated, this, [change, mode](int i) {
        const BlendMode m = BlendMode(mode->itemData(i).toInt());
        change([m](DropShadow& s) { s.blend = m; }, tr("Mischmodus des Schattens"));
    });
    m_refreshers.append([mode, current] { setBlendModeComboValue(mode, current().blend); });
    addSeparator();
    addButton(QStringLiteral("fx-clear"), tr("Schatten löschen"), [this] { m_editor.setSelectionShadow(DropShadow(), tr("Schatten löschen")); });
}

void PropertyBar::buildMeshSection()
{
    // Wie die Eigenschaftsleiste der Maschenfüllung: Spalten und Reihen des Netzes, Netz löschen
    auto mesh = [this]() -> const MeshFill* {
        const QList<const DrawObject*> objects = m_editor.selectedObjects();
        return objects.size() == 1 && objects.first()->style().fill.isMesh() ? &objects.first()->style().fill.mesh : nullptr;
    };
    auto field = [this, mesh](const QString& key, const QString& iconName, const QString& tip, bool columns) {
        QDoubleSpinBox* f = addField(m_row, iconName, tip, QString(), 0,
                                     [key, mesh, columns] {
                                         if (const MeshFill* m = mesh())
                                             return double(columns ? m->columns : m->rows);
                                         return Options::instance().number(key);
                                     },
                                     [this, key, mesh, columns](double v) {
                                         const int n = int(std::lround(v));
                                         Options::instance().set(key, n);
                                         if (const MeshFill* m = mesh())
                                             m_canvas.meshTool().setGrid(columns ? n : m->columns, columns ? m->rows : n);
                                     });
        f->setRange(1, 50);
        addSpinButtons(f, 1);
        return f;
    };
    field(QStringLiteral("ws/tools/mesh/cols"), QStringLiteral("graph-columns"), tr("Spaltenanzahl des Netzes"), true);
    field(QStringLiteral("ws/tools/mesh/rows"), QStringLiteral("graph-rows"), tr("Anzahl Reihen des Netzes"), false);
    addSeparator();
    QToolButton* clear = addButton(QStringLiteral("edit-delete"), tr("Netz löschen"), [this, mesh] {
        const QList<const DrawObject*> objects = m_editor.selectedObjects();
        if (mesh())
            m_editor.setObjectFill(objects.first()->id(),
                                   Fill::solid(objects.first()->style().fill.representativeColor().value_or(Color(255, 255, 255))),
                                   tr("Maschenfüllung löschen"));
    });
    m_refreshers.append([clear, mesh] { clear->setEnabled(mesh() != nullptr); });
    auto* hint = new QLabel(tr("Klick ins Objekt legt das Netz an; Knoten ziehen, Palettenfarbe färbt gewählte Knoten"), m_host);
    hint->setEnabled(false);
    m_row->addWidget(hint);
}

void PropertyBar::buildShapeDefaultsSection()
{
    ShapeDefaults& d = m_editor.shapeDefaults();
    const ToolId tool = m_canvas.toolId();
    auto optionField = [this](const QString& key, const QString& iconName, const QString& tip, double min, double max) {
        QDoubleSpinBox* f = addField(m_row, iconName, tip, QString(), 0,
                                     [key] { return Options::instance().number(key); },
                                     [key](double v) { Options::instance().set(key, int(std::lround(v))); });
        f->setRange(min, max);
        addSpinButtons(f, 1);
        return f;
    };
    if (tool == ToolId::GraphPaper) {
        // Zeilen und Spalten wie in der Eigenschaftsleiste des Millimeterpapiers
        optionField(QStringLiteral("ws/tools/graph/cols"), QStringLiteral("graph-columns"), tr("Spalten"), 1, 99);
        optionField(QStringLiteral("ws/tools/graph/rows"), QStringLiteral("graph-rows"), tr("Zeilen"), 1, 99);
        return;
    }
    if (tool == ToolId::Spiral) {
        optionField(QStringLiteral("ws/tools/spiral/revolutions"), QStringLiteral("spiral-revolutions"), tr("Spiralumdrehungen"), 1, 100);
        addSeparator();
        auto* group = new QButtonGroup(m_host);
        QToolButton* sym = addButton(QStringLiteral("spiral-symmetric"), tr("Symmetrische Spirale"), {});
        QToolButton* log = addButton(QStringLiteral("spiral-log"), tr("Logarithmische Spirale"), {});
        sym->setCheckable(true);
        log->setCheckable(true);
        group->addButton(sym, 0);
        group->addButton(log, 1);
        connect(group, &QButtonGroup::idClicked, this, [](int id) { Options::instance().set(QStringLiteral("ws/tools/spiral/type"), id); });
        QDoubleSpinBox* expansion = optionField(QStringLiteral("ws/tools/spiral/expansion"), QStringLiteral("spiral-expansion"),
                                                tr("Spiralausdehnung (logarithmisch)"), 1, 100);
        m_refreshers.append([sym, log, expansion] {
            const bool logarithmic = Options::instance().integer(QStringLiteral("ws/tools/spiral/type")) == 1;
            sym->setChecked(!logarithmic);
            log->setChecked(logarithmic);
            expansion->setEnabled(logarithmic);
        });
        return;
    }
    if (tool == ToolId::Ellipse) {
        // Ellipse, Kreissegment oder Bogen wie in der Eigenschaftsleiste des Ellipsenwerkzeugs
        auto* group = new QButtonGroup(m_host);
        const QStringList icons{QStringLiteral("ellipse-full"), QStringLiteral("ellipse-pie"),
                                QStringLiteral("ellipse-arc")};
        const QStringList texts{tr("Ellipse"), tr("Kreissegment"), tr("Bogen")};
        for (int i = 0; i < icons.size(); ++i) {
            QToolButton* b = addButton(icons[i], texts[i], {});
            b->setCheckable(true);
            b->setChecked(int(d.ellipseKind) == i);
            group->addButton(b, i);
        }
        connect(group, &QButtonGroup::idClicked, this, [this, &d](int index) {
            d.ellipseKind = EllipseObject::Kind(index);
            Options::instance().set(QStringLiteral("ws/tools/ellipse/kind"), index);
            rebuild();
        });
        if (d.ellipseKind == EllipseObject::Kind::Ellipse)
            return;
        addSeparator();
        QBoxLayout* angles = addStack();
        addField(angles, QStringLiteral("angle-start"), tr("Anfangswinkel"), QStringLiteral(" °"), 1,
                 [&d] { return d.startAngle; }, [&d](double v) {
                     d.startAngle = v;
                     Options::instance().set(QStringLiteral("ws/tools/ellipse/start"), v);
                 });
        addField(angles, QStringLiteral("angle-end"), tr("Endwinkel"), QStringLiteral(" °"), 1,
                 [&d] { return d.endAngle; }, [&d](double v) {
                     d.endAngle = v;
                     Options::instance().set(QStringLiteral("ws/tools/ellipse/end"), v);
                 });
        QToolButton* direction = addButton(QStringLiteral("arc-direction"), tr("Richtung ändern (im Uhrzeigersinn)"), {});
        direction->setCheckable(true);
        connect(direction, &QToolButton::toggled, this, [&d](bool on) {
            d.clockwise = on;
            Options::instance().set(QStringLiteral("ws/tools/ellipse/clockwise"), on ? 1 : 0);
        });
        m_refreshers.append([direction, &d] {
            const QSignalBlocker block(direction);
            direction->setChecked(d.clockwise);
        });
        return;
    }
    // Ecken und Schärfe für neue Polygone und Sterne
    using K = PolygonObject::Kind;
    const K kind = tool == ToolId::Star ? K::Star : tool == ToolId::ComplexStar ? K::ComplexStar : K::Polygon;
    int& points = kind == K::Star ? d.starPoints : kind == K::ComplexStar ? d.complexStarPoints : d.polygonPoints;
    QDoubleSpinBox* pointsField = addField(
        m_row, kind == K::Polygon ? QStringLiteral("polygon-points") : kind == K::Star ? QStringLiteral("star-points")
                                                                                          : QStringLiteral("complex-star-points"),
        kind == K::Polygon ? tr("Punkte oder Seiten des Polygons") : tr("Spitzen des Sterns"), QString(), 0,
        [&points] { return double(points); }, [this, &points, &d, kind](double v) {
            points = int(std::lround(v));
            int& sharp = kind == K::ComplexStar ? d.complexStarSharpness : d.starSharpness;
            sharp = PolygonObject::clampSharpness(kind, points, sharp);
            refresh();
        });
    pointsField->setRange(PolygonObject::minPoints(kind), 500);
    addSpinButtons(pointsField, 1);
    if (kind == K::Polygon)
        return;
    int& sharpness = kind == K::Star ? d.starSharpness : d.complexStarSharpness;
    QDoubleSpinBox* sharpField = addField(m_row, QStringLiteral("star-sharpness"), tr("Schärfe der Spitzen"), QString(), 0,
                                          [&sharpness] { return double(sharpness); },
                                          [&sharpness, &points, kind](double v) {
                                              sharpness = PolygonObject::clampSharpness(kind, points, int(std::lround(v)));
                                          });
    addSpinButtons(sharpField, 1);
    // Zuerst den Bereich nachführen (hängt von der Eckenzahl ab), dann den Wert
    m_refreshers.prepend([sharpField, &points, kind] {
        sharpField->setRange(1, PolygonObject::maxSharpness(kind, points));
    });
}

void PropertyBar::buildEraserSection()
{
    EraserTool& eraser = m_canvas.eraserTool();
    QDoubleSpinBox* width = addField(m_row, QStringLiteral("eraser-width"), tr("Stärke des Radierers"),
                                     QStringLiteral(" mm"), 1, [&eraser] { return eraser.width(); },
                                     [&eraser](double v) { eraser.setWidth(v); });
    width->setMinimum(0.1);
    addSpinButtons(width, 0.5);
    addSeparator();
    QToolButton* round = addButton(QStringLiteral("eraser-round"), tr("Runde Spitze"), [&eraser, this] {
        eraser.setRound(true);
        refresh();
    });
    QToolButton* square = addButton(QStringLiteral("eraser-square"), tr("Eckige Spitze"), [&eraser, this] {
        eraser.setRound(false);
        refresh();
    });
    round->setCheckable(true);
    square->setCheckable(true);
    m_refreshers.append([=, &eraser] {
        round->setChecked(eraser.round());
        square->setChecked(!eraser.round());
    });
}

void PropertyBar::buildMediaSection()
{
    ArtisticMediaTool& tool = m_canvas.mediaTool();
    using Kind = ArtisticMedia::Kind;
    struct Mode {
        const char* icon;
        QString text;
        Kind kind;
    };
    const Mode modes[] = {
        {"media-preset", tr("Voreinstellung"), Kind::Preset},
        {"media-brush", tr("Pinsel"), Kind::Brush},
        {"media-sprayer", tr("Sprühdose"), Kind::Sprayer},
        {"media-calligraphy", tr("Kalligrafisch"), Kind::Calligraphy},
        {"media-pressure", tr("Druck (Stift oder Pfeil auf/ab beim Zeichnen)"), Kind::Pressure},
    };
    for (const Mode& m : modes) {
        const Kind kind = m.kind;
        QToolButton* b = addButton(QString::fromLatin1(m.icon), m.text, [&tool, kind, this] {
            ArtisticMedia media = tool.media();
            if (media.kind == kind)
                return;
            // Pinsel und Sprühdose beginnen mit dem ersten Eintrag ihrer Bibliothek
            if (kind == Kind::Brush || kind == Kind::Sprayer) {
                const MediaLibrary& lib = MediaLibrary::instance();
                const QList<MediaStroke> list = kind == Kind::Brush ? lib.brushes() : lib.sprayers();
                const double width = media.width;
                media = list.value(0).media;
                media.width = width;
            } else {
                media.art.clear();
                media.artName.clear();
            }
            media.kind = kind;
            tool.setMedia(media);
            m_canvas.notifyToolState();
        });
        b->setObjectName(QStringLiteral("mediaMode%1").arg(int(kind)));
        b->setCheckable(true);
        m_refreshers.append([b, &tool, kind] { b->setChecked(tool.media().kind == kind); });
    }
    addSeparator();
    QDoubleSpinBox* smoothing = addField(m_row, QStringLiteral("smoothing"), tr("Freihandglättung"), QString(), 0,
                                         [&tool] { return double(tool.smoothing()); },
                                         [&tool](double v) { tool.setSmoothing(int(std::lround(v))); });
    smoothing->setRange(0, 100);
    addSpinButtons(smoothing, 5);
    addSeparator();
    const Kind current = tool.media().kind;
    QDoubleSpinBox* width = addField(m_row, QStringLiteral("media-width"),
                                     current == Kind::Sprayer ? tr("Grösse der Objekte") : tr("Strichbreite"), QStringLiteral(" mm"), 1,
                                     [&tool] { return tool.media().width; }, [&tool](double v) {
                                         ArtisticMedia media = tool.media();
                                         media.width = std::max(0.05, v);
                                         tool.setMedia(media);
                                     });
    width->setObjectName(QStringLiteral("mediaWidth"));
    width->setMinimum(0.05);
    addSpinButtons(width, 0.5);

    // Änderung einer Einstellung des Werkzeugs (gilt auch für die gewählten Medienkurven)
    auto change = [&tool](const std::function<void(ArtisticMedia&)>& fn) {
        ArtisticMedia media = tool.media();
        fn(media);
        tool.setMedia(media);
    };
    // Auswahlliste mit Vorschaubildern aus der Bibliothek (Pinsel bzw. Sprühlisten); eigene
    // Einträge kommen beim Auffrischen dazu
    auto libraryCombo = [&, this](const QString& name, const QString& tip, bool brushes) {
        auto* combo = new QComboBox(m_host);
        combo->setObjectName(name);
        combo->setToolTip(tip);
        combo->setIconSize(QSize(96, 20));
        combo->setMinimumWidth(150);
        m_row->addWidget(combo);
        auto revision = std::make_shared<int>(-1);
        auto entries = std::make_shared<QList<MediaStroke>>();
        connect(combo, &QComboBox::activated, this, [&tool, entries, this](int i) {
            if (i < 0 || i >= entries->size())
                return;
            const MediaStroke& s = entries->at(i);
            ArtisticMedia media = tool.media();
            media.art = s.media.art;
            media.artName = s.media.artName;
            media.sprayList = s.media.sprayList;
            media.artAspect = s.media.artAspect;
            if (media.kind == ArtisticMedia::Kind::Brush)
                media.brushRepeat = s.media.brushRepeat;
            tool.setMedia(media);
            MediaLibrary::instance().noteUsed(MediaStroke{s.name, media, false});
            m_canvas.notifyToolState();
        });
        m_refreshers.append([combo, &tool, revision, entries, brushes, this] {
            const MediaLibrary& lib = MediaLibrary::instance();
            const QSignalBlocker block(combo);
            if (*revision != lib.revision()) {
                *revision = lib.revision();
                *entries = brushes ? lib.brushes() : lib.sprayers();
                combo->clear();
                for (const MediaStroke& s : std::as_const(*entries))
                    combo->addItem(mediaStrokeIcon(s.media, combo->iconSize(), devicePixelRatioF()), s.name);
            }
            const ArtisticMedia& m = tool.media();
            int found = -1;
            for (int i = 0; i < entries->size() && found < 0; ++i) {
                const ArtisticMedia& e = entries->at(i).media;
                if (m.art.isEmpty() ? (e.art.isEmpty() && e.sprayList == m.sprayList) : (e.artName == m.artName && e.art == m.art))
                    found = i;
            }
            combo->setCurrentIndex(found);
        });
    };

    if (current == Kind::Pressure)
        return;
    if (current == Kind::Brush) {
        addSeparator();
        libraryCombo(QStringLiteral("brushList"), tr("Pinselstrich"), true);
        QToolButton* repeat = addButton(QStringLiteral("edit-repeat"), tr("Vorlage wiederholen statt strecken"), {});
        repeat->setObjectName(QStringLiteral("brushRepeat"));
        repeat->setCheckable(true);
        connect(repeat, &QToolButton::toggled, this, [change](bool on) { change([on](ArtisticMedia& m) { m.brushRepeat = on; }); });
        m_refreshers.append([repeat, &tool] {
            const QSignalBlocker block(repeat);
            repeat->setChecked(tool.media().brushRepeat);
        });
        return;
    }
    if (current == Kind::Sprayer) {
        // Sprühliste, Grössenvariation, Abstand, Anzahl je Tupfer, Drehung, Versatz, Reihenfolge
        addSeparator();
        libraryCombo(QStringLiteral("sprayList"), tr("Sprühliste"), false);
        QDoubleSpinBox* variation = addField(m_row, QStringLiteral("xf-size"), tr("Grössenvariation"), QStringLiteral(" %"), 0,
                                             [&tool] { return tool.media().sizeVariation; },
                                             [change](double v) { change([v](ArtisticMedia& m) { m.sizeVariation = std::clamp(v, 0.0, 100.0); }); });
        variation->setObjectName(QStringLiteral("sprayVariation"));
        variation->setRange(0, 100);
        addSeparator();
        QDoubleSpinBox* spacing = addField(m_row, QStringLiteral("contour-offset"), tr("Abstand der Tupfer"), QStringLiteral(" mm"), 1,
                                           [&tool] { return tool.media().spacing; },
                                           [change](double v) { change([v](ArtisticMedia& m) { m.spacing = std::max(0.2, v); }); });
        spacing->setObjectName(QStringLiteral("spraySpacing"));
        spacing->setRange(0.2, 500);
        QDoubleSpinBox* count = addField(m_row, QStringLiteral("blend-steps"), tr("Objekte je Tupfer"), QString(), 0,
                                         [&tool] { return double(tool.media().dabCount); },
                                         [change](double v) { change([v](ArtisticMedia& m) { m.dabCount = std::clamp(int(std::lround(v)), 1, 50); }); });
        count->setObjectName(QStringLiteral("sprayCount"));
        count->setRange(1, 50);
        addSpinButtons(count, 1);
        addSeparator();
        QToolButton* rotate = addButton(QStringLiteral("ft-rotate"), tr("Zufällig drehen (sonst entlang des Pfads)"), {});
        rotate->setObjectName(QStringLiteral("sprayRotate"));
        rotate->setCheckable(true);
        connect(rotate, &QToolButton::toggled, this, [change](bool on) { change([on](ArtisticMedia& m) { m.randomRotate = on; }); });
        m_refreshers.append([rotate, &tool] {
            const QSignalBlocker block(rotate);
            rotate->setChecked(tool.media().randomRotate);
        });
        auto* offsetMode = new QComboBox(m_host);
        offsetMode->setObjectName(QStringLiteral("sprayOffsetMode"));
        offsetMode->setToolTip(tr("Versatz quer zum Pfad"));
        offsetMode->addItems({tr("Kein Versatz"), tr("Abwechselnd"), tr("Zufällig")});
        m_row->addWidget(offsetMode);
        connect(offsetMode, &QComboBox::activated, this,
                [change](int i) { change([i](ArtisticMedia& m) { m.offsetMode = ArtisticMedia::Offset(std::clamp(i, 0, 2)); }); });
        m_refreshers.append([offsetMode, &tool] {
            const QSignalBlocker block(offsetMode);
            offsetMode->setCurrentIndex(int(tool.media().offsetMode));
        });
        QDoubleSpinBox* offset = addField(m_row, QString(), tr("Versatz in % der Grösse"), QStringLiteral(" %"), 0,
                                          [&tool] { return tool.media().offset; },
                                          [change](double v) { change([v](ArtisticMedia& m) { m.offset = std::clamp(v, 0.0, 1000.0); }); });
        offset->setObjectName(QStringLiteral("sprayOffset"));
        offset->setRange(0, 1000);
        auto* order = new QComboBox(m_host);
        order->setObjectName(QStringLiteral("sprayOrder"));
        order->setToolTip(tr("Reihenfolge der Objekte"));
        order->addItems({tr("Zufällig"), tr("Der Reihe nach"), tr("Nach Richtung")});
        m_row->addWidget(order);
        connect(order, &QComboBox::activated, this,
                [change](int i) { change([i](ArtisticMedia& m) { m.order = ArtisticMedia::Order(std::clamp(i, 0, 2)); }); });
        m_refreshers.append([order, &tool] {
            const QSignalBlocker block(order);
            order->setCurrentIndex(int(tool.media().order));
        });
        return;
    }
    if (current == Kind::Calligraphy) {
        addField(m_row, QStringLiteral("media-angle"), tr("Federwinkel"), QStringLiteral(" °"), 1,
                 [&tool] { return tool.media().angle; }, [&tool](double v) {
                     ArtisticMedia media = tool.media();
                     media.angle = v;
                     tool.setMedia(media);
                 });
        return;
    }
    // Strichformen als Vorschaubilder, wie die Liste "Originale"
    addSeparator();
    auto* presets = new QComboBox(m_host);
    presets->setObjectName(QStringLiteral("presetList"));
    presets->setToolTip(tr("Strichform"));
    presets->setIconSize(QSize(96, 20));
    for (int i = 0; i < pathops::mediaPresetCount(); ++i) {
        ArtisticMedia sample;
        sample.kind = Kind::Preset;
        sample.preset = i;
        presets->addItem(mediaStrokeIcon(sample, presets->iconSize(), devicePixelRatioF()), QString());
        presets->setItemData(i, pathops::mediaPresetName(i), Qt::ToolTipRole);
    }
    m_row->addWidget(presets);
    connect(presets, &QComboBox::activated, this, [&tool](int index) {
        ArtisticMedia media = tool.media();
        media.preset = index;
        tool.setMedia(media);
    });
    m_refreshers.append([presets, &tool] {
        const QSignalBlocker block(presets);
        presets->setCurrentIndex(tool.media().preset);
    });
}

void PropertyBar::buildGuideSection()
{
    auto guide = [this] { return m_editor.guides().value(m_editor.selectedGuide()); };
    auto apply = [this](const Guide& g, const QString& label) { m_editor.setGuide(m_editor.selectedGuide(), g, label); };

    QBoxLayout* pos = addStack();
    addField(pos, QStringLiteral("x:"), tr("Ankerpunkt x"), QStringLiteral(" mm"), 3,
             [=, this] { return guide().anchor.x(); },
             [=, this](double v) {
                 Guide g = guide();
                 g.anchor.setX(v);
                 apply(g, tr("Hilfslinie verschieben"));
             });
    addField(pos, QStringLiteral("y:"), tr("Ankerpunkt y"), QStringLiteral(" mm"), 3, [=, this] { return guide().anchor.y(); },
             [=, this](double v) {
                 Guide g = guide();
                 g.anchor.setY(v);
                 apply(g, tr("Hilfslinie verschieben"));
             });
    addSeparator();
    addField(m_row, QStringLiteral("rotate"), tr("Winkel der Hilfslinie (gegen den Uhrzeigersinn)"),
             QStringLiteral(" °"), 1, [=, this] { return guide().angle; },
             [=, this](double v) {
                 Guide g = guide();
                 g.angle = v;
                 apply(g, tr("Hilfslinie drehen"));
             });
    addSeparator();
    addButton(QStringLiteral("guide-horizontal"), tr("Waagrecht stellen"), [=, this] {
        Guide g = guide();
        g.angle = 0.0;
        apply(g, tr("Hilfslinie drehen"));
    });
    addButton(QStringLiteral("guide-vertical"), tr("Senkrecht stellen"), [=, this] {
        Guide g = guide();
        g.angle = 90.0;
        apply(g, tr("Hilfslinie drehen"));
    });
    // Global (alle Seiten, Standard) oder nur auf dieser Seite
    addSeparator();
    QToolButton* everywhere = addButton(QStringLiteral("pages-all"), tr("Hilfslinie auf allen Seiten"), [this] {
        m_editor.setGuidePageSpecific(m_editor.selectedGuide(), false);
    });
    QToolButton* thisPage = addButton(QStringLiteral("page-current"), tr("Hilfslinie nur auf dieser Seite"), [this] {
        m_editor.setGuidePageSpecific(m_editor.selectedGuide(), true);
    });
    everywhere->setCheckable(true);
    thisPage->setCheckable(true);
    m_refreshers.append([=, this] {
        const bool local = m_editor.isGuidePageSpecific(m_editor.selectedGuide());
        everywhere->setChecked(!local);
        thisPage->setChecked(local);
    });
}

void PropertyBar::buildFillSection()
{
    auto first = [this]() -> const DrawObject* {
        const QList<const DrawObject*> objects = m_editor.selectedObjects();
        return objects.isEmpty() ? nullptr : objects.first();
    };
    auto fill = [=]() { return first() ? first()->style().fill : Fill(); };
    auto size = [this] { return m_editor.selectionLocalSize(); };

    auto* type = new QComboBox(m_host);
    type->setToolTip(tr("Füllart"));
    type->addItem(icon(QStringLiteral("fill-none")), tr("Keine Füllung"));
    type->addItem(icon(QStringLiteral("fill-uniform")), tr("Gleichmässig"));
    type->addItem(icon(QStringLiteral("fill-linear")), tr("Linear"));
    type->addItem(icon(QStringLiteral("fill-radial")), tr("Radial"));
    type->addItem(icon(QStringLiteral("fill-conical")), tr("Konisch"));
    m_row->addWidget(type);
    connect(type, &QComboBox::activated, this, [=, this](int index) {
        const Fill current = fill();
        Fill f;
        if (index == 1) {
            f = Fill::solid(current.representativeColor().value_or(Color(0, 0, 0)));
        } else if (index >= 2) {
            const Fill::Type t = index == 2 ? Fill::Type::Linear : index == 3 ? Fill::Type::Radial : Fill::Type::Conical;
            const Color from = current.representativeColor().value_or(Color(0, 0, 0));
            f = Fill::gradient(t, from, Color(255, 255, 255));
            if (current.isGradient())
                f.stops = current.stops;
        }
        m_editor.setSelectionFill(f);
    });
    m_refreshers.append([=] {
        const Fill f = fill();
        const int index = f.type == Fill::Type::None      ? 0
                          : f.type == Fill::Type::Solid   ? 1
                          : f.type == Fill::Type::Linear  ? 2
                          : f.type == Fill::Type::Radial  ? 3
                                                          : 4;
        const QSignalBlocker block(type);
        type->setCurrentIndex(index);
    });

    addSeparator();
    auto* from = new ColorButton(tr("Anfangsfarbe"), m_host);
    auto* to = new ColorButton(tr("Endfarbe"), m_host);
    from->setToolTip(tr("Anfangsfarbe"));
    m_row->addWidget(from);
    m_row->addWidget(new QLabel(QStringLiteral("→"), m_host));
    m_row->addWidget(to);
    connect(from, &ColorButton::colorChanged, this, [=, this](const Color& c) {
        Fill f = fill();
        if (f.isGradient())
            f.stops.first().color = c;
        else
            f = Fill::solid(c);
        m_editor.setSelectionFill(f);
    });
    connect(to, &ColorButton::colorChanged, this, [=, this](const Color& c) {
        Fill f = fill();
        if (!f.isGradient())
            return;
        f.stops.last().color = c;
        m_editor.setSelectionFill(f);
    });
    m_refreshers.append([=] {
        const Fill f = fill();
        from->setColor(f.isGradient() ? f.stops.first().color : f.representativeColor().value_or(Color(255, 255, 255)));
        to->setColor(f.isGradient() ? f.stops.last().color : Color(255, 255, 255));
        to->setEnabled(f.isGradient());
        from->setEnabled(!f.isNone());
    });

    addSeparator();
    auto geometry = [=] { return fill().geometry(size()); };
    auto setGeometry = [=, this](const GradientGeometry& g) {
        Fill f = fill();
        if (!f.isGradient())
            return;
        f.setGeometry(g, size());
        m_editor.setSelectionFill(f);
    };
    QDoubleSpinBox* angle = addField(m_row, QStringLiteral("rotate"), tr("Verlaufswinkel"), QStringLiteral(" °"), 1,
                                     [=] { return geometry().angle; },
                                     [=](double v) {
                                         GradientGeometry g = geometry();
                                         g.angle = v;
                                         setGeometry(g);
                                     });
    QDoubleSpinBox* pad = addField(m_row, QStringLiteral("edge-pad"), tr("Randauffüllung"), QStringLiteral(" %"), 1,
                                   [=] { return geometry().edgePad; },
                                   [=](double v) {
                                       GradientGeometry g = geometry();
                                       g.edgePad = v;
                                       setGeometry(g);
                                   });
    pad->setRange(0, 49);
    m_refreshers.append([=] {
        angle->setEnabled(fill().isGradient() && fill().type != Fill::Type::Radial);
        pad->setEnabled(fill().isGradient());
    });

    addSeparator();
    QToolButton* dialog = addButton(QStringLiteral("fill-linear"), tr("Farbverlauf bearbeiten … (F11)"), [=, this] {
        if (const std::optional<Fill> f = GradientDialog::getFill(window(), fill(), size()))
            m_editor.setSelectionFill(*f);
    });
    dialog->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    dialog->setText(tr("Farbverlauf …"));
}

void PropertyBar::buildNodeSection()
{
    // Knotenbefehle des Formwerkzeugs, Reihenfolge wie in der Eigenschaftsleiste
    using C = ShapeTool::Command;
    const QList<QList<C>> groups = {
        {C::Add, C::Delete}, {C::ToLine, C::ToCurve}, {C::Cusp, C::Smooth, C::Symmetric}, {C::Close, C::Reverse}};
    ShapeTool& shape = m_canvas.shapeTool();
    for (int g = 0; g < groups.size(); ++g) {
        if (g > 0)
            addSeparator();
        for (C c : groups[g]) {
            QToolButton* b = addButton(ShapeTool::commandIcon(c), ShapeTool::commandText(c), [&shape, c] { shape.run(c); });
            m_refreshers.append([b, &shape, c] { b->setEnabled(shape.canRun(c)); });
        }
    }
    // Gewählte Knoten transformieren: Rahmen zum Skalieren bzw. Drehen/Neigen, Spiegeln
    addSeparator();
    using T = ShapeTool::NodeTransform;
    const QList<std::tuple<T, QString, QString>> modes = {
        {T::Scale, QStringLiteral("ft-scale"), tr("Knoten strecken und skalieren")},
        {T::Rotate, QStringLiteral("ft-rotate"), tr("Knoten drehen oder neigen")}};
    for (const auto& entry : modes) {
        const T mode = std::get<0>(entry);
        QToolButton* b = addButton(std::get<1>(entry), std::get<2>(entry), {});
        b->setObjectName(mode == T::Scale ? QStringLiteral("nodeScale") : QStringLiteral("nodeRotate"));
        b->setCheckable(true);
        connect(b, &QToolButton::clicked, this, [&shape, mode](bool on) { shape.setNodeTransform(on ? mode : T::None); });
        m_refreshers.append([b, &shape, mode] {
            b->setEnabled(shape.canTransformNodes());
            b->setChecked(shape.nodeTransform() == mode);
        });
    }
    QToolButton* mirrorH = addButton(QStringLiteral("mirror-h"), tr("Knoten waagrecht spiegeln"),
                                     [&shape] { shape.reflectNodes(Qt::Horizontal); });
    QToolButton* mirrorV = addButton(QStringLiteral("mirror-v"), tr("Knoten senkrecht spiegeln"),
                                     [&shape] { shape.reflectNodes(Qt::Vertical); });
    m_refreshers.append([mirrorH, mirrorV, &shape] {
        mirrorH->setEnabled(shape.canTransformNodes());
        mirrorV->setEnabled(shape.canTransformNodes());
    });
    addSeparator();
    auto* count = new QLabel(m_host);
    m_row->addWidget(count);
    m_refreshers.append([count, &shape] {
        const CurveObject* c = shape.curve();
        count->setText(c ? tr("%1 von %2 Knoten gewählt").arg(shape.selectedNodes().size()).arg(c->nodeCount())
                         : QString());
    });
}

void PropertyBar::buildGeometrySection()
{
    auto bounds = [this] { return m_editor.selectionBounds(); };
    auto centered = [](const QPointF& c, double w, double h) { return QRectF(c.x() - w / 2, c.y() - h / 2, w, h); };

    QBoxLayout* pos = addStack();
    addField(pos, QStringLiteral("x:"), tr("Lage x (Mitte)"), QStringLiteral(" mm"), 3,
             [=, this] { return bounds().center().x(); },
             [=, this](double v) { m_editor.setSelectionBounds(bounds().translated(v - bounds().center().x(), 0)); });
    addField(pos, QStringLiteral("y:"), tr("Lage y (Mitte)"), QStringLiteral(" mm"), 3, [=, this] { return bounds().center().y(); },
             [=, this](double v) { m_editor.setSelectionBounds(bounds().translated(0, v - bounds().center().y())); });

    QBoxLayout* size = addStack();
    addField(size, QStringLiteral("width"), tr("Breite"), QStringLiteral(" mm"), 3, [=, this] { return bounds().width(); },
             [=, this](double v) {
                 const QRectF b = bounds();
                 const double h = m_lockRatio && b.width() > 0 ? b.height() * v / b.width() : b.height();
                 m_editor.setSelectionBounds(centered(b.center(), v, h));
             });
    addField(size, QStringLiteral("height"), tr("Höhe"), QStringLiteral(" mm"), 3, [=, this] { return bounds().height(); },
             [=, this](double v) {
                 const QRectF b = bounds();
                 const double w = m_lockRatio && b.height() > 0 ? b.width() * v / b.height() : b.width();
                 m_editor.setSelectionBounds(centered(b.center(), w, v));
             });

    // Skalierung in Prozent: Eingabe wirkt relativ zur jetzigen Grösse, danach wieder 100 %
    QBoxLayout* scale = addStack();
    addField(scale, QString(), tr("Skalierung waagrecht"), QStringLiteral(" %"), 1, [] { return 100.0; },
             [=, this](double v) { m_editor.scaleSelection(v / 100.0, m_lockRatio ? v / 100.0 : 1.0); });
    addField(scale, QString(), tr("Skalierung senkrecht"), QStringLiteral(" %"), 1, [] { return 100.0; },
             [=, this](double v) { m_editor.scaleSelection(m_lockRatio ? v / 100.0 : 1.0, v / 100.0); });

    QToolButton* lock = addButton(QStringLiteral("lock"), tr("Seitenverhältnis beibehalten"), {});
    lock->setCheckable(true);
    connect(lock, &QToolButton::toggled, this, [this, lock](bool on) {
        m_lockRatio = on;
        lock->setIcon(icon(on ? QStringLiteral("lock") : QStringLiteral("lock-open")));
    });
    m_refreshers.append([this, lock] {
        const QSignalBlocker block(lock);
        lock->setChecked(m_lockRatio);
        lock->setIcon(icon(m_lockRatio ? QStringLiteral("lock") : QStringLiteral("lock-open")));
    });

    addSeparator();
    const bool single = m_editor.selectedObjects().size() == 1;
    QDoubleSpinBox* angle = addField(
        m_row, QStringLiteral("rotate"), tr("Drehwinkel (gegen den Uhrzeigersinn)"), QStringLiteral(" °"), 1,
        [this] {
            const QList<const DrawObject*> objects = m_editor.selectedObjects();
            return objects.size() == 1 ? normalizedCcw(objects.first()->rotationAngle()) : 0.0;
        },
        [this](double v) { m_editor.rotateSelectionTo(v); });
    angle->setEnabled(single);

    addSeparator();
    addButton(QStringLiteral("mirror-h"), tr("Waagrecht spiegeln"),
              [this] { m_editor.mirrorSelection(Qt::Horizontal); });
    addButton(QStringLiteral("mirror-v"), tr("Senkrecht spiegeln"),
              [this] { m_editor.mirrorSelection(Qt::Vertical); });
}

void PropertyBar::buildTypeSection(const QList<PropertyDescriptor>& descriptors)
{
    using K = PropertyDescriptor::Kind;
    auto value = [this](const QString& id) {
        const QList<const DrawObject*> objects = m_editor.selectedObjects();
        return objects.isEmpty() ? QVariant() : objects.first()->propertyValue(id);
    };

    for (int i = 0; i < descriptors.size(); ++i) {
        const PropertyDescriptor& d = descriptors[i];
        if (d.separatorBefore)
            addSeparator();
        switch (d.kind) {
        case K::Length:
        case K::Angle:
        case K::Number: {
            const QString suffix = d.kind == K::Length  ? QStringLiteral(" mm")
                                   : d.kind == K::Angle ? QStringLiteral(" °")
                                                        : QString();
            auto field = [&](QBoxLayout* into, const PropertyDescriptor& p) {
                QDoubleSpinBox* spin = addField(
                    into, p.icon, p.label, suffix, d.kind == K::Number ? 0 : 3,
                    [=, this] { return value(p.id).toDouble(); },
                    [this, p](double v) { m_editor.setObjectProperty(p.id, v, p.label); });
                spin->setRange(p.minimum, p.maximum);
                spin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
                spin->setFixedWidth(84);
            };
            if (d.stackWithNext && i + 1 < descriptors.size()) {
                QBoxLayout* stack = addStack();
                field(stack, d);
                field(stack, descriptors[++i]);
            } else {
                field(m_row, d);
            }
            break;
        }
        case K::Toggle: {
            QToolButton* b = addButton(d.icon, d.label, {});
            b->setCheckable(true);
            connect(b, &QToolButton::toggled, this,
                    [this, d](bool on) { m_editor.setObjectProperty(d.id, on, d.label); });
            m_refreshers.append([=, this] {
                const QSignalBlocker block(b);
                b->setChecked(value(d.id).toBool());
            });
            break;
        }
        case K::Choice: {
            auto* group = new QButtonGroup(m_host);
            group->setExclusive(true);
            for (int c = 0; c < d.choiceIcons.size(); ++c) {
                QToolButton* b = addButton(d.choiceIcons[c], d.choiceLabels.value(c), {});
                b->setCheckable(true);
                group->addButton(b, c);
            }
            connect(group, &QButtonGroup::idClicked, this,
                    [this, d](int index) { m_editor.setObjectProperty(d.id, index, d.label); });
            m_refreshers.append([=, this] {
                const QSignalBlocker block(group);
                if (QAbstractButton* b = group->button(value(d.id).toInt()))
                    b->setChecked(true);
            });
            break;
        }
        case K::List: {
            auto* combo = new QComboBox(m_host);
            combo->addItems(d.choiceLabels);
            combo->setToolTip(d.label);
            m_row->addWidget(combo);
            connect(combo, &QComboBox::activated, this, [this, d](int index) { m_editor.setObjectProperty(d.id, index, d.label); });
            m_refreshers.append([=, this] {
                const QSignalBlocker block(combo);
                combo->setCurrentIndex(value(d.id).toInt());
            });
            break;
        }
        case K::Text: {
            auto* edit = new QLineEdit(m_host);
            edit->setPlaceholderText(d.label);
            edit->setToolTip(d.label);
            edit->setFixedWidth(d.textWidth);
            m_row->addWidget(edit);
            connect(edit, &QLineEdit::editingFinished, this, [this, d, edit, value] {
                if (edit->text() != value(d.id).toString())
                    m_editor.setObjectProperty(d.id, edit->text(), d.label);
            });
            m_refreshers.append([=, this] {
                const QSignalBlocker block(edit);
                edit->setText(value(d.id).toString());
            });
            break;
        }
        case K::Action:
            addButton(d.icon, d.label, [this, d] { emit objectActionRequested(d.id); });
            break;
        }
    }
}

void PropertyBar::buildImageSection()
{
    addSeparator();
    auto textButton = [this](const QString& iconName, const QString& text) {
        auto* b = new QToolButton(m_host);
        b->setIcon(icon(iconName));
        b->setIconSize(QSize(18, 18));
        b->setText(text);
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setAutoRaise(true);
        m_row->addWidget(b);
        return b;
    };
    QToolButton* edit = textButton(QStringLiteral("bitmap-edit"), tr("Bitmap bearbeiten …"));
    edit->setToolTip(tr("Bildanpassung: Farbe, Helligkeit, Kontrast, Tonwerte"));
    connect(edit, &QToolButton::clicked, this, &PropertyBar::editBitmapRequested);

    QToolButton* traceButton = textButton(QStringLiteral("bitmap-trace"), tr("Bitmap vektorisieren"));
    traceButton->setToolTip(tr("Bitmap in Kurven umwandeln (SuperTrace)"));
    auto* menu = new QMenu(traceButton);
    menu->addAction(icon(QStringLiteral("bitmap-trace")), tr("Blitzvektorisierung"), this,
                    [this] { emit traceRequested(int(trace::Preset::Logo), true); });
    QMenu* outline = menu->addMenu(tr("Umrissvektorisierung"));
    for (trace::Preset p : trace::presets())
        outline->addAction(trace::presetName(p) + QStringLiteral(" …"), this, [this, p] { emit traceRequested(int(p), false); });
    traceButton->setMenu(menu);
    traceButton->setPopupMode(QToolButton::InstantPopup);

    addSeparator();
    QToolButton* reset = addButton(QStringLiteral("frame-reset"), tr("Rahmen zurücksetzen (Beschneiden aufheben)"), [this] {
        if (const ImageObject* image = m_editor.selectedImage()) {
            ImageObject full(image->key(), image->size());
            m_editor.setImageFrame(image->id(), full.frame(), tr("Rahmen zurücksetzen"));
        }
    });
    m_refreshers.append([this, reset] {
        const ImageObject* image = m_editor.selectedImage();
        reset->setEnabled(image && image->isCropped());
    });
    addSeparator();
    QToolButton* wrap = addButton(QStringLiteral("wrap-text"), tr("Mengentext umbrechen – folgt"), {});
    wrap->setEnabled(false);
}

std::pair<QSlider*, QDoubleSpinBox*> PropertyBar::addEffectSlider(Effect effect, const QString& iconName,
                                                                  const QString& tooltip, double maximum, double step,
                                                                  const QString& suffix, int decimals,
                                                                  std::function<double(const Ramp&)> get,
                                                                  std::function<void(Ramp&, double)> modify,
                                                                  const QString& label)
{
    if (iconName.endsWith(QLatin1Char(':'))) {
        auto* text = new QLabel(iconName, m_host);
        text->setToolTip(tooltip);
        m_row->addWidget(text);
    } else if (!iconName.isEmpty()) {
        auto* pic = new QLabel(m_host);
        pic->setPixmap(icon(iconName).pixmap(16));
        pic->setToolTip(tooltip);
        m_row->addWidget(pic);
    }
    auto* slider = new QSlider(Qt::Horizontal, m_host);
    slider->setRange(0, int(std::lround(maximum / step)));
    slider->setFixedWidth(110);
    slider->setToolTip(tooltip);
    auto* spin = new ExpressionSpinBox(m_host);
    spin->setDecimals(decimals);
    spin->setRange(0.0, maximum);
    spin->setSuffix(suffix);
    spin->setKeyboardTracking(false);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setFixedWidth(64);
    spin->setToolTip(tooltip);
    m_row->addWidget(slider);
    m_row->addWidget(spin);

    auto current = [this, effect, get] {
        const QList<const DrawObject*> objects = m_editor.selectedObjects();
        return objects.isEmpty() ? 0.0 : get(objects.first()->effect(effect));
    };
    // Vorschau beim Ziehen: nur die Zeichenfläche, noch kein Undo-Schritt
    auto preview = [this, effect, modify](double v) {
        for (const DrawObject* o : m_editor.selectedObjects()) {
            Ramp r = o->effect(effect);
            modify(r, v);
            m_canvas.setEffectPreview(o->id(), effect, r);
        }
    };
    auto commit = [this, effect, modify, label](double v) {
        m_canvas.clearEffectPreview();
        m_editor.setSelectionEffect(effect, [&modify, v](Ramp& r) { modify(r, v); }, label);
    };
    connect(slider, &QSlider::sliderMoved, this, [=](int pos) {
        const QSignalBlocker block(spin);
        spin->setValue(pos * step);
        preview(pos * step);
    });
    connect(slider, &QSlider::sliderReleased, this, [=] { commit(slider->value() * step); });
    // Klick in die Schiene oder Pfeiltasten
    connect(slider, &QSlider::valueChanged, this, [=](int pos) {
        if (!slider->isSliderDown())
            commit(pos * step);
    });
    connect(spin, &QDoubleSpinBox::editingFinished, this, [=] {
        if (std::abs(spin->value() - current()) > 1e-9)
            commit(spin->value());
    });
    m_refreshers.append([=] {
        const double v = current();
        const QSignalBlocker a(slider);
        const QSignalBlocker b(spin);
        slider->setValue(int(std::lround(v / step)));
        spin->setValue(v);
    });
    return {slider, spin};
}

void PropertyBar::buildShapingSection()
{
    // Verschmelzen, Zuschneiden, Schnittmenge, Vereinfachen, Vorn minus hinten, Hinten minus
    // vorn, Begrenzung: wie in der Eigenschaftsleiste bei mehreren ausgewählten Objekten
    addSeparator();
    using Op = Editor::ShapeOp;
    for (auto [op, iconName] : {std::pair{Op::Weld, QStringLiteral("shape-weld")}, std::pair{Op::Trim, QStringLiteral("shape-trim")},
                                std::pair{Op::Intersect, QStringLiteral("shape-intersect")},
                                std::pair{Op::Simplify, QStringLiteral("shape-simplify")},
                                std::pair{Op::FrontMinusBack, QStringLiteral("shape-front-minus-back")},
                                std::pair{Op::BackMinusFront, QStringLiteral("shape-back-minus-front")},
                                std::pair{Op::Boundary, QStringLiteral("shape-boundary")}}) {
        QToolButton* b = addButton(iconName, Editor::shapeOpName(op), [this, op] { m_editor.shapeSelection(op); });
        m_refreshers.append([this, b, op] { b->setEnabled(m_editor.canShapeSelection(op)); });
    }
}

void PropertyBar::buildBlurSlider()
{
    // Unschärfe gilt für jedes Objekt, auch Gruppen, Text und Bilder; die Pfade bleiben scharf.
    // Bei einem Unschärfe-Verlauf stellt der Regler die stärkste Stelle ein, der Verlauf bleibt.
    addEffectSlider(Effect::Blur, QStringLiteral("blur"), tr("Unschärfe in mm (Gaussscher Weichzeichner)"), 20.0, 0.1,
                    QStringLiteral(" mm"), 1, [](const Ramp& r) { return r.maximum(); },
                    [](Ramp& r, double v) {
                        if (!r.isGradient()) {
                            r = Ramp::uniform(v);
                        } else if (const double m = r.maximum(); m > 0.0) {
                            r.from *= v / m;
                            r.to *= v / m;
                        } else {
                            r.to = v;
                        }
                    },
                    tr("Unschärfe"));
}

void PropertyBar::buildEffectSection(Effect effect)
{
    const bool blur = effect == Effect::Blur;
    auto current = [this, effect] {
        const QList<const DrawObject*> objects = m_editor.selectedObjects();
        return objects.isEmpty() ? Ramp() : objects.first()->effect(effect);
    };
    const QString label = blur ? tr("Unschärfe") : tr("Transparenz");
    m_row->addWidget(new QLabel(blur ? tr("Unschärfe:") : tr("Transparenz:"), m_host));
    using T = Ramp::Type;
    for (auto [type, iconName, text] : {std::tuple{T::None, QStringLiteral("fill-none"), tr("Keine")},
                                        std::tuple{T::Uniform, QStringLiteral("fill-uniform"), tr("Gleichmässig")},
                                        std::tuple{T::Linear, QStringLiteral("fill-linear"), tr("Linear")},
                                        std::tuple{T::Radial, QStringLiteral("fill-radial"), tr("Radial")},
                                        std::tuple{T::Conical, QStringLiteral("fill-conical"), tr("Konisch")},
                                        std::tuple{T::Pattern, QStringLiteral("fill-pattern"), tr("Muster")}}) {
        if (blur && type == T::Pattern)
            continue; // Unschärfe hat kein Muster
        QToolButton* b = addButton(iconName, text, [this, effect, type, label] {
            m_editor.setSelectionEffect(effect, [effect, type](Ramp& r) { r = EffectTool::defaultRamp(effect, type, r); },
                                        label);
        });
        b->setCheckable(true);
        m_refreshers.append([b, current, type] {
            const Ramp r = current();
            b->setChecked(r.type == type || (type == T::None && !r.isActive() && !r.isGradient()));
        });
    }
    addSeparator();
    const double maximum = blur ? 20.0 : 100.0;
    const double step = blur ? 0.1 : 1.0;
    const QString suffix = blur ? QStringLiteral(" mm") : QStringLiteral(" %");
    const int decimals = blur ? 1 : 0;
    addEffectSlider(effect, tr("Anfang:"), blur ? tr("Unschärfe am Anfang bzw. gleichmässig") : tr("Transparenz am Anfang bzw. gleichmässig"),
                    maximum, step, suffix, decimals, [](const Ramp& r) { return r.type == T::None ? 0.0 : r.from; },
                    [](Ramp& r, double v) {
                        if (r.type == T::None)
                            r = Ramp::uniform(v);
                        else
                            r.from = v;
                    },
                    label);
    const auto [endSlider, endSpin] = addEffectSlider(
        effect, tr("Ende:"), blur ? tr("Unschärfe am Ende des Verlaufs") : tr("Transparenz am Ende des Verlaufs"), maximum,
        step, suffix, decimals, [](const Ramp& r) { return r.isGradient() || r.type == T::Pattern ? r.to : 0.0; },
        [](Ramp& r, double v) {
            if (r.isGradient() || r.type == T::Pattern)
                r.to = v;
        },
        label);
    m_refreshers.append([endSlider, endSpin, current] {
        const bool gradient = current().isGradient() || current().type == T::Pattern;
        endSlider->setEnabled(gradient);
        endSpin->setEnabled(gradient);
    });
    if (!blur) {
        // Mischmodus der Transparenz
        addSeparator();
        QComboBox* mode = createBlendModeCombo(m_host, tr("Mischmodus"));
        m_row->addWidget(mode);
        connect(mode, &QComboBox::activated, this, [this, mode](int i) {
            m_editor.setSelectionBlendMode(BlendMode(mode->itemData(i).toInt()));
        });
        m_refreshers.append([this, mode] {
            const QList<const DrawObject*> objects = m_editor.selectedObjects();
            setBlendModeComboValue(mode, objects.isEmpty() ? BlendMode::Normal : objects.first()->blendMode());
        });
        // Mustertransparenz: Muster wählen
        QToolButton* choose = addButton(QStringLiteral("fill-pattern"), tr("Muster der Transparenz wählen …"), [this, current, label] {
            const Ramp r = current();
            if (r.type != T::Pattern)
                return;
            PatternFillDialog dialog(r.pattern, m_host);
            if (dialog.exec() == QDialog::Accepted) {
                const PatternFill p = dialog.pattern();
                m_editor.setSelectionEffect(Effect::Transparency, [p](Ramp& x) { x.pattern = p; }, label);
            }
        });
        m_refreshers.append([choose, current] { choose->setEnabled(current().type == T::Pattern); });
    }
    if (!blur)
        return;
    // Feinheit beim dauerhaften Umwandeln (In Kurven)
    addSeparator();
    QBoxLayout* fine = addStack();
    QDoubleSpinBox* steps = addField(fine, tr("S:"), tr("Stufenkonturen je Unschärfe beim Umwandeln in Kurven"), QString(), 0,
                                     [this] { return double(m_editor.shapeDefaults().blurSteps); },
                                     [this](double v) { m_editor.shapeDefaults().blurSteps = int(v); });
    steps->setRange(4, 256);
    QDoubleSpinBox* bands = addField(fine, tr("B:"), tr("Bänder eines Unschärfe-Verlaufs beim Umwandeln in Kurven"),
                                     QString(), 0, [this] { return double(m_editor.shapeDefaults().blurBands); },
                                     [this](double v) { m_editor.shapeDefaults().blurBands = int(v); });
    bands->setRange(1, 128);
}

void PropertyBar::buildTextSection()
{
    addSeparator();
    auto format = [this]() -> CharFormat {
        const TextTool& tool = m_canvas.textTool();
        if (tool.editing())
            return tool.currentFormat();
        const QList<const TextObject*> texts = m_editor.selectedTexts();
        if (!texts.isEmpty())
            return texts.first()->paragraphs().first().runs.first().format;
        return m_editor.shapeDefaults().artisticText;
    };
    auto apply = [this](const std::function<void(CharFormat&)>& fn, const QString& label) {
        TextTool& tool = m_canvas.textTool();
        if (tool.editing()) {
            tool.applyFormat(fn);
            m_canvas.setFocus(); // gleich weitertippen
        } else if (!m_editor.selectedTexts().isEmpty()) {
            m_editor.applyTextFormat(fn, label);
        } else {
            ShapeDefaults& d = m_editor.shapeDefaults();
            fn(d.artisticText);
            fn(d.paragraphText);
        }
        refresh();
    };

    auto* fontIcon = new QLabel(m_host);
    fontIcon->setPixmap(icon(QStringLiteral("font")).pixmap(18));
    m_row->addWidget(fontIcon);
    auto* font = new FontBox(m_editor, m_host);
    font->setToolTip(tr("Schriftart"));
    font->setFixedWidth(190);
    m_row->addWidget(font);
    connect(font, &FontBox::familyActivated, this, [apply](const QString& family) {
        apply([family](CharFormat& f) { f.family = family; }, tr("Schriftart"));
    });

    auto* size = new QComboBox(m_host);
    size->setToolTip(tr("Schriftgrösse in Punkt"));
    size->setEditable(true);
    size->setInsertPolicy(QComboBox::NoInsert);
    size->setFixedWidth(72);
    for (int pt : {6, 7, 8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 36, 40, 48, 60, 72, 96, 120, 150, 200})
        size->addItem(QStringLiteral("%1 pt").arg(pt), double(pt));
    m_row->addWidget(size);
    auto applySize = [apply](double pt) {
        if (pt > 0.0 && pt <= 3000.0)
            apply([pt](CharFormat& f) { f.sizePt = pt; }, tr("Schriftgrösse"));
    };
    connect(size, &QComboBox::activated, this, [size, applySize](int index) { applySize(size->itemData(index).toDouble()); });
    connect(size->lineEdit(), &QLineEdit::editingFinished, this, [size, applySize] {
        QString text = size->currentText();
        text.remove(QStringLiteral("pt")).replace(QLatin1Char(','), QLatin1Char('.'));
        bool ok = false;
        const double pt = text.trimmed().toDouble(&ok);
        if (ok)
            applySize(pt);
    });

    addSeparator();
    QToolButton* bold = addButton(QStringLiteral("text-bold"), tr("Fett (Strg+B)"), {});
    QToolButton* italic = addButton(QStringLiteral("text-italic"), tr("Kursiv (Strg+I)"), {});
    QToolButton* underline = addButton(QStringLiteral("text-underline"), tr("Unterstrichen (Strg+U)"), {});
    for (QToolButton* b : {bold, italic, underline})
        b->setCheckable(true);
    connect(bold, &QToolButton::clicked, this, [apply](bool on) { apply([on](CharFormat& f) { f.bold = on; }, tr("Fett")); });
    connect(italic, &QToolButton::clicked, this,
            [apply](bool on) { apply([on](CharFormat& f) { f.italic = on; }, tr("Kursiv")); });
    connect(underline, &QToolButton::clicked, this,
            [apply](bool on) { apply([on](CharFormat& f) { f.underline = on; }, tr("Unterstrichen")); });

    // Horizontale Ausrichtung (beim Bearbeiten und für ausgewählten Text)
    addSeparator();
    auto* alignGroup = new QButtonGroup(m_host);
    alignGroup->setExclusive(true);
    const QStringList alignIcons{QStringLiteral("align-left"), QStringLiteral("align-center"),
                                 QStringLiteral("align-right"), QStringLiteral("align-justify")};
    const QStringList alignTexts{tr("Linksbündig"), tr("Zentriert"), tr("Rechtsbündig"), tr("Blocksatz")};
    for (int i = 0; i < alignIcons.size(); ++i) {
        QToolButton* b = addButton(alignIcons[i], alignTexts[i], {});
        b->setCheckable(true);
        alignGroup->addButton(b, i);
    }
    connect(alignGroup, &QButtonGroup::idClicked, this, [this](int index) {
        const auto align = TextParagraph::Align(index);
        TextTool& tool = m_canvas.textTool();
        if (tool.editing()) {
            tool.setAlignment(align);
            m_canvas.setFocus();
        } else {
            m_editor.setTextAlignment(align);
        }
    });

    // Silbentrennung (nur Mengentext)
    addSeparator();
    QToolButton* hyphen = addButton(QStringLiteral("hyphenation"), tr("Silbentrennung verwenden"), {});
    hyphen->setCheckable(true);
    connect(hyphen, &QToolButton::clicked, this, [this](bool on) {
        TextTool& tool = m_canvas.textTool();
        if (tool.editing()) {
            tool.setHyphenate(on);
            m_canvas.setFocus();
        } else {
            m_editor.setTextHyphenation(on);
        }
    });

    m_refreshers.append([=, this] {
        const CharFormat f = format();
        {
            const QSignalBlocker block(font);
            font->setCurrentFamily(f.family);
        }
        {
            const QSignalBlocker block(size);
            size->setEditText(QStringLiteral("%1 pt").arg(QLocale::c().toString(f.sizePt, 'g', 4)));
        }
        bold->setChecked(f.bold);
        italic->setChecked(f.italic);
        underline->setChecked(f.underline);
        const TextTool& tool = m_canvas.textTool();
        const QList<const TextObject*> texts = m_editor.selectedTexts();
        const bool hasParagraph = tool.editing() || !texts.isEmpty();
        TextParagraph::Align align = TextParagraph::Align::Left;
        if (tool.editing())
            align = tool.currentAlignment();
        else if (!texts.isEmpty())
            align = texts.first()->paragraphs().first().align;
        bool paragraphText = tool.editing() && tool.editingKind() == TextObject::Kind::Paragraph;
        bool hyphenOn = tool.editing() && tool.hyphenate();
        for (const TextObject* t : texts) {
            if (!tool.editing() && t->kind() == TextObject::Kind::Paragraph) {
                paragraphText = true;
                hyphenOn = hyphenOn || t->hyphenate();
            }
        }
        hyphen->setEnabled(paragraphText);
        hyphen->setChecked(paragraphText && hyphenOn);
        const QSignalBlocker block(alignGroup);
        for (QAbstractButton* b : alignGroup->buttons())
            b->setEnabled(hasParagraph);
        if (QAbstractButton* b = alignGroup->button(int(align)))
            b->setChecked(true);
    });
}

void PropertyBar::buildOutlineSection()
{
    // Wie in der Objektleiste. Linien mit offenen Enden: Pfeilspitze Anfang, Linienstil,
    // Pfeilspitze Ende. Flächen (Rechteck, Ellipse, Polygon …): Mengentext umbrechen und
    // Linienstil. Danach Konturbreite, Anordnung und In Kurven.
    addSeparator();
    const bool ends = m_signature.endsWith(QLatin1String(":ends"));
    auto firstStyle = [this] {
        const QList<const DrawObject*> objects = m_editor.selectedObjects();
        return objects.isEmpty() ? m_editor.defaultStyle() : objects.first()->style();
    };
    QComboBox* startArrow = ends ? addArrowCombo(true) : nullptr;
    if (!ends) {
        QToolButton* wrap = addButton(QStringLiteral("wrap-text"), tr("Mengentext umbrechen – folgt mit dem Textwerkzeug"), {});
        wrap->setEnabled(false);
        addSeparator();
    }
    QComboBox* dash = new QComboBox(m_host);
    dash->setToolTip(tr("Linienstil"));
    dash->setIconSize(QSize(80, 12));
    for (int i = 0; i < outline::dashCount(); ++i)
        dash->addItem(dashIcon(i, dash->iconSize()), QString());
    m_row->addWidget(dash);
    QComboBox* endArrow = ends ? addArrowCombo(false) : nullptr;
    if (ends) {
        connect(startArrow, &QComboBox::activated, this,
                [this](int index) { m_editor.setSelectionOutlineShape(std::nullopt, index, std::nullopt); });
        connect(endArrow, &QComboBox::activated, this,
                [this](int index) { m_editor.setSelectionOutlineShape(std::nullopt, std::nullopt, index); });
    }
    connect(dash, &QComboBox::activated, this,
            [this](int index) { m_editor.setSelectionOutlineShape(index, std::nullopt, std::nullopt); });
    m_refreshers.append([=] {
        const Style s = firstStyle();
        for (auto [combo, value] : {std::pair{startArrow, s.startArrow}, std::pair{dash, s.dash},
                                    std::pair{endArrow, s.endArrow}}) {
            if (!combo)
                continue;
            const QSignalBlocker block(combo);
            combo->setCurrentIndex(std::clamp(value, 0, combo->count() - 1));
        }
    });

    addSeparator();
    auto* label = new QLabel(m_host);
    label->setPixmap(icon(QStringLiteral("outline-width")).pixmap(18));
    label->setToolTip(tr("Konturbreite"));
    m_row->addWidget(label);

    auto* combo = new QComboBox(m_host);
    combo->setToolTip(tr("Konturbreite"));
    combo->setEditable(true);
    combo->setInsertPolicy(QComboBox::NoInsert);
    combo->setMinimumWidth(100);
    for (double w : kOutlineWidths)
        combo->addItem(formatWidth(w), w);
    combo->insertSeparator(combo->count());
    for (double pt : kOutlineWidthsPt)
        combo->addItem(formatWidth(pt * Options::unitInMm(kUnitPt), kUnitPt), pt * Options::unitInMm(kUnitPt));
    combo->setMaxVisibleItems(combo->count());
    m_row->addWidget(combo);

    auto apply = [this](double w) {
        if (w <= 0.0)
            m_editor.setSelectionStroke(std::nullopt);
        else
            m_editor.setSelectionStrokeWidth(w);
    };
    connect(combo, &QComboBox::activated, this, [=, this](int index) { apply(combo->itemData(index).toDouble()); });
    connect(combo->lineEdit(), &QLineEdit::editingFinished, this, [=, this] {
        // Ausdruck in der Dokumenteinheit, eine angehängte Einheit ("2 pt") wird umgerechnet
        const QString text = combo->currentText().trimmed();
        if (text == formatWidth(0.0) || text == formatWidth(0.076))
            return; // unverändert (Keine, Haarlinie)
        if (const std::optional<double> w = evaluateLength(text, Options::instance().rulerUnit(true)))
            apply(*w);
    });
    m_refreshers.append([this, combo] {
        const QList<const DrawObject*> objects = m_editor.selectedObjects();
        if (objects.isEmpty())
            return;
        const Style& s = objects.first()->style();
        const QSignalBlocker block(combo);
        combo->setEditText(formatWidth(s.stroke ? s.strokeWidth : 0.0, Options::instance().rulerUnit(true)));
    });

    // Unschärfe gilt für jedes Objekt, auch Gruppen (wie der Gausssche Weichzeichner in Inkscape)
    addSeparator();
    buildBlurSlider();

    // Anordnung auf der Ebene wie die beiden Knöpfe in der Eigenschaftsleiste
    addSeparator();
    using A = Editor::Arrange;
    for (auto [how, iconName, text] : {std::tuple{A::ToLayerFront, QStringLiteral("arrange-front"), tr("Nach vorn auf der Ebene (Umschalt+Bild↑)")},
                                       std::tuple{A::ToLayerBack, QStringLiteral("arrange-back"), tr("Nach hinten auf der Ebene (Umschalt+Bild↓)")}}) {
        QToolButton* b = addButton(iconName, text, [this, how] { m_editor.arrangeSelection(how); });
        m_refreshers.append([this, b, how] { b->setEnabled(m_editor.canArrange(how)); });
    }

    // Immer vorhanden, aber nur aktiv, wenn es etwas umzuwandeln gibt (die Leiste wird je
    // Zusammensetzung aufbewahrt, nicht je Objekt)
    addSeparator();
    QToolButton* toCurves = addButton(QStringLiteral("to-curves"), tr("In Kurven konvertieren (Strg+Q)"),
                                      [this] { m_editor.convertSelectionToCurves(); });
    m_refreshers.append([this, toCurves] { toCurves->setEnabled(m_editor.canConvertSelectionToCurves()); });
}

QComboBox* PropertyBar::addArrowCombo(bool atStart)
{
    // Klappliste als Raster mit drei Spalten wie die Pfeilspitzen-Auswahl
    auto* combo = new QComboBox(m_host);
    combo->setToolTip(atStart ? tr("Pfeilspitze am Anfang") : tr("Pfeilspitze am Ende"));
    combo->setIconSize(QSize(34, 14));
    auto* view = new QListView(combo);
    view->setViewMode(QListView::IconMode);
    view->setFlow(QListView::LeftToRight);
    view->setWrapping(true);
    view->setResizeMode(QListView::Adjust);
    view->setMovement(QListView::Static);
    view->setIconSize(QSize(40, 18));
    view->setGridSize(QSize(50, 28));
    view->setSpacing(0);
    view->setMinimumWidth(3 * 50 + 22);
    combo->setView(view);
    for (int i = 0; i < outline::arrowCount(); ++i) {
        combo->addItem(arrowIcon(i, atStart, QSize(40, 18)), QString());
        combo->setItemData(i, outline::arrowName(i), Qt::ToolTipRole);
    }
    m_row->addWidget(combo);
    return combo;
}

QIcon PropertyBar::arrowIcon(int index, bool atStart, const QSize& size) const
{
    const qreal dpr = devicePixelRatioF();
    static QHash<QString, QIcon> cache;
    const QString key = QStringLiteral("%1/%2/%3x%4/%5").arg(index).arg(atStart).arg(size.width()).arg(size.height()).arg(dpr);
    if (auto it = cache.constFind(key); it != cache.constEnd())
        return *it;
    QPixmap pix(size * dpr);
    pix.setDevicePixelRatio(dpr);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    const double y = size.height() / 2.0;
    QPainterPath line;
    line.moveTo(atStart ? size.width() - 3.0 : 3.0, y);
    line.lineTo(atStart ? 6.0 : size.width() - 6.0, y);
    Style s;
    s.strokeWidth = 1.4;
    s.endArrow = index;
    const outline::Stroke stroke = outline::withArrows(line, s);
    p.setPen(QPen(QColor(0x1d, 0x1d, 0x1d), s.strokeWidth, Qt::SolidLine, Qt::FlatCap));
    p.drawPath(stroke.line);
    p.fillPath(stroke.heads, QColor(0x1d, 0x1d, 0x1d));
    p.end();
    return *cache.insert(key, QIcon(pix));
}

QIcon PropertyBar::dashIcon(int index, const QSize& size) const
{
    const qreal dpr = devicePixelRatioF();
    static QHash<QString, QIcon> cache;
    const QString key = QStringLiteral("%1/%2x%3/%4").arg(index).arg(size.width()).arg(size.height()).arg(dpr);
    if (auto it = cache.constFind(key); it != cache.constEnd())
        return *it;
    QPixmap pix(size * dpr);
    pix.setDevicePixelRatio(dpr);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    QPen pen(QColor(0x1d, 0x1d, 0x1d), 2.0, Qt::SolidLine, Qt::FlatCap);
    if (const QList<double> d = outline::dashPattern(index); !d.isEmpty())
        pen.setDashPattern(d);
    p.setPen(pen);
    p.drawLine(QPointF(2, size.height() / 2.0), QPointF(size.width() - 2, size.height() / 2.0));
    p.end();
    return *cache.insert(key, QIcon(pix));
}

} // namespace vek

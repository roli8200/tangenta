#include "Canvas.h"

#include "Blur.h"

#include "GroupObject.h"
#include "ImageObject.h"
#include "ImagePool.h"
#include "CurveObject.h"
#include "DimensionObject.h"
#include "InteractiveFillTool.h"
#include "ArtisticMediaTool.h"
#include "BezierTool.h"
#include "DrawingTools.h"
#include "ShapingTools.h"
#include "PerfectShapeTool.h"
#include "CutTools.h"
#include "DimensionTool.h"
#include "ConnectorTool.h"
#include "SmartDrawingTool.h"
#include "TableObject.h"
#include "CloneObject.h"
#include "SuperClipObject.h"
#include "BlendObject.h"
#include "TableTool.h"
#include "MeshTool.h"
#include "PipetteTools.h"
#include "PerspectiveTool.h"
#include "ShadowTool.h"
#include "InteractiveTools.h"
#include "Options.h"
#include "SuperClipBar.h"
#include "EffectTool.h"
#include "FreehandTool.h"
#include "Outline.h"
#include "PathOps.h"
#include "PickTool.h"
#include "ShapeTool.h"
#include "RectangleTool.h"
#include "TextObject.h"
#include "TextTool.h"
#include "ZoomTool.h"

#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMimeData>
#include <QPaintEvent>
#include <QScreen>
#include <QWheelEvent>
#include <QPainterPathStroker>
#include <QLinearGradient>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace vek {

namespace {

const QColor kDesktop(0xff, 0xff, 0xff);
const QColor kPageBorder(0x9a, 0x9a, 0x9a);
const QColor kPageShadow(0xc8, 0xc8, 0xc8);
const QColor kGuide(0x2b, 0x5c, 0xd6);
const QColor kGuideSelected(0xe0, 0x20, 0x20);
constexpr double kMinZoom = 0.05; // Pixel je mm
constexpr double kMaxZoom = 2000.0;

} // namespace

Canvas::Canvas(Editor& editor, QWidget* parent)
    : QWidget(parent), m_editor(editor)
{
    m_tools[static_cast<std::size_t>(ToolId::Pick)] = std::make_unique<PickTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::Shape)] = std::make_unique<ShapeTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::Zoom)] = std::make_unique<ZoomTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::Freehand)] = std::make_unique<FreehandTool>(*this, FreehandTool::Mode::Freehand);
    m_tools[static_cast<std::size_t>(ToolId::Line)] = std::make_unique<FreehandTool>(*this, FreehandTool::Mode::Line);
    m_tools[static_cast<std::size_t>(ToolId::Bezier)] = std::make_unique<BezierTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::BSpline)] = std::make_unique<BSplineTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::Pen)] = std::make_unique<BezierTool>(*this, true);
    using BM = BrushShapingTool::Mode;
    m_tools[static_cast<std::size_t>(ToolId::Smear)] = std::make_unique<BrushShapingTool>(*this, BM::Smear);
    m_tools[static_cast<std::size_t>(ToolId::Smudge)] = std::make_unique<BrushShapingTool>(*this, BM::Smudge);
    m_tools[static_cast<std::size_t>(ToolId::Roughen)] = std::make_unique<BrushShapingTool>(*this, BM::Roughen);
    m_tools[static_cast<std::size_t>(ToolId::Twirl)] = std::make_unique<BrushShapingTool>(*this, BM::Twirl);
    m_tools[static_cast<std::size_t>(ToolId::Attract)] = std::make_unique<BrushShapingTool>(*this, BM::Attract);
    m_tools[static_cast<std::size_t>(ToolId::Repel)] = std::make_unique<BrushShapingTool>(*this, BM::Repel);
    m_tools[static_cast<std::size_t>(ToolId::FreeTransform)] = std::make_unique<FreeTransformTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::Polyline)] = std::make_unique<PolylineTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::SmartFill)] = std::make_unique<SmartFillTool>(*this);
    using PC = perfect::Category;
    m_tools[static_cast<std::size_t>(ToolId::PerfectBasic)] = std::make_unique<PerfectShapeTool>(*this, PC::Basic);
    m_tools[static_cast<std::size_t>(ToolId::PerfectArrow)] = std::make_unique<PerfectShapeTool>(*this, PC::Arrow);
    m_tools[static_cast<std::size_t>(ToolId::PerfectFlowchart)] = std::make_unique<PerfectShapeTool>(*this, PC::Flowchart);
    m_tools[static_cast<std::size_t>(ToolId::PerfectBanner)] = std::make_unique<PerfectShapeTool>(*this, PC::Banner);
    m_tools[static_cast<std::size_t>(ToolId::PerfectCallout)] = std::make_unique<PerfectShapeTool>(*this, PC::Callout);
    using TP = ThreePointTool::Shape;
    m_tools[static_cast<std::size_t>(ToolId::ThreePointCurve)] = std::make_unique<ThreePointTool>(*this, TP::Curve);
    m_tools[static_cast<std::size_t>(ToolId::ThreePointRectangle)] = std::make_unique<ThreePointTool>(*this, TP::Rectangle);
    m_tools[static_cast<std::size_t>(ToolId::ThreePointEllipse)] = std::make_unique<ThreePointTool>(*this, TP::Ellipse);
    m_tools[static_cast<std::size_t>(ToolId::VirtualDelete)] = std::make_unique<VirtualDeleteTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::ArtisticMedia)] = std::make_unique<ArtisticMediaTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::Crop)] = std::make_unique<CropTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::Knife)] = std::make_unique<KnifeTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::Eraser)] = std::make_unique<EraserTool>(*this);
    using S = RectangleTool::Shape;
    m_tools[static_cast<std::size_t>(ToolId::Rectangle)] = std::make_unique<RectangleTool>(*this, S::Rectangle);
    m_tools[static_cast<std::size_t>(ToolId::Ellipse)] = std::make_unique<RectangleTool>(*this, S::Ellipse);
    m_tools[static_cast<std::size_t>(ToolId::Polygon)] = std::make_unique<RectangleTool>(*this, S::Polygon);
    m_tools[static_cast<std::size_t>(ToolId::Star)] = std::make_unique<RectangleTool>(*this, S::Star);
    m_tools[static_cast<std::size_t>(ToolId::ComplexStar)] = std::make_unique<RectangleTool>(*this, S::ComplexStar);
    m_tools[static_cast<std::size_t>(ToolId::Spiral)] = std::make_unique<RectangleTool>(*this, S::Spiral);
    m_tools[static_cast<std::size_t>(ToolId::GraphPaper)] = std::make_unique<RectangleTool>(*this, S::GraphPaper);
    m_tools[static_cast<std::size_t>(ToolId::Pan)] = std::make_unique<PanTool>(*this);
    using DM = DimensionTool::Mode;
    m_tools[static_cast<std::size_t>(ToolId::DimensionParallel)] = std::make_unique<DimensionTool>(*this, DM::Parallel);
    m_tools[static_cast<std::size_t>(ToolId::DimensionHV)] = std::make_unique<DimensionTool>(*this, DM::HorizontalVertical);
    m_tools[static_cast<std::size_t>(ToolId::DimensionAngle)] = std::make_unique<DimensionTool>(*this, DM::Angular);
    using CK = ConnectorObject::Kind;
    m_tools[static_cast<std::size_t>(ToolId::Connector)] = std::make_unique<ConnectorTool>(*this, CK::Straight);
    m_tools[static_cast<std::size_t>(ToolId::ConnectorAngle)] = std::make_unique<ConnectorTool>(*this, CK::RightAngle);
    m_tools[static_cast<std::size_t>(ToolId::ConnectorRound)] = std::make_unique<ConnectorTool>(*this, CK::RoundedRightAngle);
    m_tools[static_cast<std::size_t>(ToolId::SmartDrawing)] = std::make_unique<SmartDrawingTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::Table)] = std::make_unique<TableTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::Mesh)] = std::make_unique<MeshTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::ColorPipette)] = std::make_unique<ColorPipetteTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::AttributePipette)] = std::make_unique<AttributePipetteTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::Perspective)] = std::make_unique<PerspectiveTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::DropShadow)] = std::make_unique<ShadowTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::Blend)] = std::make_unique<BlendTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::Contour)] = std::make_unique<ContourTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::Distort)] = std::make_unique<DistortTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::Envelope)] = std::make_unique<EnvelopeTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::Extrude)] = std::make_unique<ExtrudeTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::InteractiveFill)] = std::make_unique<InteractiveFillTool>(*this);
    m_tools[static_cast<std::size_t>(ToolId::Transparency)] = std::make_unique<EffectTool>(*this, Effect::Transparency);
    m_tools[static_cast<std::size_t>(ToolId::Blur)] = std::make_unique<EffectTool>(*this, Effect::Blur);
    m_tools[static_cast<std::size_t>(ToolId::Text)] = std::make_unique<TextTool>(*this);

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAcceptDrops(true); // Bilder aus dem Dateimanager oder Browser hineinziehen
    auto refresh = [this] { update(); };
    connect(&m_editor, &Editor::documentChanged, this, [this] { m_effectCache.clear(); });
    connect(&m_editor, &Editor::documentReplaced, this, [this] { m_effectCache.clear(); });
    connect(&m_editor, &Editor::documentChanged, this, refresh);
    connect(&m_editor, &Editor::guideSelectionChanged, this, refresh);
    connect(&m_editor, &Editor::selectionChanged, this, refresh);
    connect(&m_editor, &Editor::currentPageChanged, this, refresh);
    connect(&m_editor, &Editor::superClipEditChanged, this, refresh);
    m_clipBar = new SuperClipBar(*this);
    connect(&m_editor, &Editor::documentReplaced, this, [this] {
        m_fitPending = true;
        update();
    });
    tool()->activate();
}

Canvas::~Canvas() = default;

void Canvas::setTool(ToolId id)
{
    if (id == m_toolId)
        return;
    tool()->deactivate();
    m_toolId = id;
    if (id == ToolId::Rectangle || id == ToolId::Ellipse || id == ToolId::Polygon || id == ToolId::Star
        || id == ToolId::ComplexStar || id == ToolId::Spiral || id == ToolId::GraphPaper || id == ToolId::Freehand || id == ToolId::Line || id == ToolId::Bezier
        || id == ToolId::ArtisticMedia)
        m_lastDrawingTool = id;
    tool()->activate();
    emit toolChanged(id);
    update();
}

QTransform Canvas::viewTransform() const
{
    return QTransform(m_zoom, 0, 0, m_zoom, m_origin.x(), m_origin.y());
}

QPointF Canvas::toDoc(const QPointF& screen) const
{
    return (screen - m_origin) / m_zoom;
}

QPointF Canvas::toScreen(const QPointF& doc) const
{
    return doc * m_zoom + m_origin;
}

double Canvas::pixelsPerMm100() const
{
    // 100 % = wirkliche Grösse am Bildschirm ("Zoom relativ zu 1:1"), sonst 96 dpi; dazu die
    // Kalibrierung aus Optionen > Zoom/Schwenken > Lineale kalibrieren
    const Options& o = Options::instance();
    const double calibration = std::max(0.1, o.number(QStringLiteral("ws/tools/zoom/calibration")));
    const QScreen* sc = screen();
    if (!o.flag(QStringLiteral("ws/tools/zoom/relative")))
        return (sc ? sc->logicalDotsPerInchX() : 96.0) / 25.4 * calibration;
    return physicalPixelsPerMm() * calibration;
}

double Canvas::physicalPixelsPerMm() const
{
    // Wirkliche Pixeldichte des Bildschirms (Millimeter auf dem Lineal = Millimeter auf dem Bildschirm),
    // in geräteunabhängigen Pixeln
    const QScreen* s = screen();
    if (!s || s->physicalDotsPerInchX() < 20)
        return 96.0 / 25.4;
    return s->physicalDotsPerInchX() / s->devicePixelRatio() / 25.4;
}

double Canvas::zoomPercent() const
{
    return m_zoom / pixelsPerMm100() * 100.0;
}

void Canvas::setZoomPercent(double percent)
{
    zoomBy(percent / 100.0 * pixelsPerMm100() / m_zoom, rect().center());
}

void Canvas::fitPage()
{
    const QSizeF page = m_editor.currentPage().size();
    zoomToRect(QRectF(QPointF(0, 0), page).adjusted(-page.width() * 0.04, -page.height() * 0.04,
                                                    page.width() * 0.04, page.height() * 0.04));
}

void Canvas::zoomToRect(const QRectF& docRect)
{
    if (docRect.width() <= 0 || docRect.height() <= 0 || width() <= 0 || height() <= 0)
        return;
    m_zoom = std::clamp(std::min(width() / docRect.width(), height() / docRect.height()), kMinZoom, kMaxZoom);
    m_origin = QPointF(width() / 2.0, height() / 2.0) - docRect.center() * m_zoom;
    update();
    emit viewChanged();
}

void Canvas::zoomBy(double factor, const QPointF& screenAnchor)
{
    const QPointF anchorDoc = toDoc(screenAnchor);
    m_zoom = std::clamp(m_zoom * factor, kMinZoom, kMaxZoom);
    m_origin = screenAnchor - anchorDoc * m_zoom; // Punkt unter dem Mauszeiger bleibt stehen
    update();
    emit viewChanged();
}

void Canvas::setGuidesVisible(bool visible)
{
    m_guidesVisible = visible;
    update();
}

void Canvas::setGuidePreview(std::optional<Guide> guide)
{
    m_guidePreview = guide;
    update();
}

int Canvas::guideAt(const QPointF& screen) const
{
    if (!m_guidesVisible)
        return -1;
    const QList<Guide>& guides = m_editor.guides();
    const QPointF doc = toDoc(screen);
    for (int i = guides.size() - 1; i >= 0; --i) {
        if (std::abs(guides[i].distance(doc)) * m_zoom <= 3.0)
            return i;
    }
    return -1;
}

bool Canvas::guideSegment(const Guide& guide, double inset, QPointF* a, QPointF* b) const
{
    // Lange Strecke durch den Anker, dann auf das (verkleinerte) Fenster zuschneiden (Liang-Barsky)
    const QRectF box = QRectF(rect()).adjusted(inset, inset, -inset, -inset);
    const QPointF anchor = toScreen(guide.anchor);
    const QPointF d = guide.direction();
    const double reach = std::hypot(width(), height()) + std::hypot(anchor.x(), anchor.y());
    const QPointF p0 = anchor - d * reach;
    const QPointF delta = d * (2 * reach);
    double t0 = 0.0;
    double t1 = 1.0;
    const double p[4] = {-delta.x(), delta.x(), -delta.y(), delta.y()};
    const double q[4] = {p0.x() - box.left(), box.right() - p0.x(), p0.y() - box.top(), box.bottom() - p0.y()};
    for (int i = 0; i < 4; ++i) {
        if (p[i] == 0.0) {
            if (q[i] < 0)
                return false;
            continue;
        }
        const double t = q[i] / p[i];
        if (p[i] < 0)
            t0 = std::max(t0, t);
        else
            t1 = std::min(t1, t);
    }
    if (t0 > t1)
        return false;
    *a = p0 + delta * t0;
    *b = p0 + delta * t1;
    return true;
}

bool Canvas::offerPaletteColor(const std::optional<Color>& color, bool fill)
{
    return tool()->paletteColor(color, fill);
}

void Canvas::resizeEvent(QResizeEvent*)
{
    if (m_fitPending && width() > 0 && height() > 0) {
        m_fitPending = false;
        fitPage();
    }
    emit viewChanged();
}

void Canvas::paintEvent(QPaintEvent*)
{
    if (m_fitPending && isVisible()) {
        m_fitPending = false;
        fitPage();
    }
    QPainter p(this);
    p.fillRect(rect(), kDesktop);

    const Page& page = m_editor.currentPage();
    const QRectF pageRect = viewTransform().mapRect(QRectF(QPointF(0, 0), page.size()));
    const Options& opt = Options::instance();
    const bool border = opt.flag(QStringLiteral("doc/page/showBorder"));
    const ViewMode mode = viewMode();
    // Kantenglättung nur in Erweitert (und im Pixelmodus über das Zwischenbild)
    const bool draft = mode != ViewMode::Enhanced;
    // Doppelseiten: Partnerseite daneben (nur Anzeige; Klick darauf wechselt die Seite)
    const auto [partner, partnerOffset] = facingPartner();
    if (partner >= 0) {
        const QRectF partnerRect = pageRect.translated(partnerOffset * m_zoom, 0);
        if (border)
            p.fillRect(partnerRect.translated(4, 4), kPageShadow);
        p.save();
        p.translate(partnerOffset * m_zoom, 0);
        paintBackground(p, pageRect);
        p.setRenderHint(QPainter::Antialiasing, !draft);
        paintPage(p, m_editor.document().masterPage());
        paintPage(p, m_editor.document().page(partner));
        p.restore();
        if (border) {
            p.setPen(QPen(kPageBorder, 1));
            p.setBrush(Qt::NoBrush);
            p.drawRect(partnerRect);
        }
    }
    if (border)
        p.fillRect(pageRect.translated(4, 4), kPageShadow);
    // Hintergrund der Seite (Dokument > Hintergrund: Farbe oder Bitmap)
    paintBackground(p, pageRect);
    paintGrid(p, pageRect);

    if (mode == ViewMode::Pixel) {
        // Pixel: die Seite so, wie sie als Bitmap mit 96 dpi aussähe, vergrössert ohne Glättung
        const double pxPerMm = 96.0 / 25.4;
        const QRectF visible = pageRect.intersected(QRectF(rect()));
        if (!visible.isEmpty()) {
            const QRectF docArea(toDoc(visible.topLeft()), toDoc(visible.bottomRight()));
            const QSize size(std::max(1, int(std::ceil(docArea.width() * pxPerMm))), std::max(1, int(std::ceil(docArea.height() * pxPerMm))));
            QImage pixels(size, QImage::Format_ARGB32_Premultiplied);
            pixels.fill(Qt::transparent);
            {
                QPainter q(&pixels);
                renderPage(q, m_editor.currentPageIndex(), pxPerMm, -docArea.topLeft() * pxPerMm, false);
            }
            p.save();
            p.setRenderHint(QPainter::SmoothPixmapTransform, false);
            p.drawImage(QRectF(toScreen(docArea.topLeft()), QSizeF(size.width() / pxPerMm * m_zoom, size.height() / pxPerMm * m_zoom)), pixels);
            p.restore();
        }
    } else {
        p.setRenderHint(QPainter::Antialiasing, !draft); // Entwurf/Normal ohne Kantenglättung
        paintPage(p, m_editor.document().masterPage());
        paintPage(p, page);
        p.setRenderHint(QPainter::Antialiasing, false);
    }

    // Randanschnitt: gestrichelter Rahmen ausserhalb der Seite
    const double bleed = opt.number(QStringLiteral("doc/page/bleed"));
    if (bleed > 0 && opt.flag(QStringLiteral("doc/page/showBleed"))) {
        p.setPen(QPen(QColor(0xc0, 0x30, 0x30), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(pageRect.adjusted(-bleed * m_zoom, -bleed * m_zoom, bleed * m_zoom, bleed * m_zoom));
    }

    if (border) {
        p.setPen(QPen(kPageBorder, 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(pageRect);
    }

    paintSuperClipEdit(p);
    paintGuides(p);
    p.setRenderHint(QPainter::Antialiasing);
    tool()->paintOverlay(p);
    updateClipBar();
}

void Canvas::updateClipBar()
{
    // Ausgewählter SuperClip (Auswahlwerkzeug) bzw. der, dessen Inhalt man gerade bearbeitet
    const NodeId editing = m_editor.editedSuperClip();
    const SuperClipObject* clip = m_editor.document().findAs<SuperClipObject>(editing);
    if (!clip && m_toolId == ToolId::Pick) {
        const QList<const DrawObject*> sel = m_editor.selectedObjects();
        clip = sel.size() == 1 ? node_cast<SuperClipObject>(sel.first()) : nullptr;
    }
    const bool show = clip != nullptr;
    const bool locked = clip && clip->contentsLocked();
    QPoint pos;
    if (clip) {
        const QRectF r = viewTransform().mapRect(clip->boundingRect());
        const QSize s = m_clipBar->sizeHint();
        pos = QPoint(int(r.center().x() - s.width() / 2.0), int(r.bottom() + 14));
        pos.setX(std::clamp(pos.x(), 2, std::max(2, width() - s.width() - 2)));
        pos.setY(std::clamp(pos.y(), 2, std::max(2, height() - s.height() - 2)));
    }
    const ClipBarState state{show, pos, editing != InvalidNodeId, locked};
    if (state == m_clipBarState)
        return;
    m_clipBarState = state;
    // Nicht mitten im Zeichnen umbauen
    QMetaObject::invokeMethod(
        this,
        [this, state] {
            m_clipBar->sync(state.editing, state.locked);
            m_clipBar->move(state.pos);
            m_clipBar->setVisible(state.show);
        },
        Qt::QueuedConnection);
}

void Canvas::paintSuperClipEdit(QPainter& p) const
{
    // SuperClip bearbeiten: ganzer Inhalt blass (auch ausserhalb), Rahmen gestrichelt blau
    const auto* clip = m_editor.document().findAs<SuperClipObject>(m_editor.editedSuperClip());
    if (!clip || !clip->frame())
        return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(0.35);
    for (const DrawObject* c : clip->contents())
        paintObject(p, *c);
    p.setOpacity(1.0);
    p.setPen(QPen(previewColor(), 1.5, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawPath(viewTransform().map(clip->frame()->path()));
    p.restore();
}

void Canvas::paintGuides(QPainter& p) const
{
    auto line = [&](const Guide& g, const QColor& color) {
        QPointF a;
        QPointF b;
        if (!guideSegment(g, 0.0, &a, &b))
            return;
        QPen pen(color, 1, Qt::DashLine);
        pen.setDashPattern({4, 3});
        p.setPen(pen);
        if (g.isHorizontal() || g.isVertical()) { // pixelgenau, ohne Kantenglättung
            a = QPointF(std::round(a.x()) + 0.5, std::round(a.y()) + 0.5);
            b = QPointF(std::round(b.x()) + 0.5, std::round(b.y()) + 0.5);
            if (g.isHorizontal())
                b.setY(a.y());
            else
                b.setX(a.x());
        }
        p.drawLine(a, b);
    };
    if (m_guidesVisible) {
        const QList<Guide>& guides = m_editor.guides();
        for (int i = 0; i < guides.size(); ++i)
            line(guides[i], i == m_editor.selectedGuide() ? kGuideSelected
                            : Options::instance().color(guides[i].preset ? QStringLiteral("doc/guides/presetColor")
                                                                         : QStringLiteral("doc/guides/color")));
    }
    if (m_guidePreview)
        line(*m_guidePreview, previewColor());
}

QImage Canvas::backgroundImage() const
{
    const QString source = Options::instance().text(QStringLiteral("doc/background/bitmap"));
    if (source != m_backgroundSource) {
        m_backgroundSource = source;
        if (source.startsWith(QLatin1String("pool:")))
            m_backgroundImage = imagepool::displayImage(source.mid(5));
        else
            m_backgroundImage = source.isEmpty() ? QImage() : QImage(source).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }
    return m_backgroundImage;
}

void Canvas::paintBackground(QPainter& p, const QRectF& pageRect) const
{
    const Options& opt = Options::instance();
    const int type = opt.integer(QStringLiteral("doc/background/type"));
    p.fillRect(pageRect, type == 1 ? opt.color(QStringLiteral("doc/background/color")) : QColor(Qt::white));
    if (type != 2)
        return;
    const QImage image = backgroundImage();
    if (image.isNull())
        return;
    // Standardgrösse = Auflösung der Datei (sonst 96 dpi), sonst angepasste Grösse; gekachelt
    QSizeF tile;
    if (opt.flag(QStringLiteral("doc/background/customSize"))) {
        tile = QSizeF(opt.number(QStringLiteral("doc/background/width")), opt.number(QStringLiteral("doc/background/height")));
    } else {
        const double dpm = image.dotsPerMeterX() > 0 ? image.dotsPerMeterX() : 96.0 / 0.0254;
        tile = QSizeF(image.width() / dpm * 1000.0, image.height() / dpm * 1000.0);
    }
    if (tile.width() <= 0.01 || tile.height() <= 0.01)
        return;
    const double scale = pageRect.width() / std::max(1e-9, m_editor.currentPage().size().width()); // px je mm
    const QSizeF tilePx = tile * scale;
    if (tilePx.width() < 2 || tilePx.height() < 2)
        return;
    p.save();
    p.setClipRect(pageRect, Qt::IntersectClip);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    for (double y = pageRect.top(); y < pageRect.bottom(); y += tilePx.height())
        for (double x = pageRect.left(); x < pageRect.right(); x += tilePx.width())
            p.drawImage(QRectF(QPointF(x, y), tilePx), image);
    p.restore();
}

std::pair<int, double> Canvas::facingPartner() const
{
    const Options& opt = Options::instance();
    if (!opt.flag(QStringLiteral("doc/layout/facing")) || m_rendering)
        return {-1, 0.0};
    const int n = m_editor.currentPageIndex();
    const int count = m_editor.document().pageCount();
    // Beginnen bei rechter Seite: Seite 1 allein rechts, dann Paare (2,3), (4,5) …
    const bool startRight = opt.integer(QStringLiteral("doc/layout/startOn")) == 1;
    const bool left = startRight ? n % 2 == 1 : n % 2 == 0;
    const int partner = left ? n + 1 : n - 1;
    if (partner < 0 || partner >= count)
        return {-1, 0.0};
    const double width = m_editor.currentPage().size().width();
    return {partner, left ? width : -width};
}

void Canvas::paintObjectDirect(QPainter& p, const DrawObject& o) const
{
    m_rendering = true;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    paintObject(p, o);
    p.restore();
    m_rendering = false;
}

void Canvas::paintObjectPreview(QPainter& p, const DrawObject& o, const QTransform& extra) const
{
    // Ansicht vorübergehend so verschieben, dass das Objekt an der neuen Lage erscheint
    // (reine Verschiebung; bei Drehen/Skalieren genügt der blaue Umriss)
    if (extra.type() > QTransform::TxTranslate)
        return;
    auto* self = const_cast<Canvas*>(this);
    const QPointF origin = m_origin;
    self->m_origin += QPointF(extra.dx(), extra.dy()) * m_zoom;
    m_rendering = true;
    p.save();
    p.setOpacity(0.6);
    p.setRenderHint(QPainter::Antialiasing);
    paintObject(p, o);
    p.restore();
    m_rendering = false;
    self->m_origin = origin;
}

void Canvas::renderPage(QPainter& p, int pageIndex, double pxPerMm, const QPointF& offset, bool background) const
{
    // Ansicht vorübergehend auf das Gerät umstellen (const: wird danach wiederhergestellt)
    auto* self = const_cast<Canvas*>(this);
    const double zoom = m_zoom;
    const QPointF origin = m_origin;
    const QList<NodeId> hidden = m_hidden;
    self->m_zoom = pxPerMm;
    self->m_origin = offset;
    self->m_hidden.clear();
    m_rendering = true;
    m_output = true;
    m_renderArea = QRectF(QPointF(0, 0), QSizeF(p.device()->width(), p.device()->height()));
    const Page& page = m_editor.document().page(std::clamp(pageIndex, 0, m_editor.document().pageCount() - 1));
    const QRectF pageRect = viewTransform().mapRect(QRectF(QPointF(0, 0), page.size()));
    if (background && Options::instance().flag(QStringLiteral("doc/background/printExport")))
        paintBackground(p, pageRect);
    p.setRenderHint(QPainter::Antialiasing);
    paintPage(p, m_editor.document().masterPage());
    paintPage(p, page);
    m_renderArea.reset();
    m_rendering = false;
    m_output = false;
    self->m_zoom = zoom;
    self->m_origin = origin;
    self->m_hidden = hidden;
}

void Canvas::paintGrid(QPainter& p, const QRectF& pageRect) const
{
    const Options& opt = Options::instance();
    const QRectF shown = pageRect.intersected(paintArea());
    // Pixelgitter (96 dpi) ab 800 %
    if (opt.flag(QStringLiteral("doc/grid/pixel/show")) && zoomPercent() >= 800.0 && !shown.isEmpty()) {
        QColor c = opt.color(QStringLiteral("doc/grid/pixel/color"));
        c.setAlphaF(std::clamp(opt.number(QStringLiteral("doc/grid/pixel/opacity")) / 100.0, 0.0, 1.0));
        p.save();
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(QPen(c, 1));
        const double px = 25.4 / 96.0;
        const QRectF d(toDoc(shown.topLeft()), toDoc(shown.bottomRight()));
        for (double x = std::ceil(d.left() / px) * px; x <= d.right(); x += px)
            p.drawLine(QPointF(std::round(toScreen(QPointF(x, 0)).x()) + 0.5, shown.top()), QPointF(std::round(toScreen(QPointF(x, 0)).x()) + 0.5, shown.bottom()));
        for (double y = std::ceil(d.top() / px) * px; y <= d.bottom(); y += px)
            p.drawLine(QPointF(shown.left(), std::round(toScreen(QPointF(0, y)).y()) + 0.5), QPointF(shown.right(), std::round(toScreen(QPointF(0, y)).y()) + 0.5));
        p.restore();
    }
    // Basisliniengitter
    if (opt.flag(QStringLiteral("doc/grid/baseline/show")) && !shown.isEmpty()) {
        const double spacing = opt.number(QStringLiteral("doc/grid/baseline/spacing")) * 25.4 / 72.0;
        if (spacing * m_zoom >= 3.0) {
            p.save();
            p.setRenderHint(QPainter::Antialiasing, false);
            p.setPen(QPen(opt.color(QStringLiteral("doc/grid/baseline/color")).darker(115), 1));
            for (double y = opt.number(QStringLiteral("doc/grid/baseline/top")); y <= m_editor.currentPage().size().height(); y += spacing) {
                const double sy = std::round(toScreen(QPointF(0, y)).y()) + 0.5;
                if (sy >= shown.top() && sy <= shown.bottom())
                    p.drawLine(QPointF(pageRect.left(), sy), QPointF(pageRect.right(), sy));
            }
            p.restore();
        }
    }
    if (!opt.flag(QStringLiteral("doc/grid/show")))
        return;
    const bool spacing = opt.integer(QStringLiteral("doc/grid/mode")) == 1;
    const double h = opt.number(QStringLiteral("doc/grid/h"));
    const double v = opt.number(QStringLiteral("doc/grid/v"));
    if (h <= 0 || v <= 0)
        return;
    QSizeF step = spacing ? QSizeF(h, v) : QSizeF(1.0 / h, 1.0 / v);
    // Zu dicht: nur jede n-te Linie beim Herauszoomen
    while (step.width() * m_zoom < 6.0)
        step.rwidth() *= 2.0;
    while (step.height() * m_zoom < 6.0)
        step.rheight() *= 2.0;
    const QRectF visible = pageRect.intersected(paintArea());
    if (visible.isEmpty())
        return;
    const QRectF doc = QRectF(toDoc(visible.topLeft()), toDoc(visible.bottomRight()));
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    const QColor color(0xc8, 0xd2, 0xe6);
    const int firstX = int(std::ceil(doc.left() / step.width()));
    const int lastX = int(std::floor(doc.right() / step.width()));
    const int firstY = int(std::ceil(doc.top() / step.height()));
    const int lastY = int(std::floor(doc.bottom() / step.height()));
    if (opt.integer(QStringLiteral("doc/grid/dots")) == 1) {
        p.setPen(QPen(color.darker(130), 1));
        for (int i = firstX; i <= lastX; ++i)
            for (int j = firstY; j <= lastY; ++j)
                p.drawPoint(toScreen(QPointF(i * step.width(), j * step.height())).toPoint());
    } else {
        p.setPen(QPen(color, 1));
        for (int i = firstX; i <= lastX; ++i) {
            const double x = std::round(toScreen(QPointF(i * step.width(), 0)).x()) + 0.5;
            p.drawLine(QPointF(x, visible.top()), QPointF(x, visible.bottom()));
        }
        for (int j = firstY; j <= lastY; ++j) {
            const double y = std::round(toScreen(QPointF(0, j * step.height())).y()) + 0.5;
            p.drawLine(QPointF(visible.left(), y), QPointF(visible.right(), y));
        }
    }
    p.restore();
}

void Canvas::paintPage(QPainter& p, const Page& page) const
{
    for (int l = 0; l < page.childCount(); ++l) {
        auto* layer = node_cast<Layer>(page.child(l));
        if (!layer || !layer->visible() || (m_output && !layer->printable()))
            continue;
        for (int i = 0; i < layer->childCount(); ++i) {
            auto* o = node_cast<DrawObject>(layer->child(i));
            if (o && !m_hidden.contains(o->id()) && (m_renderFilter.isEmpty() || m_renderFilter.contains(o->id())))
                paintObject(p, *o);
        }
    }
}

Canvas::ViewMode Canvas::viewMode() const
{
    // Drucken und Exportieren immer in voller Qualität
    if (m_output)
        return ViewMode::Enhanced;
    return ViewMode(std::clamp(Options::instance().integer(QStringLiteral("doc/general/viewMode")), 0, 5));
}

void Canvas::paintWireframe(QPainter& p, const DrawObject& o, bool simple) const
{
    if (const auto* group = node_cast<GroupObject>(&o)) {
        for (int i = 0; i < group->childCount(); ++i)
            if (const auto* c = node_cast<DrawObject>(group->child(i)))
                paintWireframe(p, *c, simple);
        return;
    }
    if (const auto* clip = node_cast<SuperClipObject>(&o)) {
        for (int i = 0; i < clip->childCount(); ++i)
            if (const auto* c = node_cast<DrawObject>(clip->child(i)))
                paintWireframe(p, *c, simple);
        return;
    }
    if (const auto* clone = node_cast<CloneObject>(&o)) {
        if (const auto shown = clone->rendered())
            paintWireframe(p, *shown, simple);
        return;
    }
    // Umriss zeigt Bitmaps in Graustufen, einfacher Umriss nur ihren Rahmen
    if (const auto* image = node_cast<ImageObject>(&o); image && !simple) {
        const QImage gray = image->displayImage().convertToFormat(QImage::Format_Grayscale8);
        const QTransform toScreen = o.transform() * viewTransform();
        p.save();
        p.setClipPath(toScreen.map(o.localPath()), Qt::IntersectClip);
        p.setTransform(QTransform::fromScale(image->size().width() / std::max(1, gray.width()),
                                             image->size().height() / std::max(1, gray.height())) * toScreen, true);
        p.drawImage(QPointF(0, 0), gray);
        p.restore();
    }
    QPainterPath path = o.path();
    if (const auto* text = node_cast<TextObject>(&o)) {
        path = QPainterPath();
        for (const TextObject::Piece& piece : text->shownPieces())
            path.addPath(o.transform().map(piece.path));
    }
    p.save();
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Qt::black, 0)); // Haarlinie
    p.drawPath(viewTransform().map(path));
    p.restore();
}

void Canvas::setEffectPreview(NodeId id, Effect effect, const Ramp& ramp)
{
    EffectPreview& preview = m_effectPreview[id];
    (effect == Effect::Blur ? preview.blur : preview.transparency) = ramp;
    update();
}

void Canvas::clearEffectPreview()
{
    if (m_effectPreview.isEmpty())
        return;
    m_effectPreview.clear();
    update();
}

void Canvas::paintObject(QPainter& p, const DrawObject& o) const
{
    if (const auto* clone = node_cast<CloneObject>(&o)) {
        if (const auto shown = clone->rendered())
            paintObject(p, *shown);
        return;
    }
    if (const ViewMode mode = viewMode(); mode == ViewMode::SimpleWireframe || mode == ViewMode::Wireframe) {
        paintWireframe(p, o, mode == ViewMode::SimpleWireframe);
        return;
    }
    if (m_wireLens) {
        // Unter einer Drahtmodell-Linse: Flächen und Umrisse in den Farben der Linse
        if (node_cast<GroupObject>(&o) || node_cast<SuperClipObject>(&o) || node_cast<BlendObject>(&o)) {
            paintObjectSharp(p, o);
            return;
        }
        const QPainterPath screen = viewTransform().map(o.path());
        if (!o.style().fill.isNone() || node_cast<TextObject>(&o))
            p.fillPath(screen, m_wireLens->second);
        p.strokePath(screen, QPen(m_wireLens->first, 1.0));
        return;
    }
    if (o.shadow().active && viewMode() != ViewMode::Draft)
        paintShadow(p, o, o.shadow());
    // Extrusion und Kontur: hinten liegende Teile vor dem Objekt zeichnen
    if (o.extrusion().active) {
        p.save();
        p.setRenderHint(QPainter::Antialiasing);
        for (const auto& [poly, color] : extrusionFaces(o, o.extrusion())) {
            p.setPen(QPen(color.darker(130), 0));
            p.setBrush(color);
            p.drawPolygon(viewTransform().map(poly));
        }
        p.restore();
    }
    QList<ContourStep> contour;
    if (o.contour().active) {
        contour = contourSteps(o, o.contour());
        for (const ContourStep& s : std::as_const(contour)) {
            if (s.front)
                continue;
            p.fillPath(viewTransform().map(s.path), s.fill.toQColor());
            if (s.stroke)
                p.strokePath(viewTransform().map(s.path), QPen(s.stroke->toQColor(), std::max(1.0, o.style().strokeWidth * m_zoom)));
        }
    }
    struct FrontContour {
        const Canvas* canvas;
        QPainter& p;
        const QList<ContourStep>& steps;
        double width;
        ~FrontContour()
        {
            for (const ContourStep& s : steps) {
                if (!s.front)
                    continue;
                p.fillPath(canvas->viewTransform().map(s.path), s.fill.toQColor());
                if (s.stroke)
                    p.strokePath(canvas->viewTransform().map(s.path), QPen(s.stroke->toQColor(), width));
            }
        }
    } frontContour{this, p, contour, std::max(1.0, o.style().strokeWidth * m_zoom)};
    if (o.lens().isActive()) {
        paintLens(p, o);
        return;
    }
    Ramp blur = o.blurRamp();
    Ramp alpha = o.transparency();
    if (const auto it = m_effectPreview.constFind(o.id()); it != m_effectPreview.constEnd()) {
        blur = it->blur.value_or(blur);
        alpha = it->transparency.value_or(alpha);
    }
    // Entwurfsansicht: Effekte nicht berechnen
    // Entwurf und Normal: ohne Unschärfe/Transparenz-Berechnung wie die einfachen klassischen Ansichten
    const bool draft = !m_rendering && (viewMode() == ViewMode::Draft || viewMode() == ViewMode::Normal);
    const bool blurred = blur.isActive() && !draft;
    const bool transparent = alpha.isActive() && !draft;
    if (!blurred && !transparent) {
        paintObjectSharp(p, o);
        return;
    }
    if (!blurred && alpha.type == Ramp::Type::Uniform) {
        // Gleichmässige Transparenz: einfach mit geringerer Deckkraft malen
        p.save();
        p.setOpacity(p.opacity() * std::clamp(1.0 - alpha.from / 100.0, 0.0, 1.0));
        paintObjectSharp(p, o);
        p.restore();
        return;
    }
    // Objekt in ein Zwischenbild malen, weichzeichnen bzw. maskieren, einsetzen. Nur der
    // sichtbare Teil samt dem Rand, aus dem der Weichzeichner noch Farbe holt, wird berechnet.
    const double sigma = blur.maximum() * m_zoom;
    const double reach = 3.0 * sigma;
    const double pad = reach + std::max(1.0, o.style().strokeWidth * m_zoom);
    QRectF area = viewTransform().mapRect(o.boundingRect()).adjusted(-pad, -pad, pad, pad);
    area = area.intersected(paintArea().adjusted(-reach, -reach, reach, reach));
    if (area.isEmpty())
        return;
    const QRect target = area.toAlignedRect();
    const qreal deviceDpr = p.device() ? p.device()->devicePixelRatioF() : 1.0;
    // Während ein Effekt-Griff gezogen wird: Vorschau mit einem Viertel der Auflösung
    const bool preview = m_effectPreview.contains(o.id()) && !m_rendering;
    if (!preview && !m_rendering) {
        const auto it = m_effectCache.constFind(o.id());
        if (it != m_effectCache.constEnd() && it->target == target && it->view == viewTransform() && it->dpr == deviceDpr) {
            p.drawImage(target.topLeft(), it->image);
            return;
        }
    }
    const qreal dpr = preview ? deviceDpr * 0.25 : deviceDpr;
    QImage image((QSizeF(target.size()) * dpr).toSize().expandedTo(QSize(1, 1)), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    {
        QPainter ip(&image);
        ip.setRenderHints(p.renderHints());
        ip.translate(-target.topLeft());
        paintObjectSharp(ip, o);
    }
    // Pixelmitte -> lokale Koordinaten des Objekts, für die Verläufe
    const QTransform toLocal = (o.transform() * viewTransform()).inverted();
    const QRectF bounds = o.localPath().boundingRect();
    const QPointF origin = target.topLeft();
    auto local = [&](double x, double y) { return toLocal.map(origin + QPointF(x, y) / dpr); };
    if (blur.isGradient()) {
        const double scale = m_zoom * dpr;
        rampBlur(image, [&](double x, double y) { return blur.valueAt(local(x, y), bounds) * scale; },
                 blur.minimum() * scale, sigma * dpr);
    } else if (blurred) {
        gaussianBlur(image, sigma * dpr);
    }
    if (transparent)
        applyOpacity(image, [&](double x, double y) { return 1.0 - alpha.valueAt(local(x, y), bounds) / 100.0; });
    if (preview) {
        p.save();
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawImage(QRectF(target), image);
        p.restore();
        return;
    }
    p.drawImage(target.topLeft(), image);
    if (!m_rendering)
        m_effectCache.insert(o.id(), EffectCache{image, target, viewTransform(), deviceDpr});
}

namespace {

// Fläche (Kontur innen/aussen): Grundformen und Text immer, Kurven nur mit geschlossenem Teilpfad
bool isArea(const DrawObject& o)
{
    if (const auto* c = node_cast<CurveObject>(&o))
        return std::any_of(c->subPaths().begin(), c->subPaths().end(), [](const SubPath& s) { return s.closed; });
    return true;
}

} // namespace

void Canvas::strokeStyled(QPainter& p, const QPainterPath& screenPath, const Style& s, double zoom, bool closed,
                          const QPainterPath& arrowsPage, const QTransform& view)
{
    if (!s.stroke)
        return;
    const QColor color = s.stroke->toQColor();
    const bool sided = closed && s.position != Style::Position::Center;
    // Innen/aussen: doppelt so breit, die andere Hälfte wird weggeschnitten
    const double width = std::max(1.0, s.strokeWidth * zoom * (sided ? 2.0 : 1.0));
    QPen pen(color, width);
    pen.setJoinStyle(s.join == Style::Join::Round ? Qt::RoundJoin : s.join == Style::Join::Bevel ? Qt::BevelJoin : Qt::MiterJoin);
    pen.setCapStyle(s.cap == Style::Cap::Round ? Qt::RoundCap : s.cap == Style::Cap::Square ? Qt::SquareCap : Qt::FlatCap);
    const QList<double> dashes = outline::dashPattern(s.dash);
    if (!dashes.isEmpty())
        pen.setDashPattern(dashes);
    p.save();
    if (sided) {
        if (s.position == Style::Position::Inside) {
            p.setClipPath(screenPath, Qt::IntersectClip);
        } else {
            // Aussen: alles ausser der Fläche (gerade/ungerade über ein grosses Rechteck)
            QPainterPath outside;
            outside.addRect(screenPath.boundingRect().adjusted(-width * 4, -width * 4, width * 4, width * 4));
            outside.addPath(screenPath);
            outside.setFillRule(Qt::OddEvenFill);
            p.setClipPath(outside, Qt::IntersectClip);
        }
    }
    QPainterPath line = screenPath;
    if (!arrowsPage.isEmpty() && (s.startArrow > 0 || s.endArrow > 0)) {
        const outline::Stroke st = outline::withArrows(arrowsPage, s);
        line = view.map(st.line);
        p.fillPath(view.map(st.heads), color);
    }
    if (s.nibStretch < 99.999) {
        // Kalligrafische Spitze: rund in einem gestauchten, gedrehten Raum strichen
        QTransform nib;
        nib.rotate(-s.nibAngle);
        nib.scale(1.0, std::max(0.01, s.nibStretch / 100.0));
        QPainterPathStroker st(pen);
        st.setWidth(width);
        st.setCapStyle(pen.capStyle());
        st.setJoinStyle(Qt::RoundJoin);
        if (!dashes.isEmpty())
            st.setDashPattern(dashes);
        const QTransform toNib = nib.inverted();
        p.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath shape = nib.map(st.createStroke(toNib.map(line)));
        shape.setFillRule(Qt::WindingFill);
        p.fillPath(shape, color);
    } else {
        p.strokePath(line, pen);
    }
    p.restore();
}

void Canvas::strokeOnly(QPainter& p, const DrawObject& o) const
{
    strokeStyled(p, viewTransform().map(o.path()), o.style(), m_zoom, isArea(o));
}

void Canvas::paintBelow(QPainter& p, const DrawObject& stop) const
{
    // Oberstes Objekt im Ebenenbaum, das stop enthält
    const Node* top = &stop;
    while (top->parent() && !node_cast<Layer>(top->parent()))
        top = top->parent();
    const Node* pageNode = top->parent() ? top->parent()->parent() : nullptr;
    auto paintUntil = [&](const Page& page) {
        for (int l = 0; l < page.childCount(); ++l) {
            auto* layer = node_cast<Layer>(page.child(l));
            if (!layer || !layer->visible() || (m_output && !layer->printable()))
                continue;
            for (int i = 0; i < layer->childCount(); ++i) {
                if (layer->child(i) == top)
                    return true;
                if (auto* d = node_cast<DrawObject>(layer->child(i)); d && !m_hidden.contains(d->id()))
                    paintObject(p, *d);
            }
        }
        return false;
    };
    if (pageNode != &m_editor.document().masterPage() && paintUntil(m_editor.document().masterPage()))
        return;
    if (const auto* page = node_cast<Page>(pageNode))
        paintUntil(*page);
}

void Canvas::paintLens(QPainter& p, const DrawObject& o) const
{
    const QPainterPath screenPath = viewTransform().map(o.path());
    const Lens& lens = o.lens();
    const QRectF lensRect = screenPath.boundingRect();
    // Fischauge holt Bildpunkte aus der ganzen Linse: dann die ganze Linse rechnen (begrenzt)
    QRectF area = lens.type == Lens::Type::FishEye ? lensRect.intersected(paintArea().adjusted(-2000, -2000, 2000, 2000))
                                                    : lensRect.intersected(paintArea());
    if (!area.isEmpty()) {
        const qreal dpr = p.device() ? p.device()->devicePixelRatioF() : 1.0;
        const QRect target = area.toAlignedRect();
        QImage below(target.size() * dpr, QImage::Format_ARGB32_Premultiplied);
        below.setDevicePixelRatio(dpr);
        // Mitte der Linse und Blickpunkt (Bildschirm)
        const QPointF center = lensRect.center();
        const QPointF view = lens.viewpoint ? center + lens.viewOffset * m_zoom : center;
        {
            QPainter q(&below);
            q.setRenderHint(QPainter::Antialiasing);
            q.translate(-target.topLeft());
            // Seitenhintergrund wie beim normalen Zeichnen
            const QRectF pageRect = viewTransform().mapRect(QRectF(QPointF(0, 0), m_editor.currentPage().size()));
            q.fillRect(QRectF(target), QColor(0, 0, 0, 0));
            paintBackground(q, pageRect);
            // Blickpunkt in die Mitte holen, beim Vergrössern um die Mitte skalieren
            const double f = lens.type == Lens::Type::Magnify ? std::max(1.0, lens.amount / 10.0) : 1.0;
            q.translate(center);
            q.scale(f, f);
            q.translate(-view);
            if (lens.type == Lens::Type::Wireframe)
                m_wireLens = std::pair{lens.color.toQColor(), lens.color2.toQColor()};
            paintBelow(q, o);
            m_wireLens.reset();
        }
        if (lens.type == Lens::Type::FishEye) {
            // Jeder Bildpunkt holt seine Farbe von der Stelle, die das Fischauge dorthin wölbt
            const QImage src = below;
            const QPointF origin = target.topLeft();
            const QSizeF radius(lensRect.width() / 2.0, lensRect.height() / 2.0);
            for (int y = 0; y < below.height(); ++y) {
                auto* line = reinterpret_cast<QRgb*>(below.scanLine(y));
                for (int x = 0; x < below.width(); ++x) {
                    const QPointF d = origin + QPointF(x + 0.5, y + 0.5) / dpr;
                    const QPointF s = (lens.fishEye(d, center, radius, false) - origin) * dpr;
                    const int sx = std::clamp(int(s.x()), 0, src.width() - 1), sy = std::clamp(int(s.y()), 0, src.height() - 1);
                    line[x] = reinterpret_cast<const QRgb*>(src.constScanLine(sy))[sx];
                }
            }
        } else if (lens.changesColor()) {
            for (int y = 0; y < below.height(); ++y) {
                auto* line = reinterpret_cast<QRgb*>(below.scanLine(y));
                for (int x = 0; x < below.width(); ++x) {
                    const QRgb px = line[x];
                    const int a = qAlpha(px);
                    int r = qRed(px), g = qGreen(px), b = qBlue(px);
                    lens.filter(r, g, b, a);
                    line[x] = qRgba(r, g, b, a);
                }
            }
        }
        p.save();
        p.setClipPath(screenPath, Qt::IntersectClip);
        p.drawImage(QPointF(target.topLeft()), below);
        p.restore();
    }
    strokeOnly(p, o);
}

QTransform Canvas::shadowTransform(const QRectF& b, const DropShadow& s)
{
    if (!s.perspective)
        return QTransform::fromTranslate(s.offset.x(), s.offset.y());
    // Perspektivisch: an der Unter- (bzw. Ober-)kante gespiegelt, gestreckt und geneigt
    const double baseY = s.fromTop ? b.top() : b.bottom();
    const double k = std::clamp(s.stretch, 1.0, 400.0) / 100.0;
    const double shear = std::tan(qDegreesToRadians(std::clamp(s.angle, -85.0, 85.0)));
    // y relativ zur Kante: Höhe h -> Länge k*h nach hinten (nach oben bei Unterkante)
    QTransform t = QTransform::fromTranslate(-b.center().x(), -baseY);
    t *= QTransform(1, 0, shear * k * (s.fromTop ? -1 : 1), k, 0, 0);
    t *= QTransform::fromTranslate(b.center().x(), baseY);
    return t;
}

QList<Canvas::ContourStep> Canvas::contourSteps(const DrawObject& o, const ContourEffect& c)
{
    QList<ContourStep> steps;
    QPainterPath base = o.path();
    base.setFillRule(Qt::WindingFill);
    const Color startFill = o.style().fill.representativeColor().value_or(Color(255, 255, 255));
    auto mix = [](const Color& a, const Color& b, double t) {
        auto ch = [t](int x, int y) { return std::uint8_t(std::lround(x + (y - x) * t)); };
        return Color(ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), ch(a.a, b.a));
    };
    const int n = std::clamp(c.steps, 1, 200);
    auto grown = [&base](double d) {
        QPainterPathStroker st;
        st.setWidth(2.0 * d);
        st.setJoinStyle(Qt::RoundJoin);
        return base.united(st.createStroke(base)).simplified();
    };
    auto shrunk = [&base](double d) {
        QPainterPathStroker st;
        st.setWidth(2.0 * d);
        st.setJoinStyle(Qt::RoundJoin);
        return base.subtracted(st.createStroke(base)).simplified();
    };
    if (c.direction == ContourEffect::Direction::Outside) {
        // Aussen: grösste Stufe hinten, in der Endfarbe
        for (int k = n; k >= 1; --k) {
            const double t = double(k) / n;
            steps << ContourStep{grown(c.offset * k), mix(startFill, c.endFill, t),
                                 o.style().stroke ? std::optional<Color>(mix(*o.style().stroke, c.endStroke, t)) : std::nullopt, false};
        }
    } else {
        // Innen bzw. zur Mitte: bis die Form verschwindet (Mitte) oder n Stufen
        const QRectF b = base.boundingRect();
        const int count = c.direction == ContourEffect::Direction::Center
                              ? std::max(1, int(std::min(b.width(), b.height()) / 2.0 / std::max(0.01, c.offset)))
                              : n;
        for (int k = 1; k <= count; ++k) {
            const QPainterPath p = shrunk(c.offset * k);
            if (p.isEmpty())
                break;
            const double t = double(k) / count;
            steps << ContourStep{p, mix(startFill, c.endFill, t),
                                 o.style().stroke ? std::optional<Color>(mix(*o.style().stroke, c.endStroke, t)) : std::nullopt, true};
        }
    }
    return steps;
}

QList<std::pair<QPolygonF, QColor>> Canvas::extrusionFaces(const DrawObject& o, const Extrusion& e)
{
    QList<std::pair<QPolygonF, QColor>> faces;
    const QPointF center = o.boundingRect().center();
    const QPointF vp = center + e.vanish;
    const double f = std::clamp(e.depth, 0.0, 99.0) / 100.0;
    auto back = [&](const QPointF& p) { return e.parallel ? p + e.vanish * f : p + (vp - p) * f; };
    // Licht von oben links: Seitenflächen nach Richtung ihrer Kante heller/dunkler
    const QColor base = e.color.toQColor();
    for (const QPolygonF& poly : o.path().toSubpathPolygons()) {
        QPolygonF backFace;
        for (const QPointF& p : poly)
            backFace << back(p);
        faces << std::pair{backFace, base.darker(150)};
        QList<std::pair<double, std::pair<QPolygonF, QColor>>> sides;
        for (int i = 0; i + 1 < poly.size(); ++i) {
            const QPointF a = poly[i], b = poly[i + 1];
            if (QLineF(a, b).length() < 1e-9)
                continue;
            const QPolygonF quad({a, b, back(b), back(a)});
            QColor c = base;
            if (e.shade) {
                const QPointF edge = b - a;
                const double nx = edge.y(), ny = -edge.x(); // Aussennormale (Uhrzeigersinn egal: Betrag)
                const double len = std::hypot(nx, ny);
                const double light = (-nx * 0.7071 - ny * 0.7071) / std::max(1e-9, len);
                c = light > 0 ? base.lighter(100 + int(light * 40)) : base.darker(100 + int(-light * 50));
            }
            // Weiter vom Fluchtpunkt entfernte Flächen liegen vorn
            const QPointF mid = (a + b) / 2.0;
            sides << std::pair{-QLineF(mid, vp).length(), std::pair{quad, c}};
        }
        std::sort(sides.begin(), sides.end(), [](const auto& x, const auto& y) { return x.first < y.first; });
        for (const auto& s : sides)
            faces << s.second;
    }
    return faces;
}

void Canvas::paintShadow(QPainter& p, const DrawObject& o, const DropShadow& s) const
{
    // Silhouette (Fläche samt Kontur) in Seitenkoordinaten
    QPainterPath shape = o.path();
    if (const auto* text = node_cast<TextObject>(&o)) {
        shape = QPainterPath();
        for (const TextObject::Piece& piece : text->shownPieces())
            shape.addPath(o.transform().map(piece.path));
    }
    if (o.style().stroke && o.style().strokeWidth > 0) {
        QPainterPathStroker st;
        st.setWidth(o.style().strokeWidth);
        st.setJoinStyle(Qt::MiterJoin);
        shape = shape.united(st.createStroke(shape));
    }
    shape.setFillRule(Qt::WindingFill);
    const QTransform toShadow = shadowTransform(o.boundingRect(), s);
    const QPainterPath screen = (toShadow * viewTransform()).map(shape);
    const double sigma = std::max(0.0, s.feather * m_zoom / 2.0);
    const double pad = 3.0 * sigma + 2.0;
    QRectF area = screen.boundingRect().adjusted(-pad, -pad, pad, pad).intersected(paintArea().adjusted(-pad, -pad, pad, pad));
    if (area.isEmpty())
        return;
    const QRect target = area.toAlignedRect();
    QImage mask(target.size(), QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);
    {
        QPainter q(&mask);
        q.setRenderHint(QPainter::Antialiasing);
        q.translate(-target.topLeft());
        QColor c = s.color.toQColor();
        q.fillPath(screen, c);
        if (s.perspective && s.fade > 0) {
            // Ausblenden: zum fernen Ende hin durchsichtig (Ziel-Ausschnitt)
            const QRectF b = o.boundingRect();
            const QPointF nearPt = (toShadow * viewTransform()).map(QPointF(b.center().x(), s.fromTop ? b.top() : b.bottom()));
            const QPointF farPt = (toShadow * viewTransform()).map(QPointF(b.center().x(), s.fromTop ? b.bottom() : b.top()));
            QLinearGradient g(nearPt, farPt);
            g.setColorAt(0, QColor(0, 0, 0, 255));
            g.setColorAt(1, QColor(0, 0, 0, int(255 * (1.0 - std::clamp(s.fade, 0.0, 100.0) / 100.0))));
            q.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            q.fillRect(QRectF(target), g);
        }
    }
    if (sigma > 0.3)
        gaussianBlur(mask, sigma);
    p.save();
    p.setOpacity(p.opacity() * std::clamp(s.opacity / 100.0, 0.0, 1.0));
    p.drawImage(target.topLeft(), mask);
    p.restore();
}

void Canvas::paintBevel(QPainter& p, const DrawObject& o) const
{
    const Bevel& bv = o.bevel();
    const QPainterPath screenPath = viewTransform().map(o.path());
    const double widthPx = std::max(1.0, bv.width * m_zoom);
    // Das Höhenfeld braucht die Kanten: sichtbarer Teil plus so viel Rand, wie der Weichzeichner reicht
    const double reach = 2.0 * widthPx + 2.0;
    QRectF area = screenPath.boundingRect().adjusted(-2, -2, 2, 2).intersected(paintArea().adjusted(-reach, -reach, reach, reach));
    if (area.isEmpty() || !area.intersects(paintArea()))
        return;
    const QRect target = area.toAlignedRect();
    // Höhenfeld: weichgezeichnete Form, am Rand 0, innen ab der Breite 1
    QImage mask(target.size(), QImage::Format_Grayscale8);
    mask.fill(0);
    {
        QPainter q(&mask);
        q.setRenderHint(QPainter::Antialiasing);
        q.translate(-target.topLeft());
        q.fillPath(screenPath, Qt::white);
    }
    QImage height = mask.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    gaussianBlur(height, widthPx / 2.0);
    const double az = qDegreesToRadians(bv.direction), el = qDegreesToRadians(std::clamp(bv.altitude, 1.0, 89.0));
    // Licht in Bildschirmkoordinaten (y nach unten)
    const double lx = std::cos(az) * std::cos(el), ly = -std::sin(az) * std::cos(el), lz = std::sin(el);
    const double strength = std::clamp(bv.intensity / 100.0, 0.0, 1.0);
    const double relief = bv.style == Bevel::Style::Emboss ? 2.0 : 1.0;
    QImage shade(target.size(), QImage::Format_ARGB32_Premultiplied);
    shade.fill(Qt::transparent);
    const QColor light = bv.light.toQColor(), dark = bv.shadow.toQColor();
    auto h = [&](int x, int y) {
        x = std::clamp(x, 0, height.width() - 1);
        y = std::clamp(y, 0, height.height() - 1);
        return qAlpha(height.pixel(x, y)) / 255.0;
    };
    for (int y = 0; y < shade.height(); ++y) {
        const uchar* m = mask.constScanLine(y);
        auto* out = reinterpret_cast<QRgb*>(shade.scanLine(y));
        for (int x = 0; x < shade.width(); ++x) {
            if (m[x] == 0)
                continue;
            // Normale aus dem Gefälle des Höhenfelds
            const double dx = (h(x + 1, y) - h(x - 1, y)) * widthPx * relief;
            const double dy = (h(x, y + 1) - h(x, y - 1)) * widthPx * relief;
            const double nl = std::sqrt(dx * dx + dy * dy + 1.0);
            double dot = (-dx * lx - dy * ly + lz) / nl - lz; // flach = 0
            // Licht- und Schattenseite gleich stark (die Lichtseite kann höchstens 1 - lz gewinnen)
            dot = dot > 0 ? dot / std::max(1e-6, 1.0 - lz) : dot / std::max(1e-6, lz + 1.0);
            const double v = std::clamp(dot * strength, -1.0, 1.0) * (m[x] / 255.0);
            const QColor& c = v > 0 ? light : dark;
            const int alpha = int(std::abs(v) * 255);
            out[x] = qRgba(c.red() * alpha / 255, c.green() * alpha / 255, c.blue() * alpha / 255, alpha);
        }
    }
    p.drawImage(target.topLeft(), shade);
}

void Canvas::paintObjectSharp(QPainter& p, const DrawObject& o) const
{
    if (auto* blend = node_cast<BlendObject>(&o)) {
        // Überblendung: Start, Zwischenstufen, Ende
        if (const DrawObject* a = blend->start())
            paintObject(p, *a);
        for (const auto& step : blend->intermediates())
            paintObject(p, *step);
        if (const DrawObject* b = blend->end())
            paintObject(p, *b);
        return;
    }
    if (auto* clip = node_cast<SuperClipObject>(&o)) {
        // Rahmen, Inhalte nur in seiner Form, Kontur des Rahmens obenauf
        const DrawObject* frame = clip->frame();
        if (!frame)
            return;
        paintObject(p, *frame);
        p.save();
        p.setClipPath(viewTransform().map(frame->path()), Qt::IntersectClip);
        for (const DrawObject* c : clip->contents())
            paintObject(p, *c);
        p.restore();
        strokeOnly(p, *frame);
        return;
    }
    if (auto* group = node_cast<GroupObject>(&o)) {
        for (int i = 0; i < group->childCount(); ++i) {
            if (auto* child = node_cast<DrawObject>(group->child(i)))
                paintObject(p, *child);
        }
        return;
    }
    if (const auto* sprayed = node_cast<CurveObject>(&o); sprayed && sprayed->media().kind == ArtisticMedia::Kind::Sprayer) {
        // Sprühdose: jedes Objekt in seiner Farbe
        const QTransform toScreen = o.transform() * viewTransform();
        for (const SubPath& sp : sprayed->subPaths())
            for (const pathops::SprayPiece& piece : pathops::sprayPieces(CurveObject(QList<SubPath>{sp}).spinePath(), sprayed->media()))
                p.fillPath(toScreen.map(piece.path), piece.color.toQColor());
        return;
    }
    const QPainterPath screenPath = viewTransform().map(o.path());
    const Style& s = o.style();
    if (s.stroke && s.behindFill && !node_cast<ImageObject>(&o) && !node_cast<TableObject>(&o) && !node_cast<DimensionObject>(&o))
        strokeStyled(p, screenPath, s, m_zoom, isArea(o), o.path(), viewTransform());
    if (auto* image = node_cast<ImageObject>(&o)) {
        const QImage pixels = image->displayImage();
        const QTransform toScreen = o.transform() * viewTransform();
        p.save();
        p.setClipPath(toScreen.map(o.localPath()), Qt::IntersectClip);
        if (pixels.isNull()) {
            // Fehlendes Bild: graue Fläche wie ein Platzhalter
            p.fillPath(toScreen.map(o.localPath()), QColor(0xd0, 0xd0, 0xd0));
        } else {
            p.setRenderHint(QPainter::SmoothPixmapTransform);
            // combine: Im Zwischenbild der Unschärfe ist der Maler schon verschoben
            p.setTransform(QTransform::fromScale(image->size().width() / pixels.width(),
                                                 image->size().height() / pixels.height())
                               * toScreen,
                           true);
            p.drawImage(QPointF(0, 0), pixels);
        }
        p.restore();
    } else if (auto* table = node_cast<TableObject>(&o)) {
        // Tabelle: Zellhintergründe (eigene Farbe oder Objektfüllung), Text, dann das Gitter
        const QTransform toScreen = o.transform() * viewTransform();
        for (int r = 0; r < table->rows(); ++r) {
            for (int c = 0; c < table->columns(); ++c) {
                QPainterPath cell;
                cell.addRect(table->cellRect(r, c));
                if (const std::optional<Color> fill = table->cell(r, c).fill) {
                    p.fillPath(toScreen.map(cell), fill->toQColor());
                } else if (!s.fill.isNone()) {
                    QBrush brush = s.fill.brush(o.localPath().boundingRect());
                    brush.setTransform(brush.transform() * toScreen);
                    p.fillPath(toScreen.map(cell), brush);
                }
            }
        }
        const QColor ink = table->textFormat().color ? table->textFormat().color->toQColor() : QColor(Qt::black);
        for (const TextObject::Piece& piece : table->textPieces())
            p.fillPath(toScreen.map(piece.path), piece.color ? piece.color->toQColor() : ink);
        if (s.stroke) {
            QPen pen(s.stroke->toQColor(), std::max(1.0, s.strokeWidth * m_zoom));
            pen.setJoinStyle(Qt::MiterJoin);
            p.strokePath(toScreen.map(table->gridPath()), pen);
        }
        return;
    } else if (auto* dim = node_cast<DimensionObject>(&o)) {
        // Bemassung: Linien mit der Kontur, Pfeile und Masszahl in der Konturfarbe gefüllt
        const QColor ink = s.stroke ? s.stroke->toQColor() : QColor(Qt::black);
        QPen pen(ink, std::max(1.0, s.strokeWidth * m_zoom));
        p.save();
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(viewTransform().map(dim->linePath()));
        p.fillPath(viewTransform().map(dim->fillPath()), ink);
        p.restore();
        return;
    } else if (auto* text = node_cast<TextObject>(&o)) {
        // Text: Bereiche mit eigener Farbe zuerst, der Rest in der Objektfüllung
        const QTransform toScreen = o.transform() * viewTransform();
        if (text->kind() == TextObject::Kind::Paragraph && !text->hasEnvelope()
            && Options::instance().flag(QStringLiteral("ws/text/frames/showFrames"))) {
            // Mengentextrahmen gestrichelt (nur am Bildschirm); rot, wenn hier
            // Text abgeschnitten wird
            const QList<NodeId> chain = text->chain();
            const bool cut = text->overflows() && (chain.isEmpty() || chain.last() == text->id());
            p.save();
            p.setPen(QPen(cut ? QColor(0xe0, 0x30, 0x30) : QColor(0x9a, 0x9a, 0x9a), 1.0, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawPolygon(toScreen.map(QPolygonF(text->frameRect())));
            p.restore();
        }
        const bool asGlyphs = m_output && m_textAsGlyphs && text->exportableAsText() && !m_complexFillsAsBitmaps;
        for (const TextObject::Piece& piece : text->shownPieces()) {
            // Echte Schrift: einfarbige Bereiche als Glyphenläufe (die PDF-Ausgabe bettet die Schrift ein)
            const std::optional<QColor> solid = piece.color ? std::optional<QColor>(piece.color->toQColor())
                                                : s.fill.type == Fill::Type::Solid ? std::optional<QColor>(s.fill.color.toQColor())
                                                                                   : std::nullopt;
            if (asGlyphs && solid && !piece.runs.isEmpty()) {
                if (!piece.extras.isEmpty())
                    p.fillPath(toScreen.map(piece.extras), *solid);
                p.save();
                p.setWorldTransform(QTransform::fromScale(1.0 / TextObject::kUnitsPerMm, 1.0 / TextObject::kUnitsPerMm) * toScreen * p.worldTransform());
                p.setPen(*solid);
                for (const QGlyphRun& run : piece.runs)
                    p.drawGlyphRun(QPointF(0, 0), run);
                p.restore();
                continue;
            }
            if (piece.color) {
                p.fillPath(toScreen.map(piece.path), piece.color->toQColor());
            } else if (!s.fill.isNone()) {
                QBrush brush = s.fill.brush(o.localPath().boundingRect());
                brush.setTransform(brush.transform() * toScreen);
                p.fillPath(toScreen.map(piece.path), brush);
            }
        }
    } else if (!s.fill.isNone()) {
        // Verläufe liegen in lokalen Koordinaten und folgen der Objekttransformation
        QBrush brush = s.fill.brush(o.localPath().boundingRect());
        brush.setTransform(brush.transform() * o.transform() * viewTransform());
        // Entwurf: Verläufe und Maschen als gleichmässige Farbe
        if (viewMode() == ViewMode::Draft && (s.fill.isGradient() || s.fill.isMesh()))
            brush = QBrush(s.fill.representativeColor().value_or(Color(128, 128, 128)).toQColor());
        // Offene Kurven werden nur gefüllt, wenn das Dokument es will (Optionen > Dokument > Allgemein)
        const auto* curve = node_cast<CurveObject>(&o);
        if (curve && !curve->media().isActive() && !Options::instance().flag(QStringLiteral("doc/general/fillOpenCurves"))) {
            QList<SubPath> closed;
            for (const SubPath& sp : curve->subPaths())
                if (sp.closed)
                    closed << sp;
            if (!closed.isEmpty())
                p.fillPath((o.transform() * viewTransform()).map(CurveObject::toPainterPath(closed)), brush);
        } else if (m_output && m_complexFillsAsBitmaps && brush.style() != Qt::SolidPattern && brush.style() != Qt::NoBrush) {
            // Verläufe, Muster und Maschen als Bild (300 dpi) in der Form – für ältere RIPs
            const QRectF device = p.worldTransform().mapRect(screenPath.boundingRect()).intersected(QRectF(QPointF(-1e5, -1e5), QSizeF(2e5, 2e5)));
            const double k = std::min(1.0, 300.0 / 25.4 / std::max(1e-6, m_zoom * std::sqrt(std::abs(p.worldTransform().determinant()))));
            QImage img(QSize(std::max(1, int(std::ceil(device.width() * k))), std::max(1, int(std::ceil(device.height() * k)))),
                       QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::transparent);
            {
                QPainter ip(&img);
                ip.setRenderHint(QPainter::Antialiasing);
                ip.scale(k, k);
                ip.translate(-device.topLeft());
                ip.setWorldTransform(p.worldTransform(), true);
                ip.fillPath(screenPath, brush);
            }
            p.save();
            p.setClipPath(screenPath, Qt::IntersectClip);
            p.setWorldTransform(QTransform());
            p.drawImage(device, img);
            p.restore();
        } else {
            if (s.fill.isMesh())
                p.setRenderHint(QPainter::SmoothPixmapTransform);
            p.fillPath(screenPath, brush);
        }
    }
    if (o.bevel().isActive())
        paintBevel(p, o);
    if (s.stroke && !s.behindFill)
        strokeStyled(p, screenPath, s, m_zoom, isArea(o), o.path(), viewTransform());
}

void Canvas::paintSnapHint(QPainter& p, const SnapResult& snap) const
{
    if (!snap.snapped())
        return;
    const QPointF s = toScreen(snap.point);
    p.save();
    if (!snap.lines.isEmpty()) {
        QPen pen(snap.lineColor.isValid() ? snap.lineColor : previewColor(), 1, Qt::DashLine);
        p.setPen(pen);
        for (const QLineF& l : snap.lines)
            p.drawLine(toScreen(l.p1()), toScreen(l.p2()));
    }
    p.setPen(QPen(previewColor(), 1));
    p.setBrush(Qt::NoBrush);
    // Optionen > An Objekten ausrichten: Positionsmarkierung und Bildschirmtipp
    if (Options::instance().flag(QStringLiteral("ws/snap/markers")))
        p.drawRect(QRectF(s.x() - 4, s.y() - 4, 8, 8));
    if (!Options::instance().flag(QStringLiteral("ws/snap/screentip"))) {
        p.restore();
        return;
    }
    QFont f = font();
    f.setPointSizeF(f.pointSizeF() * 0.9);
    p.setFont(f);
    p.setPen(QColor(0x10, 0x30, 0x90));
    p.drawText(s + QPointF(8, -6), snap.hint);
    p.restore();
}

ShapeTool& Canvas::shapeTool() const
{
    return static_cast<ShapeTool&>(*m_tools[static_cast<std::size_t>(ToolId::Shape)]);
}

TableTool& Canvas::tableTool() const
{
    return static_cast<TableTool&>(*m_tools[static_cast<std::size_t>(ToolId::Table)]);
}

MeshTool& Canvas::meshTool() const
{
    return static_cast<MeshTool&>(*m_tools[static_cast<std::size_t>(ToolId::Mesh)]);
}

PickTool& Canvas::pickTool() const
{
    return static_cast<PickTool&>(*m_tools[static_cast<std::size_t>(ToolId::Pick)]);
}

CropTool& Canvas::cropTool() const
{
    return static_cast<CropTool&>(*m_tools[static_cast<std::size_t>(ToolId::Crop)]);
}

EraserTool& Canvas::eraserTool() const
{
    return static_cast<EraserTool&>(*m_tools[static_cast<std::size_t>(ToolId::Eraser)]);
}

ArtisticMediaTool& Canvas::mediaTool() const
{
    return static_cast<ArtisticMediaTool&>(*m_tools[static_cast<std::size_t>(ToolId::ArtisticMedia)]);
}

TextTool& Canvas::textTool() const
{
    return static_cast<TextTool&>(*m_tools[static_cast<std::size_t>(ToolId::Text)]);
}

void Canvas::mouseDoubleClickEvent(QMouseEvent* e)
{
    tool()->mouseDoubleClick(e, toDoc(e->position()));
}

void Canvas::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::MiddleButton) {
        m_panning = true;
        m_panStart = e->position();
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    // Doppelseiten: Klick auf die Partnerseite macht sie zur aktuellen, ohne dass sich die Ansicht verschiebt
    if (e->button() == Qt::LeftButton) {
        const auto [partner, offset] = facingPartner();
        if (partner >= 0) {
            const QRectF partnerRect = viewTransform().mapRect(QRectF(QPointF(offset, 0), m_editor.currentPage().size()));
            if (partnerRect.contains(e->position())) {
                m_origin.rx() += offset * m_zoom;
                m_editor.setCurrentPageIndex(partner);
                update();
                emit viewChanged();
                return;
            }
        }
    }
    tool()->mousePress(e, toDoc(e->position()));
}

void Canvas::mouseMoveEvent(QMouseEvent* e)
{
    if (m_panning) {
        m_origin += e->position() - m_panStart;
        m_panStart = e->position();
        update();
        emit viewChanged();
        return;
    }
    emit cursorMoved(toDoc(e->position()));
    tool()->mouseMove(e, toDoc(e->position()));
}

void Canvas::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::MiddleButton && m_panning) {
        m_panning = false;
        tool()->activate(); // Werkzeug-Cursor wiederherstellen
        return;
    }
    tool()->mouseRelease(e, toDoc(e->position()));
}

void Canvas::leaveEvent(QEvent*)
{
    emit cursorMoved(QPointF(qQNaN(), qQNaN()));
}

void Canvas::wheelEvent(QWheelEvent* e)
{
    const double steps = e->angleDelta().y() / 120.0;
    if (e->modifiers() & Qt::ControlModifier) {
        m_origin.rx() += e->angleDelta().y() / 2.0; // Strg: waagrecht rollen
    } else if (e->modifiers() & Qt::ShiftModifier) {
        m_origin.ry() += e->angleDelta().y() / 2.0; // Umschalt: senkrecht rollen
    } else if (steps != 0.0 && Options::instance().integer(QStringLiteral("ws/display/wheel")) == 1) {
        m_origin.ry() += e->angleDelta().y() / 2.0; // Optionen: Mausrad = Bildlauf
    } else if (steps != 0.0) {
        zoomBy(std::pow(1.2, steps), e->position()); // Mausrad zoomt
        if (Options::instance().flag(QStringLiteral("ws/tools/zoom/wheelCenter"))) {
            // Stelle unter der Maus in die Mitte holen
            m_origin += QPointF(rect().center()) - e->position();
            update();
            emit viewChanged();
        }
        e->accept();
        return;
    }
    update();
    emit viewChanged();
    e->accept();
}

namespace {

// Lokale Bilddateien bzw. Bilddaten in einem Ziehvorgang
QStringList droppedImageFiles(const QMimeData* data)
{
    QStringList files;
    const QStringList formats = imagepool::importSuffixes();
    for (const QUrl& url : data->urls()) {
        const QString suffix = QFileInfo(url.toLocalFile()).suffix().toLower();
        if (url.isLocalFile() && formats.contains(suffix))
            files << url.toLocalFile();
    }
    return files;
}

} // namespace

void Canvas::dragEnterEvent(QDragEnterEvent* e)
{
    if (!droppedImageFiles(e->mimeData()).isEmpty() || e->mimeData()->hasImage())
        e->acceptProposedAction();
}

void Canvas::dragMoveEvent(QDragMoveEvent* e)
{
    e->acceptProposedAction();
}

void Canvas::dropEvent(QDropEvent* e)
{
    QPointF at = toDoc(e->position());
    const QPointF step(m_editor.duplicateOffset());
    bool any = false;
    for (const QString& path : droppedImageFiles(e->mimeData())) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly) && m_editor.importImage(file.readAll(), at) != InvalidNodeId) {
            at += step; // mehrere Bilder leicht versetzt wie beim Duplizieren
            any = true;
        }
    }
    if (!any && e->mimeData()->hasImage())
        any = m_editor.importImage(qvariant_cast<QImage>(e->mimeData()->imageData()), at) != InvalidNodeId;
    if (any) {
        e->acceptProposedAction();
        setTool(ToolId::Pick);
        activateWindow();
        setFocus();
    }
}

bool Canvas::event(QEvent* e)
{
    // Beim Tippen gehören auch Tasten mit Menükürzel (Entf, Strg+A, Strg+C …) dem Werkzeug
    if (e->type() == QEvent::ShortcutOverride && tool()->capturesKeyboard()) {
        e->accept();
        return true;
    }
    // Tab wechselt sonst den Fokus, statt im Text einen Tabulator zu setzen
    if (e->type() == QEvent::KeyPress && tool()->capturesKeyboard()) {
        auto* key = static_cast<QKeyEvent*>(e);
        if (key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab) {
            keyPressEvent(key);
            return true;
        }
    }
    return QWidget::event(e);
}

void Canvas::panBy(const QPointF& delta)
{
    m_origin += delta;
    update();
    emit viewChanged();
}

void Canvas::setHiddenObjects(const QList<NodeId>& ids)
{
    if (m_hidden == ids)
        return;
    m_hidden = ids;
    update();
}

namespace {

constexpr double kTextTab = 9.0; // Kantenlänge der Laschen, px

QPointF textTabPoint(const TextFrameGeometry& f, bool bottom, const QTransform& view)
{
    return (f.transform * view).map(QPointF(f.size.width() / 2.0, bottom ? f.size.height() : 0.0));
}

// Schnittpunkt der Strecke line mit dem Rand von frame, der am nächsten bei near liegt
std::optional<QPointF> frameEdgeHit(const QPolygonF& frame, const QLineF& line, const QPointF& near)
{
    std::optional<QPointF> best;
    for (int i = 0; i < frame.size(); ++i) {
        QPointF hit;
        const QLineF edge(frame[i], frame[(i + 1) % frame.size()]);
        if (line.intersects(edge, &hit) != QLineF::BoundedIntersection)
            continue;
        if (!best || QLineF(hit, near).length() < QLineF(*best, near).length())
            best = hit;
    }
    return best;
}

} // namespace

QList<TextFrameGeometry> Canvas::textChainGeometry(NodeId id) const
{
    const Document& doc = m_editor.document();
    const auto* text = doc.findAs<TextObject>(id);
    if (!text)
        return {};
    QList<NodeId> ids = text->chain();
    if (ids.isEmpty())
        ids = {id};
    QList<TextFrameGeometry> frames;
    for (NodeId member : ids) {
        const auto* t = doc.findAs<TextObject>(member);
        if (!t)
            continue;
        const Page* page = doc.pageOf(member);
        const bool visible = page == &m_editor.currentPage() || page == &doc.masterPage();
        frames << TextFrameGeometry{member, t->transform(), t->frameSize(), visible};
    }
    return frames;
}

void Canvas::paintTextChain(QPainter& p, const QList<TextFrameGeometry>& frames, std::optional<bool> overflow) const
{
    if (frames.isEmpty() || !Options::instance().flag(QStringLiteral("ws/text/frames/showLinks")))
        return;
    bool overflowing = overflow.value_or(false);
    if (!overflow) {
        const auto* head = m_editor.document().findAs<TextObject>(frames.first().id);
        overflowing = head && head->overflows();
    }
    const QTransform view = viewTransform();
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    // Blaue Pfeile: von Rahmenmitte zu Rahmenmitte, nur zwischen den Rändern
    for (int i = 0; i + 1 < frames.size(); ++i) {
        if (!frames[i].visible || !frames[i + 1].visible)
            continue;
        const QPolygonF from = (frames[i].transform * view).map(QPolygonF(QRectF(QPointF(0, 0), frames[i].size)));
        const QPolygonF to = (frames[i + 1].transform * view).map(QPolygonF(QRectF(QPointF(0, 0), frames[i + 1].size)));
        const QLineF centers(from.boundingRect().center(), to.boundingRect().center());
        const std::optional<QPointF> exit = frameEdgeHit(from, centers, centers.p2());
        const std::optional<QPointF> entry = frameEdgeHit(to, centers, centers.p1());
        if (!exit || !entry)
            continue; // Rahmen überlappen
        const QPointF a = *exit;
        const QPointF b = *entry;
        const double length = std::hypot(b.x() - a.x(), b.y() - a.y());
        if (length < 1.0)
            continue;
        const QPointF d = (b - a) / length;
        const QPointF n(-d.y(), d.x());
        p.setPen(QPen(previewColor(), 1.2));
        p.drawLine(a, b);
        p.setPen(Qt::NoPen);
        p.setBrush(previewColor());
        p.drawPolygon(QPolygonF({b, b - d * 8.0 + n * 3.5, b - d * 8.0 - n * 3.5}));
    }
    // Laschen
    auto tab = [&](const QPointF& c, int glyph) {
        const QRectF box(c.x() - kTextTab / 2, c.y() - kTextTab / 2, kTextTab, kTextTab);
        p.setPen(QPen(QColor(0x30, 0x30, 0x30), 1.0));
        p.setBrush(Qt::white);
        p.drawRect(box);
        p.setPen(QPen(QColor(0x30, 0x30, 0x30), 1.0));
        if (glyph == 1) { // verknüpft: Textzeilen
            for (int k = 0; k < 3; ++k) {
                const double y = box.top() + 2.5 + k * 2.0;
                p.drawLine(QPointF(box.left() + 2, y), QPointF(box.right() - 2, y));
            }
        } else if (glyph == 2) { // Überlauf: Pfeil nach unten
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0xd0, 0x20, 0x20));
            p.drawPolygon(QPolygonF({QPointF(box.left() + 1.5, box.top() + 2.5), QPointF(box.right() - 1.5, box.top() + 2.5),
                                     QPointF(box.center().x(), box.bottom() - 1.5)}));
        }
    };
    for (int i = 0; i < frames.size(); ++i) {
        if (!frames[i].visible)
            continue;
        const bool last = i + 1 == frames.size();
        tab(textTabPoint(frames[i], false, view), i > 0 ? 1 : 0);
        tab(textTabPoint(frames[i], true, view), !last ? 1 : overflowing ? 2 : 0);
    }
    p.restore();
}

int Canvas::textLinkTabAt(const QPointF& screen, const QList<TextFrameGeometry>& frames) const
{
    if (frames.isEmpty() || !frames.last().visible || !Options::instance().flag(QStringLiteral("ws/text/frames/showLinks")))
        return -1;
    const QPointF d = screen - textTabPoint(frames.last(), true, viewTransform());
    if (std::abs(d.x()) <= kTextTab / 2 + 1 && std::abs(d.y()) <= kTextTab / 2)
        return int(frames.size()) - 1;
    return -1;
}

void Canvas::keyPressEvent(QKeyEvent* e)
{
    if (tool()->keyPress(e))
        return;
    switch (e->key()) {
    case Qt::Key_Space:
        // Leertaste wechselt zwischen Auswahl- und zuletzt benutztem Zeichenwerkzeug
        setTool(m_toolId == ToolId::Pick ? m_lastDrawingTool : ToolId::Pick);
        return;
    case Qt::Key_F8:
        setTool(ToolId::Text);
        return;
    case Qt::Key_F6:
        setTool(ToolId::Rectangle);
        return;
    case Qt::Key_F7:
        setTool(ToolId::Ellipse);
        return;
    case Qt::Key_Y:
        if (e->modifiers() == Qt::NoModifier) {
            setTool(ToolId::Polygon);
            return;
        }
        break;
    case Qt::Key_F5:
        setTool(ToolId::Freehand);
        return;
    case Qt::Key_F10:
        setTool(ToolId::Shape);
        return;
    case Qt::Key_I:
        if (e->modifiers() == Qt::NoModifier) {
            setTool(ToolId::ArtisticMedia);
            return;
        }
        break;
    case Qt::Key_X:
        if (e->modifiers() == Qt::NoModifier) {
            setTool(ToolId::Eraser);
            return;
        }
        break;

    case Qt::Key_G:
        if (e->modifiers() == Qt::NoModifier) {
            setTool(ToolId::InteractiveFill);
            return;
        }
        break;
    case Qt::Key_M:
        if (e->modifiers() == Qt::NoModifier) {
            setTool(ToolId::Mesh);
            return;
        }
        break;
    case Qt::Key_Z:
        if (e->modifiers() == Qt::NoModifier) {
            setTool(ToolId::Zoom);
            return;
        }
        break;
    default:
        break;
    }
    QWidget::keyPressEvent(e);
}

} // namespace vek

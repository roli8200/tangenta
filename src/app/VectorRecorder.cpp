#include "VectorRecorder.h"

#include <QPainter>
#include <QPixmap>

#include <climits>

#include <cmath>

namespace vek {

namespace {

class RecordingEngine final : public QPaintEngine {
public:
    RecordingEngine() : QPaintEngine(QPaintEngine::AllFeatures) {}

    bool begin(QPaintDevice*) override { return true; }
    bool end() override { return true; }
    Type type() const override { return QPaintEngine::User; }

    void updateState(const QPaintEngineState& s) override
    {
        const QPaintEngine::DirtyFlags f = s.state();
        if (f & DirtyTransform)
            m_transform = s.transform();
        if (f & DirtyPen)
            m_pen = s.pen();
        if (f & DirtyBrush)
            m_brush = s.brush();
        if (f & DirtyBrushOrigin)
            m_brushOrigin = s.brushOrigin();
        if (f & DirtyOpacity)
            m_opacity = s.opacity();
        if (f & DirtyCompositionMode)
            m_composition = s.compositionMode();
        if (f & DirtyClipEnabled)
            m_clipEnabled = s.isClipEnabled();
        if (f & (DirtyClipPath | DirtyClipRegion)) {
            QPainterPath next;
            if (f & DirtyClipPath)
                next = s.clipPath();
            else
                next.addRegion(s.clipRegion());
            next = m_transform.map(next);
            switch (s.clipOperation()) {
            case Qt::NoClip: m_clip.reset(); break;
            case Qt::ReplaceClip: m_clip = next; break;
            case Qt::IntersectClip: m_clip = m_clip ? m_clip->intersected(next) : next; break;
            }
            m_clipEnabled = s.clipOperation() != Qt::NoClip;
        }
    }

    void drawPath(const QPainterPath& path) override
    {
        RecordedItem item;
        item.path = m_transform.map(path);
        item.path.setFillRule(path.fillRule());
        if (m_brush.style() != Qt::NoBrush) {
            QBrush b = m_brush;
            b.setTransform(b.transform() * QTransform::fromTranslate(m_brushOrigin.x(), m_brushOrigin.y()) * m_transform);
            item.fill = b;
        }
        if (m_pen.style() != Qt::NoPen && m_pen.brush().style() != Qt::NoBrush) {
            QPen p = m_pen;
            const double scale = p.isCosmetic() ? 1.0 : std::sqrt(std::abs(m_transform.determinant()));
            p.setWidthF(std::max(0.0, (p.widthF() <= 0 ? 1.0 : p.widthF()) * scale));
            item.stroke = p;
        }
        if (!item.fill && !item.stroke)
            return;
        if (m_clipEnabled && m_clip)
            item.clip = m_clip;
        item.opacity = m_opacity;
        item.composition = m_composition;
        m_items << item;
    }

    void drawPolygon(const QPointF* points, int count, PolygonDrawMode mode) override
    {
        QPainterPath p;
        p.moveTo(points[0]);
        for (int i = 1; i < count; ++i)
            p.lineTo(points[i]);
        if (mode != PolylineMode)
            p.closeSubpath();
        p.setFillRule(mode == OddEvenMode ? Qt::OddEvenFill : Qt::WindingFill);
        if (mode == PolylineMode) {
            const QBrush keep = m_brush;
            m_brush = Qt::NoBrush;
            drawPath(p);
            m_brush = keep;
        } else {
            drawPath(p);
        }
    }

    void drawImage(const QRectF& r, const QImage& image, const QRectF& sr, Qt::ImageConversionFlags) override
    {
        RecordedItem item;
        item.kind = RecordedItem::Kind::Image;
        item.image = image.copy(sr.toAlignedRect()).convertToFormat(QImage::Format_ARGB32);
        item.imageTransform = QTransform::fromScale(r.width() / std::max(1, item.image.width()), r.height() / std::max(1, item.image.height()))
                              * QTransform::fromTranslate(r.x(), r.y()) * m_transform;
        if (m_clipEnabled && m_clip)
            item.clip = m_clip;
        item.opacity = m_opacity;
        item.composition = m_composition;
        m_items << item;
    }

    void drawPixmap(const QRectF& r, const QPixmap& pm, const QRectF& sr) override { drawImage(r, pm.toImage(), sr, Qt::AutoColor); }

    void drawTiledPixmap(const QRectF& r, const QPixmap& pixmap, const QPointF& s) override
    {
        // Als Pinsel-Fläche aufzeichnen
        QBrush b(pixmap);
        b.setTransform(QTransform::fromTranslate(-s.x(), -s.y()));
        const QBrush keep = m_brush;
        const QPen pen = m_pen;
        m_brush = b;
        m_pen = Qt::NoPen;
        QPainterPath p;
        p.addRect(r);
        drawPath(p);
        m_brush = keep;
        m_pen = pen;
    }

    QList<RecordedItem> m_items;

private:
    QTransform m_transform;
    QPen m_pen;
    QBrush m_brush;
    QPointF m_brushOrigin;
    double m_opacity = 1.0;
    QPainter::CompositionMode m_composition = QPainter::CompositionMode_SourceOver;
    std::optional<QPainterPath> m_clip;
    bool m_clipEnabled = false;
};

bool translucent(const QBrush& b)
{
    if (b.style() == Qt::NoBrush)
        return false;
    if (const QGradient* g = b.gradient()) {
        for (const auto& stop : g->stops())
            if (stop.second.alpha() < 255)
                return true;
        return false;
    }
    if (b.style() == Qt::TexturePattern)
        return b.textureImage().hasAlphaChannel() || b.texture().hasAlphaChannel();
    return b.color().alpha() < 255;
}

} // namespace

bool RecordedItem::needsBlending() const
{
    if (opacity < 0.999 || composition != QPainter::CompositionMode_SourceOver)
        return true;
    if (kind == Kind::Image) {
        if (!image.hasAlphaChannel())
            return false;
        for (int y = 0; y < image.height(); ++y) {
            const auto* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
            for (int x = 0; x < image.width(); ++x)
                if (qAlpha(line[x]) < 255)
                    return true;
        }
        return false;
    }
    return (fill && translucent(*fill)) || (stroke && translucent(stroke->brush()));
}

QPainterPath RecordedItem::coverage() const
{
    QPainterPath shape;
    if (kind == Kind::Image) {
        shape.addPolygon(imageTransform.map(QPolygonF(QRectF(image.rect()))));
        shape.closeSubpath();
    } else {
        if (fill)
            shape = path;
        if (stroke) {
            QPainterPathStroker stroker(*stroke);
            const QPainterPath outline = stroker.createStroke(path);
            shape = shape.isEmpty() ? outline : shape.united(outline);
        }
    }
    if (clip)
        shape = shape.intersected(*clip);
    return shape;
}

VectorRecorder::VectorRecorder(const QSizeF& size, double dpi)
    : m_size(size), m_dpi(dpi), m_engine(std::make_unique<RecordingEngine>())
{
}

VectorRecorder::~VectorRecorder() = default;

QPaintEngine* VectorRecorder::paintEngine() const
{
    return m_engine.get();
}

const QList<RecordedItem>& VectorRecorder::items() const
{
    return static_cast<RecordingEngine*>(m_engine.get())->m_items;
}

int VectorRecorder::metric(PaintDeviceMetric metric) const
{
    switch (metric) {
    case PdmWidth: return int(std::ceil(m_size.width()));
    case PdmHeight: return int(std::ceil(m_size.height()));
    case PdmWidthMM: return int(std::ceil(m_size.width() / m_dpi * 25.4));
    case PdmHeightMM: return int(std::ceil(m_size.height() / m_dpi * 25.4));
    case PdmNumColors: return INT_MAX;
    case PdmDepth: return 32;
    case PdmDpiX:
    case PdmDpiY:
    case PdmPhysicalDpiX:
    case PdmPhysicalDpiY: return int(std::lround(m_dpi));
    case PdmDevicePixelRatio: return 1;
    case PdmDevicePixelRatioScaled: return int(QPaintDevice::devicePixelRatioFScale());
    case PdmDevicePixelRatioF_EncodedA:
    case PdmDevicePixelRatioF_EncodedB: return QPaintDevice::metric(metric);
    }
    return 0;
}

} // namespace vek

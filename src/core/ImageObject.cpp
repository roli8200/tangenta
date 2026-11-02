#include "ImageObject.h"

#include "ImagePool.h"

namespace vek {

namespace {

QList<SubPath> rectFrame(const QSizeF& size)
{
    SubPath s;
    s.closed = true;
    for (const QPointF& p : {QPointF(0, 0), QPointF(size.width(), 0), QPointF(size.width(), size.height()),
                             QPointF(0, size.height())}) {
        CurveNode n;
        n.point = p;
        s.nodes << n;
    }
    return {s};
}

} // namespace

ImageObject::ImageObject(const QString& key, const QSizeF& size) : m_key(key), m_size(size)
{
    resetFrame();
    Style s;
    s.fill = Fill::none();
    s.stroke.reset();
    setStyle(s);
}

QPainterPath ImageObject::localPath() const
{
    QPainterPath p = CurveObject::toPainterPath(m_frame);
    p.setFillRule(Qt::WindingFill);
    return p;
}

bool ImageObject::hitTest(const QPointF& point, double tolerance, bool) const
{
    // Ein Bild ist überall im Rahmen getroffen, nicht nur am Umriss
    return DrawObject::hitTest(point, tolerance, true);
}

QImage ImageObject::image() const
{
    return imagepool::image(m_key);
}

QImage ImageObject::displayImage() const
{
    return imagepool::displayImage(m_key);
}

void ImageObject::resetFrame()
{
    m_frame = rectFrame(m_size);
}

bool ImageObject::isCropped() const
{
    return m_frame != rectFrame(m_size);
}

void ImageObject::writeProperties(QJsonObject& out) const
{
    DrawObject::writeProperties(out);
    out[QStringLiteral("image")] = m_key;
    out[QStringLiteral("width")] = m_size.width();
    out[QStringLiteral("height")] = m_size.height();
    out[QStringLiteral("frame")] = CurveObject::pathsToJson(m_frame);
}

void ImageObject::readProperties(const QJsonObject& in)
{
    DrawObject::readProperties(in);
    m_key = in.value(QStringLiteral("image")).toString();
    m_size = QSizeF(in.value(QStringLiteral("width")).toDouble(), in.value(QStringLiteral("height")).toDouble());
    m_frame = CurveObject::pathsFromJson(in.value(QStringLiteral("frame")).toArray());
    if (m_frame.isEmpty())
        resetFrame();
}

} // namespace vek

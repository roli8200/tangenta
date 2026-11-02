#include "GroupObject.h"

namespace vek {

QPainterPath GroupObject::path() const
{
    QPainterPath p;
    for (int i = 0; i < childCount(); ++i) {
        if (auto* obj = node_cast<DrawObject>(child(i)))
            p.addPath(obj->path());
    }
    return p;
}

QRectF GroupObject::boundingRect() const
{
    QRectF r;
    for (int i = 0; i < childCount(); ++i) {
        if (auto* obj = node_cast<DrawObject>(child(i)))
            r = r.isNull() ? obj->boundingRect() : r.united(obj->boundingRect());
    }
    return r;
}

bool GroupObject::hitTest(const QPointF& point, double tolerance, bool treatAsFilled) const
{
    for (int i = childCount() - 1; i >= 0; --i) {
        auto* obj = node_cast<DrawObject>(child(i));
        if (obj && obj->hitTest(point, tolerance, treatAsFilled))
            return true;
    }
    return false;
}

void GroupObject::applyTransform(const QTransform& m)
{
    for (int i = 0; i < childCount(); ++i) {
        if (auto* obj = node_cast<DrawObject>(child(i)))
            obj->applyTransform(m);
    }
}

void GroupObject::setStyle(const Style& s)
{
    for (int i = 0; i < childCount(); ++i) {
        if (auto* obj = node_cast<DrawObject>(child(i)))
            obj->setStyle(s);
    }
}

} // namespace vek

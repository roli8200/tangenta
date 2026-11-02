#include "PowerClipObject.h"

namespace vek {

const DrawObject* PowerClipObject::frame() const
{
    return childCount() > 0 ? node_cast<DrawObject>(child(0)) : nullptr;
}

QList<const DrawObject*> PowerClipObject::contents() const
{
    QList<const DrawObject*> list;
    for (int i = 1; i < childCount(); ++i)
        if (const auto* o = node_cast<DrawObject>(child(i)))
            list << o;
    return list;
}

QPainterPath PowerClipObject::path() const
{
    const DrawObject* f = frame();
    return f ? f->path() : QPainterPath();
}

QRectF PowerClipObject::boundingRect() const
{
    const DrawObject* f = frame();
    return f ? f->boundingRect() : QRectF();
}

bool PowerClipObject::hitTest(const QPointF& point, double tolerance, bool) const
{
    // Inhalte sind nur im Rahmen sichtbar: der Rahmen als Fläche zählt
    const DrawObject* f = frame();
    return f && f->hitTest(point, tolerance, true);
}

void PowerClipObject::applyTransform(const QTransform& m)
{
    for (int i = 0; i < childCount(); ++i) {
        if (i > 0 && !m_locked)
            break;
        if (auto* obj = node_cast<DrawObject>(child(i)))
            obj->applyTransform(m);
    }
}

void PowerClipObject::setStyle(const Style& s)
{
    // Füllung und Kontur gehören dem Rahmen
    if (childCount() > 0)
        if (auto* f = node_cast<DrawObject>(child(0)))
            f->setStyle(s);
}

void PowerClipObject::writeProperties(QJsonObject& out) const
{
    DrawObject::writeProperties(out);
    if (!m_locked)
        out[QStringLiteral("contentsLocked")] = false;
}

void PowerClipObject::readProperties(const QJsonObject& in)
{
    DrawObject::readProperties(in);
    m_locked = in.value(QStringLiteral("contentsLocked")).toBool(true);
}

} // namespace vek

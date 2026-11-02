#include "CloneObject.h"

#include "Serialization.h"

namespace vek {

std::shared_ptr<DrawObject> CloneObject::snapshot() const
{
    if (!m_snapshot && !m_snapshotJson.isEmpty()) {
        std::unique_ptr<Node> n = serial::fromJson(m_snapshotJson, serial::Ids::Drop);
        if (auto* d = node_cast<DrawObject>(n.get())) {
            n.release();
            m_snapshot.reset(d);
        }
    }
    return m_snapshot;
}

bool CloneObject::resolve(const std::function<const DrawObject*(NodeId)>& lookup) const
{
    const DrawObject* master = lookup(m_master);
    if (!master)
        return false;
    QJsonObject json = serial::toJson(*master, serial::Ids::Drop);
    if (json == m_snapshotJson)
        return false;
    m_snapshotJson = json;
    m_snapshot.reset();
    m_rendered.reset();
    return true;
}

std::shared_ptr<const DrawObject> CloneObject::rendered() const
{
    if (m_rendered && m_renderedFor == transform())
        return m_rendered;
    const std::shared_ptr<DrawObject> snap = snapshot();
    if (!snap)
        return nullptr;
    std::unique_ptr<Node> copy = serial::fromJson(m_snapshotJson, serial::Ids::Drop);
    auto* d = node_cast<DrawObject>(copy.get());
    if (!d)
        return nullptr;
    copy.release();
    std::shared_ptr<DrawObject> owned(d);
    // Gruppen bewegen ihren Inhalt mit
    std::function<void(DrawObject&)> apply = [&](DrawObject& o) {
        o.applyTransform(transform());
        for (int i = 0; i < o.childCount(); ++i)
            if (auto* c = node_cast<DrawObject>(o.child(i)))
                apply(*c);
    };
    if (d->childCount() > 0) {
        for (int i = 0; i < d->childCount(); ++i)
            if (auto* c = node_cast<DrawObject>(d->child(i)))
                apply(*c);
    } else {
        d->applyTransform(transform());
    }
    m_rendered = owned;
    m_renderedFor = transform();
    return m_rendered;
}

QPainterPath CloneObject::localPath() const
{
    const std::shared_ptr<DrawObject> snap = snapshot();
    return snap ? snap->path() : QPainterPath();
}

QRectF CloneObject::boundingRect() const
{
    const auto r = rendered();
    return r ? r->boundingRect() : QRectF();
}

bool CloneObject::hitTest(const QPointF& point, double tolerance, bool treatAsFilled) const
{
    const auto r = rendered();
    return r && r->hitTest(point, tolerance, treatAsFilled);
}

std::unique_ptr<DrawObject> CloneObject::detach() const
{
    const auto r = rendered();
    if (!r)
        return nullptr;
    std::unique_ptr<Node> copy = serial::fromJson(serial::toJson(*r, serial::Ids::Drop), serial::Ids::Drop);
    auto* d = node_cast<DrawObject>(copy.get());
    if (!d)
        return nullptr;
    copy.release();
    return std::unique_ptr<DrawObject>(d);
}

void CloneObject::writeProperties(QJsonObject& out) const
{
    DrawObject::writeProperties(out);
    out[QStringLiteral("master")] = QString::number(m_master);
    out[QStringLiteral("symbol")] = m_symbol;
    out[QStringLiteral("snapshot")] = m_snapshotJson;
}

void CloneObject::readProperties(const QJsonObject& in)
{
    DrawObject::readProperties(in);
    m_master = in.value(QStringLiteral("master")).toString().toULongLong();
    m_symbol = in.value(QStringLiteral("symbol")).toBool();
    m_snapshotJson = in.value(QStringLiteral("snapshot")).toObject();
    m_snapshot.reset();
    m_rendered.reset();
}

} // namespace vek

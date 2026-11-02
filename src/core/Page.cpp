#include "Page.h"

#include "DrawObject.h"

#include <QJsonArray>
#include <QtMath>

#include <cmath>

namespace vek {

bool DocumentRoot::canContain(const Node& child) const
{
    return node_cast<Page>(&child) != nullptr;
}

bool Page::canContain(const Node& child) const
{
    return node_cast<Layer>(&child) != nullptr;
}

void Page::writeProperties(QJsonObject& out) const
{
    Node::writeProperties(out);
    out[QStringLiteral("width")] = m_size.width();
    out[QStringLiteral("height")] = m_size.height();
    if (!m_guides.isEmpty()) {
        QJsonArray guides;
        for (const Guide& g : m_guides) {
            QJsonObject o{{QStringLiteral("x"), g.anchor.x()},
                          {QStringLiteral("y"), g.anchor.y()},
                          {QStringLiteral("angle"), g.angle}};
            if (g.preset)
                o[QStringLiteral("preset")] = true;
            guides.append(o);
        }
        out[QStringLiteral("guides")] = guides;
    }
    if (m_master) {
        out[QStringLiteral("nudge")] = m_nudge;
        out[QStringLiteral("duplicateX")] = m_duplicateOffset.x();
        out[QStringLiteral("duplicateY")] = m_duplicateOffset.y();
        if (!m_options.isEmpty())
            out[QStringLiteral("options")] = m_options;
    }
}

void Page::readProperties(const QJsonObject& in)
{
    Node::readProperties(in);
    m_size = QSizeF(in.value(QStringLiteral("width")).toDouble(A4.width()),
                    in.value(QStringLiteral("height")).toDouble(A4.height()));
    m_nudge = in.value(QStringLiteral("nudge")).toDouble(0.1);
    m_duplicateOffset = QPointF(in.value(QStringLiteral("duplicateX")).toDouble(5.0),
                                in.value(QStringLiteral("duplicateY")).toDouble(5.0));
    m_options = in.value(QStringLiteral("options")).toObject();
    m_guides.clear();
    for (const QJsonValue& v : in.value(QStringLiteral("guides")).toArray()) {
        const QJsonObject o = v.toObject();
        m_guides.append(Guide{QPointF(o.value(QStringLiteral("x")).toDouble(), o.value(QStringLiteral("y")).toDouble()),
                              o.value(QStringLiteral("angle")).toDouble(), o.value(QStringLiteral("preset")).toBool()});
    }
}

QPointF Guide::direction() const
{
    const double a = qDegreesToRadians(angle);
    return QPointF(std::cos(a), -std::sin(a));
}

bool Guide::isHorizontal() const
{
    return std::abs(std::remainder(angle, 180.0)) < 1e-9;
}

bool Guide::isVertical() const
{
    return std::abs(std::remainder(angle - 90.0, 180.0)) < 1e-9;
}

std::unique_ptr<Page> createPage(const QSizeF& size, const QString& layerName, bool master)
{
    auto page = std::make_unique<Page>(master);
    page->setSize(size);
    auto layer = std::make_unique<Layer>();
    layer->setName(layerName);
    attachDetachedChild(*page, std::move(layer));
    return page;
}

bool Layer::canContain(const Node& child) const
{
    return node_cast<DrawObject>(&child) != nullptr;
}

void Layer::writeProperties(QJsonObject& out) const
{
    Node::writeProperties(out);
    out[QStringLiteral("visible")] = m_visible;
    out[QStringLiteral("locked")] = m_locked;
    out[QStringLiteral("printable")] = m_printable;
}

void Layer::readProperties(const QJsonObject& in)
{
    Node::readProperties(in);
    m_visible = in.value(QStringLiteral("visible")).toBool(true);
    m_locked = in.value(QStringLiteral("locked")).toBool(false);
    m_printable = in.value(QStringLiteral("printable")).toBool(true);
}

} // namespace vek

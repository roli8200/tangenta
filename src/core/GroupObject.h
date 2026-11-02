#pragma once

#include "DrawObject.h"

namespace vek {

// Gruppe von Zeichenobjekten. Hat keine eigene Geometrie und keine eigene Transformation:
// Transformationen wirken direkt auf die Kinder. Deshalb muss eine
// Transformation einer Gruppe mit Transaction::modifySubtree() erfolgen.
class GroupObject final : public DrawObject {
public:
    QString typeName() const override { return QStringLiteral("group"); }
    bool canContain(const Node& child) const override { return node_cast<DrawObject>(&child) != nullptr; }

    QPainterPath localPath() const override { return path(); }
    QPainterPath path() const override;
    QRectF boundingRect() const override;
    bool hitTest(const QPointF& point, double tolerance, bool treatAsFilled) const override;

    void applyTransform(const QTransform& m) override;
    void setStyle(const Style& s) override;
};

} // namespace vek

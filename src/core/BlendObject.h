#pragma once

#include "CurveObject.h"
#include "DrawObject.h"

#include <memory>

namespace vek {

// Überblendung: Start- (erstes Kind) und Endobjekt (zweites Kind) mit
// berechneten Zwischenstufen, die Form, Füllfarbe und Kontur gleichmässig überblenden. Wie eine
// Gruppe ohne eigene Transformation. Ändern sich Start oder Ende, folgen die Stufen.
class BlendObject final : public DrawObject {
public:
    QString typeName() const override { return QStringLiteral("blend"); }
    bool canContain(const Node& child) const override { return node_cast<DrawObject>(&child) != nullptr; }

    const DrawObject* start() const;
    const DrawObject* end() const;
    int steps() const { return m_steps; }
    void setSteps(int steps) { m_steps = std::clamp(steps, 1, 999); }
    // Drehung der Zwischenstufen (Grad, über die ganze Überblendung)
    double rotation() const { return m_rotation; }
    void setRotation(double deg) { m_rotation = deg; }

    // Zwischenstufen als eigenständige Kurven (Seitenkoordinaten), von Start nach Ende
    QList<std::shared_ptr<CurveObject>> intermediates() const;

    QPainterPath localPath() const override { return path(); }
    QPainterPath path() const override;
    QRectF boundingRect() const override;
    bool hitTest(const QPointF& point, double tolerance, bool treatAsFilled) const override;
    void applyTransform(const QTransform& m) override;

    QList<PropertyDescriptor> propertyDescriptors() const override;
    QVariant propertyValue(const QString& id) const override;
    bool setPropertyValue(const QString& id, const QVariant& value) override;
    void writeProperties(QJsonObject& out) const override;
    void readProperties(const QJsonObject& in) override;

    // Formen auf gleich viele Punkte bringen und überblenden (für Tests offen)
    static QList<QPolygonF> interpolate(const QPainterPath& a, const QPainterPath& b, double t, int samples = 160);

private:
    int m_steps = 20;
    double m_rotation = 0.0;
};

} // namespace vek

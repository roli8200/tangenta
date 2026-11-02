#pragma once

#include "DrawObject.h"

#include <QSizeF>

namespace vek {

// Ellipse (F7); lokal im Rechteck (0,0) bis (width,height). Als
// Kreissegment (Tortenstück) oder Bogen zwischen Anfangs- und Endwinkel. Winkel in Grad
// gegen den Uhrzeigersinn, 0° zeigt nach rechts. Bleibt beim Skalieren eine Ellipse.
class EllipseObject final : public DrawObject {
public:
    enum class Kind { Ellipse, Pie, Arc };

    EllipseObject() = default;
    explicit EllipseObject(const QSizeF& size) : m_size(size) {}

    QString typeName() const override { return QStringLiteral("ellipse"); }
    QPainterPath localPath() const override;
    void applyTransform(const QTransform& m) override;

    QSizeF size() const { return m_size; }
    void setSize(const QSizeF& size) { m_size = size; }
    Kind kind() const { return m_kind; }
    void setKind(Kind kind) { m_kind = kind; }
    double startAngle() const { return m_start; }
    double endAngle() const { return m_end; }
    void setAngles(double start, double end) { m_start = start; m_end = end; }
    // Segment im Uhrzeigersinn von Anfang zu Ende statt dagegen ("Richtung ändern")
    bool clockwise() const { return m_clockwise; }
    void setClockwise(bool cw) { m_clockwise = cw; }

    QList<PropertyDescriptor> propertyDescriptors() const override;
    QVariant propertyValue(const QString& id) const override;
    bool setPropertyValue(const QString& id, const QVariant& value) override;

    void writeProperties(QJsonObject& out) const override;
    void readProperties(const QJsonObject& in) override;

private:
    QSizeF m_size;
    Kind m_kind = Kind::Ellipse;
    double m_start = 0.0;
    double m_end = 270.0;
    bool m_clockwise = false;
};

} // namespace vek

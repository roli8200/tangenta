#pragma once

#include "DrawObject.h"

#include <QSizeF>

namespace vek {

// Polygon, Stern und komplexer Stern, lokal im Rechteck (0,0) bis
// (width,height); die erste Ecke zeigt nach oben. Bleibt beim Skalieren ein Polygon.
//  - Polygon: points gleichmässig verteilte Ecken
//  - Stern: Spitzen und Einbuchtungen im Wechsel; sharpness 1..99 macht die Spitzen schärfer
//  - Komplexer Stern: Sternvieleck, jede Ecke wird mit der (sharpness+1)-ten verbunden
class PolygonObject final : public DrawObject {
public:
    enum class Kind { Polygon, Star, ComplexStar };

    PolygonObject() = default;
    PolygonObject(const QSizeF& size, Kind kind, int points, int sharpness);

    QString typeName() const override { return QStringLiteral("polygon"); }
    QPainterPath localPath() const override;
    void applyTransform(const QTransform& m) override;

    QSizeF size() const { return m_size; }
    Kind kind() const { return m_kind; }
    int points() const { return m_points; }
    int sharpness() const { return m_sharpness; }

    // Gültige Bereiche je Art
    static int minPoints(Kind kind) { return kind == Kind::ComplexStar ? 5 : 3; }
    static int maxSharpness(Kind kind, int points);
    static int clampSharpness(Kind kind, int points, int sharpness);

    QList<PropertyDescriptor> propertyDescriptors() const override;
    QVariant propertyValue(const QString& id) const override;
    bool setPropertyValue(const QString& id, const QVariant& value) override;

    void writeProperties(QJsonObject& out) const override;
    void readProperties(const QJsonObject& in) override;

private:
    QSizeF m_size;
    Kind m_kind = Kind::Polygon;
    int m_points = 5;
    int m_sharpness = 53;
};

} // namespace vek

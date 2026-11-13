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

    // Formwerkzeug: symmetrisch gezogene Knoten. Ecken (Sternspitzen) und Seitenmitten
    // (Einbuchtungen) liegen je auf einem Ring; Faktor auf dessen Radius und Drehung in Grad.
    // Beim Polygon werden gezogene Seitenmitten zu glatten Kurvenknoten (geschwungenes Polygon).
    struct NodeShape {
        double outerScale = 1.0;
        double outerTwist = 0.0;
        double innerScale = 1.0;
        double innerTwist = 0.0;
        bool innerMoved() const { return innerScale != 1.0 || innerTwist != 0.0; }
        bool isDefault() const { return *this == NodeShape{}; }
        friend bool operator==(const NodeShape&, const NodeShape&) = default;
    };
    const NodeShape& nodeShape() const { return m_nodeShape; }
    void setNodeShape(const NodeShape& s) { m_nodeShape = s; }
    // Knoten für das Formwerkzeug (lokal), 2 * points: gerade Indizes Ecken, ungerade Seitenmitten.
    // Komplexe Sterne haben keine (leer).
    QList<QPointF> shapeNodes() const;
    // Grundradius eines Rings (Anteil der Halbachsen) ohne NodeShape
    double baseRadius(bool inner) const;

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
    NodeShape m_nodeShape;
};

} // namespace vek

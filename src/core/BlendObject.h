#pragma once

#include "CurveObject.h"
#include "DrawObject.h"

#include <memory>

namespace vek {

// Überblendung: Start- (erstes Kind) und Endobjekt (zweites Kind) mit
// berechneten Zwischenstufen, die Form, Füllung und Kontur gleichmässig überblenden. Wie eine
// Gruppe ohne eigene Transformation. Ändern sich Start oder Ende, folgen die Stufen.
// Ein drittes Kind ist der Pfad, an dem die Zwischenstufen entlang laufen (Überblendung an Pfad).
class BlendObject final : public DrawObject {
public:
    QString typeName() const override { return QStringLiteral("blend"); }
    bool canContain(const Node& child) const override { return node_cast<DrawObject>(&child) != nullptr; }

    const DrawObject* start() const;
    const DrawObject* end() const;
    // Pfad der Überblendung (drittes Kind) oder nullptr
    const DrawObject* pathObject() const;
    int steps() const { return m_steps; }
    void setSteps(int steps) { m_steps = std::clamp(steps, 1, 999); }
    // Drehung der Zwischenstufen (Grad, über die ganze Überblendung)
    double rotation() const { return m_rotation; }
    void setRotation(double deg) { m_rotation = deg; }
    // An Pfad: über den ganzen Pfad (Start und Ende an seine Enden) bzw. Stufen nach der
    // Pfadrichtung drehen
    bool fullPath() const { return m_fullPath; }
    void setFullPath(bool on);
    bool rotateAlongPath() const { return m_rotateAll; }
    void setRotateAlongPath(bool on) { m_rotateAll = on; }
    // Stapelreihenfolge umkehren: das Ende liegt zuunterst, der Start obenauf
    bool reversed() const { return m_reversed; }
    void setReversed(bool on) { m_reversed = on; }
    // Start, Zwischenstufen und Ende in Zeichenreihenfolge (von unten nach oben), ohne den Pfad
    QList<const DrawObject*> paintOrder() const;

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
    // Füllung zwischen a (t = 0) und b (t = 1): Verläufe gleicher Art bleiben Verläufe
    static Fill mixFill(const Fill& a, const Fill& b, double t);

private:
    QList<std::shared_ptr<CurveObject>> computeIntermediates() const;

    int m_steps = 20;
    double m_rotation = 0.0;
    bool m_fullPath = false;
    bool m_rotateAll = false;
    bool m_reversed = false;

    // Zwischenstufen hängen nur an Start, Ende, Schritten und Drehung. Neu berechnet nur, wenn
    // sich davon etwas ändert: Zeichnen, Treffertest, Begrenzungsrahmen und Leiste fragten sonst
    // je Bildaufbau mehrmals neu an (bei vielen Stufen Sekunden je Änderung).
    struct Cache {
        QPainterPath startPath, endPath, guide;
        Style startStyle, endStyle;
        int steps = 0;
        double rotation = 0.0;
        bool fullPath = false, rotateAll = false;
        QList<std::shared_ptr<CurveObject>> intermediates;
        QPainterPath path;
        QRectF bounds;
    };
    std::shared_ptr<const Cache> cache() const;
    mutable std::shared_ptr<const Cache> m_cache;
};

} // namespace vek

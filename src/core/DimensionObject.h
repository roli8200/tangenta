#pragma once

#include "DrawObject.h"

#include <QPointF>
#include <QString>

namespace vek {

// Bemassung: Masslinie mit Hilfslinien, Pfeilen und Masszahl. Die Masszahl
// rechnet sich aus den Punkten; verschiebt oder skaliert man die Bemassung, misst sie neu.
//  - Parallel:   Abstand p1–p2, Masslinie parallel dazu durch den Lagepunkt
//  - Horizontal: waagrechter Abstand, Masslinie auf der Höhe des Lagepunkts
//  - Vertikal:   senkrechter Abstand, Masslinie an der x-Lage des Lagepunkts
//  - Winkel:     Winkel zwischen den Schenkeln p1 und p2 um den Scheitel, Bogen durch den Lagepunkt
// Punkte liegen in Seitenkoordinaten (mm); die Transformation bleibt die Einheitsmatrix.
class DimensionObject final : public DrawObject {
public:
    enum class Kind { Parallel, Horizontal, Vertical, Angular };

    DimensionObject() = default;
    DimensionObject(Kind kind, const QPointF& vertexOrP1, const QPointF& p1OrP2, const QPointF& p2OrPosition,
                    const QPointF& position = {});

    QString typeName() const override { return QStringLiteral("dimension"); }
    QPainterPath localPath() const override;
    void applyTransform(const QTransform& m) override;

    Kind kind() const { return m_kind; }
    // Gemessener Wert in der eingestellten Einheit (Länge bzw. Winkel)
    double measuredValue() const;
    QString text() const;

    // Masslinien und Hilfslinien (offen, mit der Kontur gezeichnet)
    QPainterPath linePath() const;
    // Pfeilspitzen und Masszahl (mit der Konturfarbe gefüllt)
    QPainterPath fillPath() const;

    // Darstellung der Masszahl
    int precision() const { return m_precision; }
    void setPrecision(int decimals) { m_precision = std::clamp(decimals, 0, 6); }
    int units() const { return m_units; }
    void setUnits(int units) { m_units = units; }
    bool fraction() const { return m_fraction; }
    void setFraction(bool on) { m_fraction = on; }
    QString prefix() const { return m_prefix; }
    void setPrefix(const QString& s) { m_prefix = s; }
    QString suffix() const { return m_suffix; }
    void setSuffix(const QString& s) { m_suffix = s; }
    double scale() const { return m_scale; }
    void setScale(double s) { m_scale = s > 0 ? s : 1.0; }
    QString fontFamily() const { return m_font; }
    void setFont(const QString& family, double sizePt) { m_font = family; m_sizePt = sizePt; }
    double fontSize() const { return m_sizePt; }

    // Einheiten der Längen (Index) und Winkel (0 = Grad, 1 = Radiant, 2 = Gon)
    static QStringList lengthUnits();
    static double lengthUnitInMm(int unit);

    QList<PropertyDescriptor> propertyDescriptors() const override;
    QVariant propertyValue(const QString& id) const override;
    bool setPropertyValue(const QString& id, const QVariant& value) override;

    void writeProperties(QJsonObject& out) const override;
    void readProperties(const QJsonObject& in) override;

private:
    struct Geometry {
        QPainterPath lines;
        QPainterPath fills;
    };
    Geometry build() const;

    Kind m_kind = Kind::Parallel;
    QPointF m_a; // Parallel/H/V: p1; Winkel: Scheitel
    QPointF m_b; // Parallel/H/V: p2; Winkel: Schenkel 1
    QPointF m_c; // Parallel/H/V: Lagepunkt; Winkel: Schenkel 2
    QPointF m_d; // Winkel: Lagepunkt (Radius des Bogens)
    int m_precision = 2;
    int m_units = 0;
    bool m_fraction = false;
    QString m_prefix;
    QString m_suffix;
    double m_scale = 1.0;
    QString m_font = QStringLiteral("Arial");
    double m_sizePt = 10.0;
};

} // namespace vek

#pragma once

#include "DrawObject.h"

#include <QSizeF>

#include <algorithm>
#include <array>
#include <cmath>

namespace vek {

// Rechteck mit Breite, Höhe und Ecken; lokal von (0,0) bis (width,height).
//
// Wie gewohnt bleibt ein Rechteck beim Skalieren, Strecken und Drehen ein Rechteck:
// Solange es nicht geneigt ist, wandert die Skalierung aus der Matrix in Breite und Höhe.
// Die Eckenradien bleiben dabei in mm erhalten.
class RectangleObject final : public DrawObject {
public:
    enum class CornerType { Round, Scallop, Chamfer };
    enum Corner { TopLeft, TopRight, BottomRight, BottomLeft };

    RectangleObject() = default;
    explicit RectangleObject(const QSizeF& size) : m_size(size) {}

    QString typeName() const override { return QStringLiteral("rectangle"); }
    QPainterPath localPath() const override;

    void applyTransform(const QTransform& m) override;

    QSizeF size() const { return m_size; }
    void setSize(const QSizeF& size) { m_size = size; }

    CornerType cornerType() const { return m_cornerType; }
    void setCornerType(CornerType t) { m_cornerType = t; }
    double cornerRadius(Corner c) const { return m_radii[c]; }
    // Begrenzt auf die halbe kürzere Seite
    void setCornerRadius(Corner c, double radius);
    // Gekoppelt: Ändern eines Radius ändert alle
    bool cornersLinked() const { return m_cornersLinked; }
    void setCornersLinked(bool linked) { m_cornersLinked = linked; }
    // Optionen > Werkzeuge > Rechteck > "Ecken skalieren": Radien wachsen beim Skalieren mit
    static bool scaleCorners();
    static void setScaleCorners(bool on);

    QList<PropertyDescriptor> propertyDescriptors() const override;
    QVariant propertyValue(const QString& id) const override;
    bool setPropertyValue(const QString& id, const QVariant& value) override;

    void writeProperties(QJsonObject& out) const override;
    void readProperties(const QJsonObject& in) override;

private:
    double maxRadius() const { return std::min(std::abs(m_size.width()), std::abs(m_size.height())) / 2.0; }

    QSizeF m_size;
    CornerType m_cornerType = CornerType::Round;
    std::array<double, 4> m_radii{0.0, 0.0, 0.0, 0.0};
    bool m_cornersLinked = true;
};

} // namespace vek

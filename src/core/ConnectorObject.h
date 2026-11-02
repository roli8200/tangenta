#pragma once

#include "DrawObject.h"

#include <functional>

namespace vek {

// Verbindungslinie: verbindet zwei Objekte (oder freie Punkte) gerade oder
// rechtwinklig und folgt den Objekten, wenn sie verschoben, skaliert oder gedreht werden.
// Ein Ende hängt an einem Objekt über eine Stelle relativ zu dessen Grundform (0..1 je
// Richtung); das Dokument berechnet die Lage nach jeder Änderung neu (resolve).
class ConnectorObject final : public DrawObject {
public:
    enum class Kind { Straight, RightAngle, RoundedRightAngle };

    struct End {
        QPointF point;            // freies Ende bzw. zuletzt berechnete Lage (Seite, mm)
        NodeId target = 0;        // 0 = frei
        QPointF anchor{0.5, 0.5}; // relativ zur Grundform des Ziels
        friend bool operator==(const End&, const End&) = default;
    };

    ConnectorObject() = default;
    ConnectorObject(Kind kind, const End& a, const End& b);

    QString typeName() const override { return QStringLiteral("connector"); }
    QPainterPath localPath() const override;
    void applyTransform(const QTransform& m) override;

    Kind kind() const { return m_kind; }
    void setKind(Kind kind) { m_kind = kind; }
    const End& start() const { return m_a; }
    const End& end() const { return m_b; }
    void setEnds(const End& a, const End& b);
    // Mindestabstand der rechtwinkligen Linie von den Objekten (Optionen > Verbindung)
    double clearance() const { return m_clearance; }
    void setClearance(double mm) { m_clearance = mm; }
    bool isAttached() const { return m_a.target || m_b.target; }

    // Lage der verbundenen Enden neu berechnen; lookup liefert das Zielobjekt oder nullptr
    void resolve(const std::function<const DrawObject*(NodeId)>& lookup) const;
    // Anker eines Objekts in Seitenkoordinaten
    static QPointF anchorPoint(const DrawObject& target, const QPointF& anchor);
    // Anker (relativ) für eine Seitenstelle am Objekt
    static QPointF anchorFor(const DrawObject& target, const QPointF& page);

    QList<PropertyDescriptor> propertyDescriptors() const override;
    QVariant propertyValue(const QString& id) const override;
    bool setPropertyValue(const QString& id, const QVariant& value) override;

    void writeProperties(QJsonObject& out) const override;
    void readProperties(const QJsonObject& in) override;

private:
    Kind m_kind = Kind::Straight;
    End m_a;
    End m_b;
    double m_clearance = 8.0;
    // Vom Dokument berechnete Lage der Enden (Seite) und Austrittsrichtung
    mutable QPointF m_resolvedA;
    mutable QPointF m_resolvedB;
    mutable QPointF m_dirA;
    mutable QPointF m_dirB;
    mutable bool m_resolved = false;
};

} // namespace vek

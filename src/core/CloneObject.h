#pragma once

#include "DrawObject.h"

#include <QJsonObject>

#include <functional>
#include <memory>

namespace vek {

// Klon bzw. Symbolinstanz: zeigt ein anderes Objekt (das Original bzw. die
// Symboldefinition) unter einer eigenen Transformation. Ändert sich das Original, folgt der
// Klon (das Dokument löst nach jeder Änderung auf, wie bei Verbindungslinien). Fehlt das
// Original, bleibt der zuletzt gesehene Stand (wird mitgespeichert).
class CloneObject final : public DrawObject {
public:
    CloneObject() = default;
    CloneObject(NodeId master, bool symbol) : m_master(master), m_symbol(symbol) {}

    QString typeName() const override { return QStringLiteral("clone"); }
    QPainterPath localPath() const override;
    QRectF boundingRect() const override;
    bool hitTest(const QPointF& point, double tolerance, bool treatAsFilled) const override;

    NodeId master() const { return m_master; }
    bool isSymbol() const { return m_symbol; }
    // Stand des Originals übernehmen; true = hat sich geändert
    bool resolve(const std::function<const DrawObject*(NodeId)>& lookup) const;
    // Das Original in Seitenkoordinaten des Klons (mit dessen Transformation), zum Zeichnen;
    // nullptr, solange nichts aufgelöst ist
    std::shared_ptr<const DrawObject> rendered() const;
    // Eigenständige Kopie (In Objekte zurückwandeln)
    std::unique_ptr<DrawObject> detach() const;

    void writeProperties(QJsonObject& out) const override;
    void readProperties(const QJsonObject& in) override;

private:
    std::shared_ptr<DrawObject> snapshot() const;

    NodeId m_master = 0;
    bool m_symbol = false;
    mutable QJsonObject m_snapshotJson; // Original in seinen eigenen Seitenkoordinaten
    mutable std::shared_ptr<DrawObject> m_snapshot;
    mutable std::shared_ptr<const DrawObject> m_rendered;
    mutable QTransform m_renderedFor;
};

} // namespace vek

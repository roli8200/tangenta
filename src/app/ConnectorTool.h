#pragma once

#include "ConnectorObject.h"
#include "Tool.h"

#include <optional>

namespace vek {

// Verbindungswerkzeuge (gerade, rechtwinklig, rechtwinklig abgerundet): vom
// ersten Objekt zum zweiten ziehen. Die Enden rasten an Ecken, Seitenmitten und Mitte des
// Objekts ein (mit "Geometrische Ankerpunkte" auch an seinen Knoten); ausserhalb von Objekten
// bleibt ein Ende frei.
class ConnectorTool final : public Tool {
public:
    ConnectorTool(Canvas& canvas, ConnectorObject::Kind kind) : Tool(canvas), m_kind(kind) {}

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    void paintOverlay(QPainter& p) override;

private:
    // Ende an der Stelle: am nächsten Anker eines Objekts oder frei
    ConnectorObject::End endAt(const QPointF& doc, NodeId exclude) const;

    ConnectorObject::Kind m_kind;
    std::optional<ConnectorObject::End> m_start;
    ConnectorObject::End m_hover;
};

} // namespace vek

#pragma once

#include "CurveObject.h"
#include "Tool.h"

#include <QList>
#include <QPointF>

#include <functional>

#include <memory>
#include <optional>

class QMenu;

namespace vek {

// Formwerkzeug (F10): bearbeitet die Knoten einer ausgewählten Kurve.
//  - Knoten anklicken/ziehen (Umschalt ergänzt), Rahmen im Leeren wählt mehrere
//  - Steuerpunkte der Kurvensegmente an gewählten Knoten ziehen
//  - Kurvensegment ziehen verbiegt es; Linien müssen erst "In Kurve" umgewandelt werden
//  - Doppelklick auf das Segment fügt einen Knoten ein, Doppelklick auf einen Knoten löscht ihn
//  - Rechtsklick: Knotenmenü; Entf löscht die gewählten Knoten
//  - Grundformen ohne Umwandlung: Rechteckecke ziehen rundet die Ecken, Ellipsenknoten ziehen
//    ergibt Kreissegment (innen) oder Bogen (aussen), Polygonknoten ziehen bewegt alle
//    entsprechenden Knoten symmetrisch
//  - Mehrere gewählte Knoten: Rahmen zum Skalieren bzw. Drehen/Neigen (Eigenschaftsleiste)
class ShapeTool final : public Tool {
public:
    using Tool::Tool;

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    void mouseDoubleClick(QMouseEvent* event, const QPointF& doc) override;
    bool keyPress(QKeyEvent* event) override;
    void paintOverlay(QPainter& p) override;

    // Bearbeitete Kurve: genau ein ausgewähltes Kurvenobjekt. Bei einem Bild steht hier eine
    // Kurve mit seinem Rahmen; Änderungen daran beschneiden das Bild (wie).
    const CurveObject* curve() const;
    const QList<NodeRef>& selectedNodes() const { return m_selected; }
    void setSelectedNodes(const QList<NodeRef>& nodes);

    // --- Knotenbefehle (Eigenschaftsleiste und Kontextmenü) -----------------------------
    enum class Command { Add, Delete, ToLine, ToCurve, Cusp, Smooth, Symmetric, Close, Reverse };
    bool canRun(Command c) const;
    void run(Command c);
    static QString commandText(Command c);
    static QString commandIcon(Command c);
    void fillMenu(QMenu& menu);

    // Grundform (Rechteck, Ellipse, Polygon, Stern), die das Formwerkzeug direkt bearbeitet
    const DrawObject* basicShape() const;
    // Knoten der Grundform in Seitenkoordinaten (Rechteck: Beginn der Rundung je Ecke oben links,
    // oben rechts, unten rechts, unten links; Ellipse: Anfang und Ende; Polygon: Ecken und Seitenmitten)
    QList<QPointF> basicShapeNodes() const;

    // Gewählte Knoten transformieren: Rahmen mit Griffen um die Auswahl
    enum class NodeTransform { None, Scale, Rotate };
    NodeTransform nodeTransform() const { return m_nodeTransform; }
    void setNodeTransform(NodeTransform mode);
    bool canTransformNodes() const; // mindestens zwei gewählte Knoten
    void reflectNodes(Qt::Orientation orientation);
    // Rahmen der gewählten Knoten in Seitenkoordinaten
    QRectF selectedNodesBounds() const;
    // Griff des Rahmens (0..3 Ecken ab oben links im Uhrzeigersinn, 4..7 Seiten oben, rechts,
    // unten, links) in Bildschirmkoordinaten
    QPointF handlePosition(int handle) const;

private:
    enum class Drag { None, Nodes, Control, Segment, Marquee, BasicNode, Handle };
    struct ControlRef {
        NodeRef owner;
        bool incoming = false;
    };
    struct SegmentHit {
        NodeRef end;
        double t = 0.0;
    };

    QTransform toScreen() const;
    std::optional<NodeRef> nodeAt(const QPointF& screen) const;
    std::optional<ControlRef> controlAt(const QPointF& screen) const;
    std::optional<SegmentHit> segmentAt(const QPointF& screen) const;
    QList<ControlRef> visibleControls(const CurveObject& c) const;
    QPointF controlPoint(const CurveObject& c, const ControlRef& ref) const;
    void validateSelection() const;
    void beginPreview();
    void commitPreview(const QString& label);
    void notify();
    // Änderung an der bearbeiteten Kurve bzw. am Bildrahmen als ein Undo-Schritt
    void modifyTarget(const std::function<void(CurveObject&)>& fn, const QString& label);
    std::optional<int> basicNodeAt(const QPointF& screen) const;
    QList<QPointF> basicLocalNodes(const DrawObject& o) const;
    void updateBasicPreview(const QPointF& doc, Qt::KeyboardModifiers modifiers);
    void commitBasicPreview();
    std::optional<int> handleAt(const QPointF& screen) const;
    QTransform handleTransform(const QPointF& doc, Qt::KeyboardModifiers modifiers) const;
    QString handleLabel() const;
    void paintBasicOverlay(QPainter& p);

    mutable QList<NodeRef> m_selected;
    mutable NodeId m_object = InvalidNodeId;
    Drag m_drag = Drag::None;
    bool m_moved = false;
    QPointF m_pressDoc;
    QPointF m_pressScreen;
    QPointF m_currentScreen;
    ControlRef m_control;
    SegmentHit m_segment;
    std::unique_ptr<CurveObject> m_preview; // Vorschau während eines Zugs
    std::unique_ptr<DrawObject> m_basicPreview; // Vorschau beim Ziehen an einer Grundform
    NodeId m_basicId = InvalidNodeId;
    int m_basicNode = -1;
    double m_angleTravel = 0.0; // Ellipse: überstrichener Winkel seit dem Drücken (Grad)
    double m_lastAngle = 0.0;
    NodeTransform m_nodeTransform = NodeTransform::None;
    int m_handle = -1;
    QRectF m_handleBox; // Rahmen beim Drücken (Seitenkoordinaten)
    mutable std::unique_ptr<CurveObject> m_frameProxy; // Rahmen des gewählten Bildes als Kurve
    mutable NodeId m_frameProxyOf = InvalidNodeId;
};

} // namespace vek

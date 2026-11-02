#pragma once

#include "DrawObject.h"

#include <QJsonArray>
#include <QList>
#include <QPointF>

#include <memory>

namespace vek {

// Knotenart: Spitz (Steuerpunkte unabhängig), Glatt (Steuerpunkte auf
// einer Geraden durch den Knoten), Symmetrisch (zusätzlich gleich lang)
enum class NodeKind { Cusp, Smooth, Symmetric };

// Ein Knoten mit dem Segment, das vom Vorgänger zu ihm führt. Wie gewohnt ist jedes
// Segment für sich eine Linie oder eine Kurve; "In Kurve" auf einem Knoten wandelt das
// Segment davor um.
struct CurveNode {
    QPointF point;
    bool curveIn = false; // Segment vom Vorgänger her ist eine kubische Bézierkurve
    QPointF c1{};         // Steuerpunkt am Vorgänger (nur curveIn)
    QPointF c2{};         // Steuerpunkt an diesem Knoten (nur curveIn)
    NodeKind kind = NodeKind::Cusp;

    friend bool operator==(const CurveNode&, const CurveNode&) = default;
};

// Zusammenhängender Linienzug. Beim ersten Knoten gehört das Eingangssegment zum
// Schliessen (nur wenn closed).
struct SubPath {
    QList<CurveNode> nodes;
    bool closed = false;

    friend bool operator==(const SubPath&, const SubPath&) = default;
};

// Adresse eines Knotens: Linienzug und Knoten darin
struct NodeRef {
    int path = -1;
    int node = -1;

    bool isValid() const { return path >= 0 && node >= 0; }
    friend bool operator==(const NodeRef&, const NodeRef&) = default;
};

// Künstlerische Medien: die Kurve ist nur das Rückgrat, sichtbar ist ein
// gefüllter Strich mit veränderlicher Breite entlang dieses Rückgrats
struct ArtisticMedia {
    enum class Kind { None, Preset, Calligraphy, Pressure, Sprayer };
    Kind kind = Kind::None;
    int preset = 0;       // Breitenverlauf (Voreinstellung)
    double width = 10.0;  // mm (Sprühdose: Grösse der Objekte)
    double angle = 0.0;   // Federwinkel in Grad gegen den Uhrzeigersinn (Kalligrafie)
    // Druck: Breite entlang des Strichs 0..1, gleichmässig verteilt (leer = überall voll)
    QList<double> pressure{};
    // Sprühdose: Sprühliste, Abstand der Objekte (mm), zufällig drehen/mischen, Zufallszahl
    int sprayList = 0;
    double spacing = 8.0;
    bool randomRotate = true;
    bool randomOrder = true;
    quint32 seed = 1;

    bool isActive() const { return kind != Kind::None; }
    friend bool operator==(const ArtisticMedia&, const ArtisticMedia&) = default;
};

// Kurvenobjekt: alles, was mit dem Formwerkzeug Knoten für Knoten bearbeitet wird.
// Rechtecke und andere Grundformen werden mit "In Kurven umwandeln" zu einem CurveObject.
// Knoten liegen in lokalen Koordinaten; die Transformation des DrawObject gilt wie sonst.
class CurveObject final : public DrawObject {
public:
    CurveObject() = default;
    explicit CurveObject(QList<SubPath> paths) : m_paths(std::move(paths)) {}
    // Übernimmt Linien und kubische Kurven eines QPainterPath (lokale Koordinaten)
    static std::unique_ptr<CurveObject> fromPath(const QPainterPath& path);

    QString typeName() const override { return QStringLiteral("curve"); }
    // Mit künstlerischen Medien der Umriss des Strichs, sonst das Rückgrat
    QPainterPath localPath() const override;
    // Die Knoten als Pfad, unabhängig von künstlerischen Medien
    QPainterPath spinePath() const;

    const ArtisticMedia& media() const { return m_media; }
    void setMedia(const ArtisticMedia& media) { m_media = media; }
    QList<PropertyDescriptor> propertyDescriptors() const override;
    QVariant propertyValue(const QString& id) const override;
    bool setPropertyValue(const QString& id, const QVariant& value) override;

    const QList<SubPath>& subPaths() const { return m_paths; }
    void setSubPaths(const QList<SubPath>& paths) { m_paths = paths; }
    int nodeCount() const;
    const CurveNode& node(NodeRef r) const { return m_paths[r.path].nodes[r.node]; }
    bool isValid(NodeRef r) const;

    // Segment, das in Knoten r führt (fehlt beim ersten Knoten eines offenen Linienzugs)
    bool hasSegmentInto(NodeRef r) const;
    NodeRef previous(NodeRef r) const;
    NodeRef next(NodeRef r) const;
    bool isEndNode(NodeRef r) const; // Anfang oder Ende eines offenen Linienzugs
    // Punkt auf dem Segment in Knoten r bei t (0 = Vorgänger, 1 = r)
    QPointF pointOnSegment(NodeRef r, double t) const;
    // Nächster Punkt auf dem Segment in r: liefert t, Abstand in *distance
    double nearestOnSegment(NodeRef r, const QPointF& p, double* distance) const;

    // --- Bearbeitung (lokale Koordinaten) ------------------------------------------------
    // Knoten samt ihrer Steuerpunkte verschieben
    void moveNodes(const QList<NodeRef>& nodes, const QPointF& delta);
    // Steuerpunkt setzen: incoming = c2 des Segments in r, sonst c1 des Segments aus r heraus.
    // Glatte und symmetrische Knoten führen den Gegenpunkt mit.
    void moveControl(NodeRef r, bool incoming, const QPointF& pos);
    // Segment in r: Linie <-> Kurve. Neue Steuerpunkte liegen bei 1/3 und 2/3 der Strecke.
    void setSegmentCurve(NodeRef r, bool curve);
    void setKind(NodeRef r, NodeKind kind);
    // Segment in r bei t teilen; liefert den neuen Knoten (die Form bleibt gleich)
    NodeRef insertNode(NodeRef r, double t);
    // Knoten löschen; die beiden Segmente werden zu einem. Ein Linienzug mit weniger als
    // zwei Knoten verschwindet.
    void deleteNodes(QList<NodeRef> nodes);
    // Segment in Knoten r so verbiegen, dass der Kurvenpunkt bei t um delta wandert
    void bendSegment(NodeRef r, double t, const QPointF& delta);
    void setClosed(int path, bool closed);
    void reverse(int path);
    // Neuen Knoten an ein Ende eines offenen Linienzugs anhängen (atStart: davor)
    NodeRef appendNode(int path, bool atStart, const QPointF& point);

    void writeProperties(QJsonObject& out) const override;
    void readProperties(const QJsonObject& in) override;

    // Linienzüge als Pfad bzw. JSON; auch für andere Objekte mit Knotenrahmen (Bilder)
    static QPainterPath toPainterPath(const QList<SubPath>& paths);
    static QJsonArray pathsToJson(const QList<SubPath>& paths);
    static QList<SubPath> pathsFromJson(const QJsonArray& json);

private:
    QPointF controlOut(NodeRef r) const;
    void alignOpposite(NodeRef r, bool movedIncoming);

    QList<SubPath> m_paths;
    ArtisticMedia m_media;
};

} // namespace vek

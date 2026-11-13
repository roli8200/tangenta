#pragma once

#include "CurveObject.h"

#include <QList>
#include <QPainterPath>
#include <QPointF>

#include <memory>

namespace vek::pathops {

// Planares Netz aus beliebigen Linienzügen (offen und geschlossen, Seitenkoordinaten) für die
// Smart-Füllung: Schnittpunkte teilen die Linien in Kanten, lose Enden zählen nicht, Lücken bis
// gap (mm) zwischen einem offenen Ende und einer anderen Linie werden überbrückt. Die Kanten
// bleiben Bézierstücke der ursprünglichen Linienzüge, die Ergebnisse also glatt.
class Arrangement {
public:
    explicit Arrangement(const QList<SubPath>& paths, double gap = 0.1);
    ~Arrangement();
    Arrangement(Arrangement&&) noexcept;
    Arrangement& operator=(Arrangement&&) noexcept;

    // Kleinste umschlossene Masche um point als geschlossene Linienzüge: zuerst der Rand, danach
    // die Löcher (Inseln darin, gegenläufig). Leer, wenn point in keiner Masche liegt.
    QList<SubPath> faceAt(const QPointF& point) const;
    QPainterPath facePathAt(const QPointF& point) const;
    // Zahl der umschlossenen Maschen (für Tests)
    int faceCount() const;
    int edgeCount() const;

private:
    struct Data;
    std::unique_ptr<Data> d;
};

} // namespace vek::pathops

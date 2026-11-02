#pragma once

#include "Ramp.h"
#include "Style.h"

#include <QList>
#include <QPainterPath>
#include <QTransform>

#include <memory>

namespace vek {

class GroupObject;

// Unschärfe dauerhaft machen ("In Kurven" eines unscharfen Objekts): Jede Fläche wird zu
// einem Stapel aus Stufenkonturen, jede etwas grösser bzw. kleiner als die Fläche und mit
// abgestufter Transparenz. Übereinander ergeben sie den weichen Rand des Gaussschen
// Weichzeichners, bleiben aber reine Vektoren.
//
// Ein Unschärfe-Verlauf wird in Bänder zerlegt (Streifen, Ringe, Sektoren), jedes mit der
// Unschärfe seiner Mitte.
namespace bake {

// Eine Fläche mit ihrer Füllung, in Seitenkoordinaten
struct Part {
    QPainterPath area;
    Fill fill;
};

// Konturen je Unschärfe (Vorgabe, einstellbar) und Bänder eines Verlaufs. Mit wenigen Stufen
// sieht man die Ringe von Auge.
constexpr int kDefaultSteps = 48;
constexpr int kDefaultBands = 32;

// Deckkraft der Gausskante bei Abstand d vom Rand (positiv = aussen)
double edgeAlpha(double distance, double sigma);

// Gruppe aus Stufenkonturen; blur in mm, Verlauf relativ zu localBounds unter toPage.
// nullptr, wenn keine Fläche übrig bleibt.
std::unique_ptr<GroupObject> blurToContours(const QList<Part>& parts, const Ramp& blur, const QTransform& toPage,
                                            const QRectF& localBounds, int steps = kDefaultSteps,
                                            int bands = kDefaultBands);

} // namespace bake

} // namespace vek

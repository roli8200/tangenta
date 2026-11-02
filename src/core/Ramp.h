#pragma once

#include "Patterns.h"

#include <QJsonValue>
#include <QPointF>
#include <QRectF>

namespace vek {

// Effekt, der über ein Objekt verlaufen kann
enum class Effect { Transparency, Blur };

// Verlauf eines Effekts über ein Objekt wie bei der interaktiven Transparenz:
// gleichmässig oder linear/radial/konisch von start nach end. start/end liegen wie bei Fill
// relativ zum lokalen Begrenzungsrechteck der Grundform (0..1), damit der Verlauf mit dem
// Objekt mitgeht.
//  - Transparenz: Werte in Prozent (0 = deckend, 100 = unsichtbar)
//  - Unschärfe:   Werte in mm (Standardabweichung des Gaussschen Weichzeichners)
struct Ramp {
    enum class Type { None, Uniform, Linear, Radial, Conical, Pattern };

    Type type = Type::None;
    double from = 0.0; // am Anfang bzw. gleichmässig
    double to = 0.0;   // am Ende
    QPointF start{0.0, 0.5};
    QPointF end{1.0, 0.5};
    // Mittelpunkt: an dieser Stelle (0..1 zwischen start und end) ist der
    // Verlauf halb durch; davor und danach gleichmässig
    double midpoint = 0.5;
    // Mustertransparenz: Vordergrund des Musters bekommt from, Hintergrund to
    PatternFill pattern;

    static Ramp uniform(double value);

    bool isGradient() const { return type == Type::Linear || type == Type::Radial || type == Type::Conical; }
    // Hat überhaupt eine Wirkung
    bool isActive() const { return maximum() > 0.0; }
    double minimum() const;
    double maximum() const;
    // Verlaufsparameter 0..1 an einem lokalen Punkt
    double parameterAt(const QPointF& local, const QRectF& localBounds) const;
    double valueAt(const QPointF& local, const QRectF& localBounds) const;

    QJsonValue toJson() const;
    static Ramp fromJson(const QJsonValue& json);

    friend bool operator==(const Ramp&, const Ramp&) = default;
};

} // namespace vek

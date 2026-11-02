#pragma once

#include "Color.h"
#include "Patterns.h"

#include <QBrush>
#include <QImage>
#include <QJsonObject>
#include <QList>
#include <QPointF>
#include <QRectF>

#include <array>
#include <optional>

namespace vek {

struct GradientStop {
    double position = 0.0; // 0..1
    Color color;
    friend bool operator==(const GradientStop&, const GradientStop&) = default;
};

// Verlaufsgeometrie in den Begriffen (Dialog "Farbverlauf")
struct GradientGeometry {
    double angle = 0.0;   // Grad gegen den Uhrzeigersinn, 0 = von links nach rechts
    double edgePad = 0.0; // Randauffüllung in %, 0..49
    double offsetX = 0.0; // Mittelpunktverschiebung in % der halben Breite
    double offsetY = 0.0; // dito, positiv = nach oben
};

// Maschenfüllung: Gitter aus (rows+1) x (columns+1) Knoten, jeder mit eigener
// Farbe und Lage. Lagen relativ zum lokalen Begrenzungsrechteck (0..1), zeilenweise von oben
// links. Zwischen den Knoten werden Lage und Farbe je Masche bilinear übergeblendet.
struct MeshFill {
    int columns = 2;
    int rows = 2;
    QList<QPointF> points;
    QList<Color> colors;

    static MeshFill grid(int columns, int rows, const Color& color);
    bool isValid() const;
    int nodeCount() const { return (rows + 1) * (columns + 1); }
    int index(int row, int column) const { return row * (columns + 1) + column; }
    // Masche (Zeile, Spalte) als Viereck in Einheitskoordinaten: oben links, oben rechts,
    // unten rechts, unten links
    std::array<QPointF, 4> patch(int row, int column) const;
    // Gerastert im Einheitsquadrat (Bild deckt 0..1 x 0..1); ausserhalb des Netzes durchsichtig
    QImage render(const QSize& size) const;

    QJsonObject toJson() const;
    static MeshFill fromJson(const QJsonObject& json);

    friend bool operator==(const MeshFill&, const MeshFill&) = default;
};

// Füllung eines Objekts: keine, gleichmässig, Farbverlauf oder Maschenfüllung.
//
// Die Verlaufsachse (start/end) liegt relativ zum lokalen Begrenzungsrechteck der Grundform
// (0..1 je Richtung). Damit dreht, skaliert und neigt sich der Verlauf mit dem Objekt.
//  - linear:  von start nach end
//  - radial:  Mittelpunkt start, Radius |end - start|
//  - konisch: Mittelpunkt start, Startrichtung end - start
struct Fill {
    enum class Type { None, Solid, Linear, Radial, Conical, Mesh, Pattern };

    Type type = Type::None;
    Color color;              // nur Solid
    QList<GradientStop> stops; // nur Verläufe, nach position sortiert
    QPointF start{0.0, 0.5};
    QPointF end{1.0, 0.5};
    MeshFill mesh; // nur Mesh
    PatternFill pattern; // nur Pattern (zweifarbig, Bitmap, Textur)

    static Fill none() { return {}; }
    static Fill meshFill(const MeshFill& m);
    static Fill solid(const Color& c);
    static Fill gradient(Type type, const Color& from, const Color& to);

    bool isNone() const { return type == Type::None; }
    bool isGradient() const { return type == Type::Linear || type == Type::Radial || type == Type::Conical; }
    bool isMesh() const { return type == Type::Mesh; }
    bool isPattern() const { return type == Type::Pattern; }
    static Fill patternFill(const PatternFill& p);
    // Farbe für Anzeige und Statusleiste: gleichmässige Farbe bzw. erste Verlaufsfarbe
    std::optional<Color> representativeColor() const;

    // Pinsel in lokalen Koordinaten; localBounds = Begrenzungsrechteck der Grundform. Eine
    // Maschenfüllung kommt als Bildpinsel mit eigener Transformation: Aufrufer setzen ihre
    // Abbildung deshalb mit brush.setTransform(brush.transform() * aufGerät).
    QBrush brush(const QRectF& localBounds) const;

    GradientGeometry geometry(const QSizeF& localSize) const;
    void setGeometry(const GradientGeometry& g, const QSizeF& localSize);
    void sortStops();

    QJsonValue toJson() const;
    static Fill fromJson(const QJsonValue& json);

    friend bool operator==(const Fill&, const Fill&) = default;
};

// Füllung und Kontur eines Objekts. Kein Wert bei stroke = keine Kontur.
struct Style {
    // Konturstift (F12)
    enum class Join { Miter, Round, Bevel };              // Ecken
    enum class Cap { Flat, Round, Square };               // Linienenden
    enum class Position { Center, Inside, Outside };      // Lage der Kontur zur Form

    Fill fill;
    std::optional<Color> stroke = Color(0, 0, 0);
    double strokeWidth = 0.2; // mm; mitskaliert nur mit scaleWithObject
    int dash = 0;             // Linienstil (outline::dashPattern), 0 = durchgehend
    int startArrow = 0;       // Pfeilspitzen (outline::arrowShape), 0 = keine
    int endArrow = 0;
    Join join = Join::Miter;
    Cap cap = Cap::Flat;
    Position position = Position::Center;
    bool behindFill = false;      // Kontur hinter der Füllung (nur die äussere Hälfte sichtbar)
    bool scaleWithObject = false; // Konturbreite wächst beim Skalieren mit
    double nibStretch = 100.0;    // Kalligrafie: Breite der Spitze in % (100 = rund)
    double nibAngle = 0.0;        // Kalligrafie: Winkel der Spitze in Grad gegen den Uhrzeigersinn

    QJsonObject toJson() const;
    static Style fromJson(const QJsonObject& json);

    friend bool operator==(const Style&, const Style&) = default;
};

} // namespace vek

#pragma once

#include "Color.h"

#include <QJsonObject>
#include <QList>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QSizeF>

#include <functional>

namespace vek {

// Pfad punktweise verformen (Hülle, Fischauge): Kurven und lange Strecken werden zuerst so fein
// unterteilt, dass kein Stück länger als maxStep ist; das Ergebnis besteht aus Strecken
QPainterPath warpPath(const QPainterPath& path, const std::function<QPointF(const QPointF&)>& fn, double maxStep);

// Linse (Effekte > Linse, Alt+F3): das Objekt verändert, was darunter liegt,
// statt selbst gefüllt zu werden.
struct Lens {
    enum class Type { None, Brighten, ColorAdd, Invert, Magnify, Transparency, TintedGrayscale,
                      FishEye, ColorLimit, CustomColorMap, HeatMap, Wireframe };
    Type type = Type::None;
    // Aufhellen/Transparenz/Farbe addieren/Farbgrenze in %, Vergrössern als Faktor ×10,
    // Fischauge Rate in % (-1000..1000), Wärmebild Palettendrehung in %
    double amount = 50.0;
    Color color{255, 0, 0};    // Farbe addieren, Tönung, Transparenz, Farbgrenze; Farbzuordnung: von; Drahtmodell: Kontur
    Color color2{255, 255, 0}; // Farbzuordnung: nach; Drahtmodell: Füllung
    // Blickpunkt: die Linse zeigt, was um (Linsenmitte + viewOffset) liegt (mm)
    bool viewpoint = false;
    QPointF viewOffset{0.0, 0.0};

    bool isActive() const { return type != Type::None; }
    // Wirkt die Linse auf Farben (sonst auf die Lage: Vergrössern, Fischauge, Drahtmodell)?
    bool changesColor() const;
    // Farbe eines vormultiplizierten Bildpunkts (a = Deckkraft 0..255) durch die Linse
    void filter(int& r, int& g, int& b, int a) const;
    Color mapColor(const Color& c) const;
    // Fischauge: Punkt im Bild -> Punkt in der Vorlage, um center mit den Halbachsen radius;
    // inverse = umgekehrt (Vorlage -> Bild, für "Fixiert")
    QPointF fishEye(const QPointF& p, const QPointF& center, const QSizeF& radius, bool inverse) const;
    QJsonObject toJson() const;
    static Lens fromJson(const QJsonObject& json);
    friend bool operator==(const Lens&, const Lens&) = default;
};

// Abschrägung (Effekte > Abschrägung): weiche Kante bzw. Relief aus einer Lichtquelle
struct Bevel {
    enum class Style { None, Soft, Emboss };
    Style style = Style::None;
    double width = 2.0;       // mm
    double intensity = 60.0;  // %
    double direction = 135.0; // Lichtrichtung in Grad (0 = von rechts, gegen den Uhrzeigersinn)
    double altitude = 45.0;   // Lichthöhe in Grad
    Color light{255, 255, 255};
    Color shadow{0, 0, 0};

    bool isActive() const { return style != Style::None && width > 0; }
    QJsonObject toJson() const;
    static Bevel fromJson(const QJsonObject& json);
    friend bool operator==(const Bevel&, const Bevel&) = default;
};

// Hinterlegter Schatten (Hilfsmittel 'Hinterlegter Schatten'): flach mit Versatz oder
// perspektivisch von der Unter- bzw. Oberkante aus (Winkel, Streckung, Ausblenden)
struct DropShadow {
    bool active = false;
    bool perspective = false;
    bool fromTop = false;      // perspektivisch: an der Oberkante statt an der Unterkante
    QPointF offset{2.0, 2.0};  // flach: Versatz in mm (y nach unten)
    double angle = 45.0;       // perspektivisch: Neigung gegen die Senkrechte in Grad
    double stretch = 50.0;     // perspektivisch: Länge in % der Objekthöhe
    double fade = 0.0;         // perspektivisch: Ausblenden zum Ende hin in %
    double opacity = 50.0;     // Deckung in %
    double feather = 1.5;      // Federung in mm
    Color color{0, 0, 0};

    QJsonObject toJson() const;
    static DropShadow fromJson(const QJsonObject& json);
    friend bool operator==(const DropShadow&, const DropShadow&) = default;
};

// Kontur (Hilfsmittel 'Kontur'): Stufen nach innen, aussen oder zur Mitte mit gleichem Abstand,
// Farben vom Objekt zur Endfarbe
struct ContourEffect {
    enum class Direction { Center, Inside, Outside };
    bool active = false;
    Direction direction = Direction::Outside;
    int steps = 4;
    double offset = 2.0; // mm je Stufe
    Color endFill{255, 255, 255};
    Color endStroke{0, 0, 0};

    QJsonObject toJson() const;
    static ContourEffect fromJson(const QJsonObject& json);
    friend bool operator==(const ContourEffect&, const ContourEffect&) = default;
};

// Extrusion (Hilfsmittel 'Extrusion'): Seitenflächen zum Fluchtpunkt hin
struct Extrusion {
    bool active = false;
    QPointF vanish{20.0, -20.0}; // Fluchtpunkt relativ zur Objektmitte (mm)
    double depth = 30.0;         // Tiefe in % der Strecke zum Fluchtpunkt
    bool parallel = false;       // parallel statt perspektivisch
    Color color{128, 128, 128};  // Farbe der Seitenflächen
    bool shade = true;           // je nach Lage heller/dunkler

    QJsonObject toJson() const;
    static Extrusion fromJson(const QJsonObject& json);
    friend bool operator==(const Extrusion&, const Extrusion&) = default;
};

// Hülle (Effekte > Hülle, Strg+F7): das Objekt wird in eine verformbare
// Hüllform eingepasst und bleibt dabei, was es ist (Text bleibt bearbeitbar). Die Ränder sind
// Linienzüge mit gleich vielen Punkten in lokalen Koordinaten; box ist das Rechteck der Form,
// als die Hülle angelegt wurde. Abgebildet wird wie bei einer Coons-Fläche.
struct Envelope {
    enum class Mode { Straight, SingleArc, DoubleArc, Unconstrained };
    static constexpr int kSamples = 65; // Punkte je Rand, Mitte bei kSamples / 2
    Mode mode = Mode::Unconstrained;
    QRectF box;
    QList<QPointF> top, right, bottom, left; // oben/unten von links nach rechts, links/rechts von oben nach unten

    bool isActive() const
    {
        return box.width() > 0 && box.height() > 0 && top.size() == kSamples && right.size() == kSamples
               && bottom.size() == kSamples && left.size() == kSamples;
    }
    // Unverformte Hülle um box
    static Envelope rectangle(const QRectF& box, Mode mode = Mode::Unconstrained);
    // Hülle aus einer geschlossenen Form (Vorlage, "Form aus Objekt"), in box eingepasst; die
    // Form wird an den Punkten geteilt, die den Ecken ihres Rechtecks am nächsten liegen
    static Envelope fromOutline(const QRectF& box, const QPainterPath& outline, Mode mode = Mode::Unconstrained);

    QPointF map(const QPointF& p) const;
    QPainterPath map(const QPainterPath& path) const;
    // Umriss der Hülle (geschlossen)
    QPainterPath outline() const;
    // 0 oben links, 1 oben Mitte, 2 oben rechts, 3 rechts Mitte, 4 unten rechts, 5 unten Mitte,
    // 6 unten links, 7 links Mitte
    QList<QPointF> handles() const;
    // Griff verschieben; die Ränder folgen je nach Modus gerade, in einem oder zwei Bögen
    void moveHandle(int handle, const QPointF& to);
    // Ganze Hülle abbilden (Objekt wird transformiert, Hülle bleibt in lokalen Koordinaten nötig)
    void transform(const QTransform& m);

    QJsonObject toJson() const;
    static Envelope fromJson(const QJsonObject& json);
    friend bool operator==(const Envelope&, const Envelope&) = default;
};

} // namespace vek

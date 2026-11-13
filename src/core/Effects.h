#pragma once

#include "Blend.h"
#include "Color.h"

#include <QJsonObject>
#include <QList>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QTransform>

#include <QImage>

#include <functional>
#include <utility>

namespace vek {

// Pfad punktweise verformen (Hülle, Fischauge): Kurven und lange Strecken werden zuerst so fein
// unterteilt, dass kein Stück länger als maxStep ist; das Ergebnis besteht aus Strecken
QPainterPath warpPath(const QPainterPath& path, const std::function<QPointF(const QPointF&)>& fn, double maxStep);

// Linse (Effekte > Linse, Alt+F3): das Objekt verändert, was darunter liegt,
// statt selbst gefüllt zu werden.
struct Lens {
    enum class Type { None, Brighten, ColorAdd, Invert, Magnify, Transparency, TintedGrayscale,
                      FishEye, ColorLimit, CustomColorMap, HeatMap, Wireframe, BitmapEffect };
    Type type = Type::None;
    // Aufhellen/Transparenz/Farbe addieren/Farbgrenze in %, Vergrössern als Faktor ×10,
    // Fischauge Rate in % (-1000..1000), Wärmebild Palettendrehung in %
    double amount = 50.0;
    Color color{255, 0, 0};    // Farbe addieren, Tönung, Transparenz, Farbgrenze; Farbzuordnung: von; Drahtmodell: Kontur
    Color color2{255, 255, 0}; // Farbzuordnung: nach; Drahtmodell: Füllung
    // Bitmap-Effekt: Kennung des Filters (bitmapEffects()), Stärke in amount (0..100 %)
    QString bitmapEffect = QStringLiteral("blur");
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
    // Bitmap-Effekt: Kennungen und (unübersetzte) Namen der Filter, die eine Linse anwenden kann
    static QList<std::pair<QString, const char*>> bitmapEffects();
    // Bitmap-Effekt auf das Bild unter der Linse; pxPerMm = Auflösung des Bildes (Radien in mm)
    QImage applyBitmapEffect(const QImage& below, double pxPerMm) const;
    QJsonObject toJson() const;
    static Lens fromJson(const QJsonObject& json);
    friend bool operator==(const Lens&, const Lens&) = default;
};

// Abschrägung (Effekte > Abschrägung): weiche Kante bzw. Relief aus einer Lichtquelle
struct Bevel {
    enum class Style { None, Soft, Emboss };
    Style style = Style::None;
    double width = 2.0;       // mm
    bool toCenter = false;    // bis zur Mitte (Pyramide) statt fester Breite
    double intensity = 60.0;  // %
    double direction = 135.0; // Lichtrichtung in Grad (0 = von rechts, gegen den Uhrzeigersinn)
    double altitude = 45.0;   // Lichthöhe in Grad
    Color light{255, 255, 255};
    Color shadow{0, 0, 0};

    bool isActive() const { return style != Style::None && (width > 0 || toCenter); }
    QJsonObject toJson() const;
    static Bevel fromJson(const QJsonObject& json);
    friend bool operator==(const Bevel&, const Bevel&) = default;
};

// Abschrägung bis zur Mitte: euklidischer Abstand jedes Bildpunkts zum Rand der Maske
// (Grauwert >= 128 = innen) in Pixeln, zeilenweise; maxDistance = grösster Abstand
QList<float> distanceToEdge(const QImage& mask, float* maxDistance = nullptr);

// Blockschatten (Hilfsmittel 'Blockschatten'): massiver, flacher Schatten = Fläche des Objekts,
// entlang offset geschoben (Minkowski-Summe mit der Strecke). Ein Vektorpfad, folgt dem Objekt.
struct BlockShadow {
    bool active = false;
    QPointF offset{5.0, 5.0}; // Richtung und Länge in mm (y nach unten)
    Color color{0, 0, 0};
    double expand = 0.0;      // Schatten um so viele mm vergrössern
    bool simplify = false;    // Vereinfachen: den Teil unter dem Objekt weglassen
    bool removeHoles = false; // Löcher füllen
    bool useOutline = true;   // Kontur des Objekts gehört zur Fläche

    double depth() const;     // Länge in mm
    double angle() const;     // Richtung in Grad (0 = nach rechts, gegen den Uhrzeigersinn)
    void setPolar(double depthMm, double angleDeg);
    QJsonObject toJson() const;
    static BlockShadow fromJson(const QJsonObject& json);
    friend bool operator==(const BlockShadow&, const BlockShadow&) = default;
};

// Fläche area entlang offset geschoben: area ∪ (area + offset) ∪ die von jeder Kante überstrichenen
// Vierecke. Kurven werden mit flatness mm Genauigkeit in Strecken zerlegt.
QPainterPath sweepArea(const QPainterPath& area, const QPointF& offset, double flatness = 0.05);
// Blockschatten einer Fläche (Seitenkoordinaten) samt Vergrössern, Löcher füllen, Vereinfachen
QPainterPath blockShadowPath(const QPainterPath& area, const BlockShadow& s);

// Hinterlegter Schatten (Hilfsmittel 'Hinterlegter Schatten'): flach mit Versatz oder
// perspektivisch von der Unter- bzw. Oberkante aus (Winkel, Streckung, Ausblenden).
// Innenschatten (inner): liegt im Objekt über dessen Füllung, fällt von den Kanten nach innen;
// der Versatz verschiebt ihn (positiv x/y: Schatten an der oberen/linken Innenkante).
struct DropShadow {
    bool active = false;
    bool inner = false;
    bool perspective = false;
    bool fromTop = false;      // perspektivisch: an der Oberkante statt an der Unterkante
    QPointF offset{2.0, 2.0};  // flach: Versatz in mm (y nach unten)
    double angle = 45.0;       // perspektivisch: Neigung gegen die Senkrechte in Grad
    double stretch = 50.0;     // perspektivisch: Länge in % der Objekthöhe
    double fade = 0.0;         // perspektivisch: Ausblenden zum Ende hin in %
    double opacity = 50.0;     // Deckung in %
    double feather = 1.5;      // Federung in mm
    Color color{0, 0, 0};
    // Mischmodus; neue Schatten multiplizieren, ältere Dateien ohne Angabe bleiben Normal
    BlendMode blend = BlendMode::Multiply;

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
    // Drehung des Körpers im Raum (Grad) um die Objektmitte: x kippt nach hinten/vorn, y dreht
    // nach links/rechts, z in der Ebene; Reihenfolge x, dann y, dann z
    double rotX = 0.0;
    double rotY = 0.0;
    double rotZ = 0.0;

    bool isRotated() const { return rotX != 0.0 || rotY != 0.0 || rotZ != 0.0; }
    // Räumliches Modell: die Vorderfläche liegt in der Ebene z = 0 (Seite, mm), der Körper reicht
    // bodyDepth() mm in den Bildschirm hinein. Mit Fluchtpunkt perspektivisch so, dass die
    // Rückseite ohne Drehung genau depth % des Wegs zum Fluchtpunkt liegt; parallel schräg.
    double bodyDepth() const;
    // Drehmatrix (Zeilen), angewendet auf (x, y, z) relativ zur Objektmitte
    void rotation(double r[3][3]) const;
    // Punkt p der Objektebene in Tiefe t (0 = Vorderfläche, 1 = Rückseite), gedreht um center
    // und projiziert; depth = Tiefe nach der Drehung (grösser = weiter hinten)
    QPointF project(const QPointF& center, const QPointF& p, double t, double* depth = nullptr) const;
    // Abbildung der Vorderfläche (Seite -> Seite): projektiv bei Fluchtpunkt, ohne Drehung Identität
    QTransform faceTransform(const QPointF& center) const;

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
    // Zuordnung: wie das Objekt in die Hülle abgebildet wird. Standard = Coons-Fläche aus allen
    // vier Rändern; Original = nur obere und untere Kante; Kitt = weicher (geglättete
    // Überblendung der Ränder); Horizontal = Zeilen bleiben gerade, nur linke/rechte Kante formen;
    // Vertikal = Spalten bleiben gerade, nur obere/untere Kante formen
    enum class Mapping { Standard, Original, Putty, Horizontal, Vertical };
    static constexpr int kSamples = 65; // Punkte je Rand, Mitte bei kSamples / 2
    Mode mode = Mode::Unconstrained;
    Mapping mapping = Mapping::Standard;
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

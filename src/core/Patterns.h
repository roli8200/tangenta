#pragma once

#include "Color.h"

#include <QImage>
#include <QJsonObject>
#include <QPointF>
#include <QString>

namespace vek {

// Muster: zweifarbige Muster (eingebaut), Bitmap-Muster (Bild aus dem
// Bildpool) und Texturen (berechnet, nahtlos kachelbar). Eine Kachel ist tile mm gross.
struct PatternFill {
    enum class Kind { TwoColor, Bitmap, Texture };
    Kind kind = Kind::TwoColor;
    int index = 0;              // zweifarbiges Muster bzw. Textur
    Color fg{0, 0, 0};          // Vordergrund (Textur: erste Farbe)
    Color bg{255, 255, 255};    // Hintergrund (Textur: zweite Farbe)
    QString imageKey;           // Bitmap-Muster
    quint32 seed = 1;           // Textur: Zufallszahl ("Neu würfeln")
    double tile = 10.0;         // Kachelgrösse in mm

    // Kachel als Bild (zwischengespeichert); null bei fehlendem Bitmap
    QImage image() const;
    // Helligkeit 0..1 der Kachel an einer Stelle (Kachelkoordinaten in mm, wiederholt):
    // zweifarbig 0 = Vordergrund, 1 = Hintergrund (für Mustertransparenz)
    double lightness(const QPointF& mm) const;

    QJsonObject toJson() const;
    static PatternFill fromJson(const QJsonObject& json);
    friend bool operator==(const PatternFill&, const PatternFill&) = default;
};

namespace patterns {

int twoColorCount();
QString twoColorName(int index);
int textureCount();
QString textureName(int index);

} // namespace patterns

} // namespace vek

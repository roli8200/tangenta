#pragma once

#include <QByteArray>
#include <QList>
#include <QPainterPath>
#include <QString>

#include <optional>

namespace vek::ttf {

// Eine Glyphe in Schrifteinheiten, y nach oben, Grundlinie bei 0
struct Glyph {
    char32_t code = 0;
    QPainterPath outline;
    int advance = 0;
};

struct Font {
    QString family;
    int unitsPerEm = 1000;
    int ascent = 800;
    int descent = 200; // positiv, unter der Grundlinie
    QList<Glyph> glyphs;
};

// TrueType-Datei schreiben (glyf-Umrisse, quadratisch angenähert; Aussenkonturen im Uhrzeigersinn,
// Löcher dagegen). Fehlt das Leerzeichen, wird eines ergänzt; Glyphe 0 ist .notdef.
QByteArray write(const Font& font);
// TrueType-Datei lesen (einfache Glyphen; zusammengesetzte bleiben leer) – um einer mit TANGENTA
// erzeugten Schrift weitere Zeichen hinzuzufügen
std::optional<Font> read(const QByteArray& data, QString* error = nullptr);

// Umriss in mm (y nach unten) als Glyphe: Unterkante bei bottom, Höhe height (Schrifteinheiten),
// links und rechts je bearing Abstand; überlappende Teile werden vereinigt
Glyph glyphFromOutline(const QPainterPath& mm, char32_t code, int height, int bottom = 0, int bearing = 50);

} // namespace vek::ttf

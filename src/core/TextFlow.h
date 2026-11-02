#pragma once

#include "Color.h"

#include <QGlyphRun>
#include <QList>
#include <QPainterPath>
#include <QSizeF>

#include <memory>
#include <optional>
#include <vector>

class QTextDocument;
class QTextLayout;

namespace vek {

// Pfad einer Farbe; color leer = Objektfüllung
struct TextPiece {
    QPainterPath path;
    std::optional<Color> color;
    // Für echte Schrift im PDF: die Glyphenläufe (Layout-Einheiten, TextObject::kUnitsPerMm je mm)
    // und was keine Schrift ist (Aufzählungszeichen, Initiale, Unterstreichung) als Pfad in mm
    QList<QGlyphRun> runs;
    QPainterPath extras;
};

// Ein einzelnes Zeichen (Text an Pfad): Umriss an seiner Stelle, Mitte und Grundlinie (mm)
struct TextGlyph {
    QPainterPath path;
    std::optional<Color> color;
    double center = 0.0;
    double baseline = 0.0;
};

// Setzt den Inhalt eines QTextDocument Zeile für Zeile in eine Folge von Rahmen, wie
// verketteter Mengentext: Was im ersten Rahmen keinen Platz mehr hat, fliesst oben
// im nächsten weiter, auch wenn die Rahmen verschieden breit sind. Zeilen, die in keinen
// Rahmen mehr passen, sind Überlauf und werden nicht gezeichnet.
//
// Ohne Umbruch (Grafiktext) gibt es genau einen Rahmen ohne Grenzen; die Zeilen richten sich
// an der breitesten aus.
//
// Ein- und Ausgaben in mm, jeweils lokal zum Rahmen (0,0 = links oben).
class TextFlow {
public:
    TextFlow(const QTextDocument& doc, const QList<QSizeF>& frames, bool wrap);
    ~TextFlow();
    TextFlow(const TextFlow&) = delete;
    TextFlow& operator=(const TextFlow&) = delete;

    int frameCount() const { return int(m_frameSizes.size()); }
    // Gefärbte Glyphenpfade eines Rahmens
    QList<TextPiece> pieces(int frame) const;
    // Einzelne Zeichen eines Rahmens (ohne Aufzählungszeichen und Initialen)
    QList<TextGlyph> glyphs(int frame) const;
    // Text passt nicht in die Rahmen
    bool overflows() const { return m_overflow; }
    // Ausdehnung des gesetzten Textes (Grafiktext: breiteste Zeile x Gesamthöhe)
    QRectF extent() const;
    // Textpositionen [first, last) im Rahmen; leerer Rahmen: first == last
    std::pair<int, int> frameRange(int frame) const;

    struct Caret {
        int frame = -1; // -1 = im Überlauf, nicht sichtbar
        QPointF top;
        double height = 0.0;
    };
    Caret caret(int position) const;
    // Textposition an einer Stelle im Rahmen
    int positionAt(int frame, const QPointF& local) const;
    // Markierung [from, to) als Fläche im Rahmen
    QPainterPath selection(int frame, int from, int to) const;
    // Position eine Zeile darüber/darunter (über Rahmengrenzen hinweg), Anfang/Ende der Zeile
    int lineAbove(int position) const { return verticalMove(position, -1); }
    int lineBelow(int position) const { return verticalMove(position, +1); }
    int lineStart(int position) const;
    int lineEnd(int position) const;

private:
    struct Line {
        int block;
        int index; // Zeile im Block
        int frame; // -1 = Überlauf
    };
    int lineOf(int position) const;
    int verticalMove(int position, int delta) const;

    // Aufzählungszeichen und Initiale eines Absatzes (Layout-Einheiten, Ursprung = Grundlinie)
    struct BlockExtra {
        QPainterPath bullet;
        double bulletX = 0.0;
        double bulletIndent = 0.0;
        int dropLines = 0;
        QPainterPath cap;
        double capIndent = 0.0;
        std::optional<Color> color;
    };
    std::vector<BlockExtra> m_extras;
    std::vector<std::unique_ptr<QTextLayout>> m_layouts; // je Absatz
    std::vector<int> m_blockStart;                        // Textposition des Absatzes
    std::vector<int> m_firstLine;                         // erste Zeile des Absatzes in m_lines
    std::vector<Line> m_lines;
    std::vector<QSizeF> m_frameSizes; // Layout-Einheiten
    int m_endPosition = 0;
    double m_width = 0.0; // breiteste Zeile (Grafiktext)
    double m_height = 0.0;
    bool m_overflow = false;
};

} // namespace vek

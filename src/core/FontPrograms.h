#pragma once

#include <QByteArray>
#include <QPainterPath>
#include <QString>
#include <QTransform>

#include <memory>

namespace vek::fontprog {

// Glyphenname -> Zeichen (Adobe Glyph List, uniXXXX, uXXXX[XX]); leer, wenn unbekannt
QString unicodeForGlyphName(const QString& name);
// Glyphennamen der Standardkodierungen (".notdef" bei leeren Plätzen)
QString standardEncoding(int code);
QString winAnsiEncoding(int code);
QString macRomanEncoding(int code);

// In PDF eingebettetes Schriftprogramm (FontFile, FontFile2, FontFile3): liefert die Umrisse
// der Glyphen. TrueType (auch zusammengesetzte Glyphen), CFF/Type 2 (auch CID-Schriften und
// OpenType mit CFF) und Type 1 (eexec, Flex, seac).
class Program {
public:
    static std::unique_ptr<Program> load(const QByteArray& data);
    virtual ~Program() = default;

    virtual int glyphCount() const = 0;
    // Umriss in Glyph-Einheiten, y nach oben
    virtual QPainterPath glyph(int gid) const = 0;
    // Glyph-Einheiten -> Textraum (1 = Schriftgrösse)
    virtual QTransform fontMatrix() const = 0;
    virtual int gidForName(const QString&) const { return -1; }
    // CID-Schriften (CFF): CID -> Glyphe über den Zeichensatz
    virtual int gidForCid(int cid) const { return cid; }
    virtual bool cidKeyed() const { return false; }
    virtual int gidForUnicode(char32_t) const { return -1; }
    // Eigene Kodierung des Programms (Symbolschriften, Type1-Encoding, cmap 3,0 / 1,0)
    virtual int gidForBuiltinCode(int /*code*/) const { return -1; }
    // Name der Schrift im Programm (falls vorhanden)
    virtual QString fontName() const { return {}; }
};

} // namespace vek::fontprog

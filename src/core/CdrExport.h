#pragma once

#include <QBrush>
#include <QByteArray>
#include <QImage>
#include <QPainterPath>
#include <QPen>
#include <QSizeF>
#include <QString>

#include <memory>
#include <optional>

namespace vek::cdr {

// Zeichnung im CDR-Format schreiben:
//  X3 – Version 13: RIFF 'CDRD' mit komprimierten Abschnitten
//  X6 – Version 16: ZIP mit content/root.dat (RIFF 'CDRG'), dessen Blöcke auf Datendateien
//       (data1.dat, masterPage.dat, pageN.dat) verweisen, dazu Stilvorlagen und Vorschaubild
// Voreinstellungen (Dokumentkonfiguration, Standardstile) kommen aus den Vorlagen
// :/cdr/x3-template.riff bzw. :/cdr/x6-template.riff (tools/cdr-template.py). Jede Seite hat
// dieselbe Grösse. Koordinaten in mm, (0,0) oben links, y nach unten. Objekte in
// Zeichenreihenfolge (das erste liegt zuunterst).
class Writer {
public:
    enum class Version { X3, X6 };

    explicit Writer(const QSizeF& pageMm, Version version = Version::X3);
    ~Writer();

    void beginPage();
    void beginLayer(const QString& name);
    // Gruppen dürfen verschachtelt werden; eine Gruppe mit nur einem Objekt entfällt
    void beginGroup();
    void endGroup();
    // Kurve mit Füllung (einfarbig, linearer, radialer oder konischer Verlauf; der Pinsel samt
    // Transformation in mm) und Kontur (Breite in mm, Enden, Ecken, Strichmuster). Durchsicht
    // fällt weg. Gefüllte offene Teilpfade werden geschlossen.
    void addPath(const QPainterPath& path, const std::optional<QBrush>& fill, const std::optional<QPen>& stroke);
    // Vorschaubild (wird auf 96 × 96 Bildpunkte verkleinert)
    void setPreview(const QImage& image);

    // Leeres Ergebnis, wenn die Vorlage fehlt
    QByteArray finish(QString* error = nullptr);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace vek::cdr

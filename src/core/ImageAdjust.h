#pragma once

#include <QImage>
#include <QList>
#include <QPointF>

#include <array>

namespace vek::adjust {

// Bildanpassungen wie im Bildanpassungseditor und im Menü Effekte. Alle
// Funktionen rechnen in 16 Bit je Kanal und liefern ein neues Bild (RGBA64); 14-Bit-Fotos
// verlieren so keine Abstufungen. Die Deckkraft bleibt unverändert. Reglerwerte und Kurven
// beziehen sich weiter auf die gewohnte Skala 0..255.

// Regler des Bildanpassungseditors, je -100..100, 0 = unverändert
struct Basic {
    int temperature = 0; // + wärmer (rötlich), - kälter (bläulich)
    int tint = 0;        // + Magenta, - Grün
    int saturation = 0;
    int brightness = 0;
    int contrast = 0;
    int highlights = 0;
    int shadows = 0;
    int midtones = 0;
    bool isIdentity() const;
};
QImage apply(const QImage& image, const Basic& basic);

// Automatische Anpassung: Tonwerte je Kanal auf den vollen Umfang strecken (0,5 % Rand)
QImage autoAdjust(const QImage& image);
// Kontrastverbesserung: wie oben, aber für die Helligkeit gemeinsam (Farben bleiben)
QImage stretchContrast(const QImage& image);
QImage invert(const QImage& image);
QImage desaturate(const QImage& image);
QImage posterize(const QImage& image, int levels);
QImage gamma(const QImage& image, double gamma);
// Helligkeit, Kontrast, Intensität (Mitteltöne) je -100..100
QImage brightnessContrastIntensity(const QImage& image, int brightness, int contrast, int intensity);
// Farbton -180..180 Grad, Sättigung und Helligkeit -100..100
QImage hueSaturationLightness(const QImage& image, int hue, int saturation, int lightness);
// Farbbalance je -100..100: Cyan–Rot, Magenta–Grün, Gelb–Blau
QImage colorBalance(const QImage& image, int cyanRed, int magentaGreen, int yellowBlue);
// Tonkurve durch Punkte (0..255), gilt für alle Kanäle oder einen (0 = R, 1 = G, 2 = B)
QImage toneCurve(const QImage& image, const QList<QPointF>& points, int channel = -1);
// Stützpunkte zu einer glatten, monotonen Tabelle 0..255
std::array<int, 256> curveTable(QList<QPointF> points);

// Häufigkeiten je Kanal (R, G, B) für das Histogramm
std::array<std::array<int, 256>, 3> histogram(const QImage& image);

} // namespace vek::adjust

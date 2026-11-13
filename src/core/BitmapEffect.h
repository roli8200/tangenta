#pragma once

#include "BitmapFilters.h"
#include "Color.h"
#include "ImageCorrect.h"

#include <QImage>
#include <QJsonObject>
#include <QKeySequence>
#include <QList>
#include <QString>

#include <functional>

namespace vek {

// Ein Bitmap-Effekt im Effektstapel eines Objekts (nicht-destruktiv): Kennung des Effekts,
// Reglerwerte und Farben wie im Dialog, ein/aus. Das Objekt bleibt bearbeitbar; beim Zeichnen
// wird es gerastert und der Stapel der Reihe nach angewendet.
struct BitmapEffect {
    QString id;          // z. B. "blur.soften", "creative.frame"
    QList<int> values;   // Reglerwerte in der Reihenfolge des Dialogs
    QList<Color> colors; // Farbfelder des Dialogs
    bool enabled = true;

    QJsonObject toJson() const;
    static BitmapEffect fromJson(const QJsonObject& json);
    friend bool operator==(const BitmapEffect&, const BitmapEffect&) = default;
};

namespace fx {

// Pixelangaben in den Reglern beziehen sich auf diese Auflösung; gerastert wird je nach Zoom bzw.
// Ausgabe feiner oder gröber, die Pixelwerte werden dann mitskaliert.
constexpr double kReferenceDpi = 300.0;
constexpr double referencePxPerMm() { return kReferenceDpi / 25.4; }

// Texte der Definitionen sind deutsch (Kontext "vek::MainWindow", wo die Menüs sie schon übersetzen)
struct Slider {
    const char* label;
    int minimum = -100;
    int maximum = 100;
    int value = 0;
    bool pixels = false; // Wert in Pixeln: wird mit der Auflösung skaliert
};
struct ColorField {
    const char* label;
    Color value;
};
// Custom: eigener Dialog je Kennung (Zylinder, Seite aufrollen, Korrekturen mit Pipette/Bereichen)
enum class Dialog { None, Sliders, ImageAdjust, Frame, Custom };
using Function = std::function<QImage(const QImage&, const QList<int>&, const QList<Color>&)>;

struct Definition {
    QString id;
    QString group;          // Menügruppe: mode, adjust, 3d, art, blur, color, contour, creative, distort, noise, sharpen,
                            // effects-adjust, effects-correction
    const char* menuText;   // mit &
    const char* name;       // Dialogtitel, Rückgängig-Text, Name im Andockfenster
    Dialog dialog = Dialog::Sliders;
    QList<Slider> sliders;
    QList<ColorField> colors;
    Function apply;
    double margin = 0.0; // so weit (Referenzpixel) reicht der Effekt über den Rand des Objekts
    QKeySequence shortcut = {};
    // Reichweite, die von den Reglerwerten abhängt (Referenzpixel); gilt statt margin, wenn gesetzt
    std::function<double(const QList<int>&)> reach = {};
};

const QList<Definition>& definitions();
const Definition* find(const QString& id);
// Übersetzter Name ("Weichzeichnen"); unbekannte Kennungen kommen unverändert zurück
QString displayName(const QString& id);
QString translate(const char* text);
// Effekt mit den Vorgabewerten des Dialogs
BitmapEffect defaults(const Definition& def);
// Werte vervollständigen (fehlende mit Vorgaben, Bereich begrenzen)
BitmapEffect normalized(const BitmapEffect& e);
// Pixelwerte für ein Bild mit scale Pixeln je Referenzpixel umrechnen
QList<int> scaledValues(const Definition& def, QList<int> values, double scale);

// Einen Effekt bzw. den ganzen Stapel (nur eingeschaltete) anwenden; scale = Pixel je
// Referenzpixel. Ergebnis ARGB32_Premultiplied in der Grösse des Eingangs.
QImage apply(const QImage& in, const BitmapEffect& e, double scale);
QImage applyStack(const QImage& in, const QList<BitmapEffect>& stack, double scale);
bool anyActive(const QList<BitmapEffect>& stack);
// Wie weit der Stapel über das Objekt hinausreicht, in Referenzpixeln
double margin(const QList<BitmapEffect>& stack);

// Werte der Effekte mit eigenem Dialog <-> Einstellungen
filters::PageCurl pageCurlSettings(const QList<int>& values, const QList<Color>& colors);
QList<int> pageCurlValues(const filters::PageCurl& curl);
adjust::SelectiveColor selectiveColorSettings(const QList<int>& values);
QList<int> selectiveColorValues(const adjust::SelectiveColor& settings);
adjust::ReplaceColors replaceColorsSettings(const QList<int>& values, const QList<Color>& colors);
adjust::ChannelMixer channelMixerSettings(const QList<int>& values);
QList<int> channelMixerValues(const adjust::ChannelMixer& mixer);

} // namespace fx

} // namespace vek

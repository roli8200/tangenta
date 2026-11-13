#include "BitmapEffect.h"

#include "BitmapArt.h"
#include "BitmapFilters.h"
#include "BitmapFrame.h"
#include "ImageAdjust.h"
#include "ImageCorrect.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace vek {

QJsonObject BitmapEffect::toJson() const
{
    QJsonObject o{{QStringLiteral("id"), id}};
    QJsonArray v;
    for (int x : values)
        v.append(x);
    o[QStringLiteral("values")] = v;
    if (!colors.isEmpty()) {
        QJsonArray c;
        for (const Color& color : colors)
            c.append(color.toHex());
        o[QStringLiteral("colors")] = c;
    }
    if (!enabled)
        o[QStringLiteral("enabled")] = false;
    return o;
}

BitmapEffect BitmapEffect::fromJson(const QJsonObject& json)
{
    BitmapEffect e;
    e.id = json.value(QStringLiteral("id")).toString();
    for (const QJsonValue& v : json.value(QStringLiteral("values")).toArray())
        e.values << v.toInt();
    for (const QJsonValue& v : json.value(QStringLiteral("colors")).toArray())
        e.colors << Color::fromHex(v.toString()).value_or(Color(0, 0, 0));
    e.enabled = json.value(QStringLiteral("enabled")).toBool(true);
    return e;
}

namespace fx {

namespace {

// Texte direkt mit QT_TRANSLATE_NOOP markieren: lupdate löst keine eigenen Makros auf

using V = const QList<int>&;
using C = const QList<Color>&;

QList<Slider> selectiveColorSliders()
{
    QList<Slider> list;
    for (int range = 0; range < adjust::kColorRanges; ++range)
        for (const char* label : {QT_TRANSLATE_NOOP("vek::MainWindow", "Cyan (%):"), QT_TRANSLATE_NOOP("vek::MainWindow", "Magenta (%):"), QT_TRANSLATE_NOOP("vek::MainWindow", "Gelb (%):"), QT_TRANSLATE_NOOP("vek::MainWindow", "Schwarz (%):")})
            list << Slider{label};
    list << Slider{QT_TRANSLATE_NOOP("vek::MainWindow", "Relativ (1/0):"), 0, 1, 1};
    return list;
}

QList<Slider> channelMixerSliders()
{
    QList<Slider> list;
    for (int out = 0; out < 3; ++out) {
        list << Slider{QT_TRANSLATE_NOOP("vek::MainWindow", "Rot (%):"), -200, 200, out == 0 ? 100 : 0} << Slider{QT_TRANSLATE_NOOP("vek::MainWindow", "Grün (%):"), -200, 200, out == 1 ? 100 : 0}
             << Slider{QT_TRANSLATE_NOOP("vek::MainWindow", "Blau (%):"), -200, 200, out == 2 ? 100 : 0} << Slider{QT_TRANSLATE_NOOP("vek::MainWindow", "Konstante (%):"), -100, 100, 0};
    }
    return list;
}

QList<Definition> makeDefinitions()
{
    QList<Definition> d;
    auto add = [&d](Definition def) { d << std::move(def); };
    // Bitmaps (oben)
    add({QStringLiteral("adjust.auto"), QStringLiteral("adjust"), QT_TRANSLATE_NOOP("vek::MainWindow", "&Automatische Anpassung"), QT_TRANSLATE_NOOP("vek::MainWindow", "Automatische Anpassung"), Dialog::None, {}, {},
         [](const QImage& i, V, C) { return adjust::autoAdjust(i); }});
    add({QStringLiteral("adjust.lab"), QStringLiteral("adjust"), QT_TRANSLATE_NOOP("vek::MainWindow", "Bildanpassungs&editor …"), QT_TRANSLATE_NOOP("vek::MainWindow", "Bildanpassungseditor"), Dialog::ImageAdjust,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Temperatur:")}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Tönung:")}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Sättigung:")}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Helligkeit:")}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Kontrast:")}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Glanzlichter:")},
          {QT_TRANSLATE_NOOP("vek::MainWindow", "Schatten:")}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Mitteltöne:")}, {"auto", 0, 1, 0}},
         {},
         [](const QImage& i, V v, C) {
             adjust::Basic b{v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]};
             return adjust::apply(v[8] ? adjust::autoAdjust(i) : i, b);
         }});
    // Bitmaps > Modus
    add({QStringLiteral("mode.bw"), QStringLiteral("mode"), QT_TRANSLATE_NOOP("vek::MainWindow", "&Schwarzweiss (1 Bit) …"), QT_TRANSLATE_NOOP("vek::MainWindow", "Schwarzweiss"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Schwelle:"), 0, 255, 128}}, {}, [](const QImage& i, V v, C) { return filters::toBlackWhite(i, v[0]); }});
    add({QStringLiteral("mode.gray8"), QStringLiteral("mode"), QT_TRANSLATE_NOOP("vek::MainWindow", "&Graustufen (8 Bit)"), QT_TRANSLATE_NOOP("vek::MainWindow", "Graustufen (8 Bit)"), Dialog::None, {}, {},
         [](const QImage& i, V, C) { return filters::toGrayscale(i, 8); }});
    add({QStringLiteral("mode.gray16"), QStringLiteral("mode"), QT_TRANSLATE_NOOP("vek::MainWindow", "G&raustufen (16 Bit)"), QT_TRANSLATE_NOOP("vek::MainWindow", "Graustufen (16 Bit)"), Dialog::None, {}, {},
         [](const QImage& i, V, C) { return filters::toGrayscale(i, 16); }});
    for (int bits : {8, 16})
        add({bits == 8 ? QStringLiteral("mode.duotone8") : QStringLiteral("mode.duotone16"), QStringLiteral("mode"),
             bits == 8 ? QT_TRANSLATE_NOOP("vek::MainWindow", "&Duoton (8 Bit) …") : QT_TRANSLATE_NOOP("vek::MainWindow", "D&uoton (16 Bit) …"), QT_TRANSLATE_NOOP("vek::MainWindow", "Duoton"), Dialog::Sliders,
             {{QT_TRANSLATE_NOOP("vek::MainWindow", "Gewichtung Farbe 2:"), 0, 100, 50}},
             {{QT_TRANSLATE_NOOP("vek::MainWindow", "Farbe 1 (Tiefen):"), Color(0, 0, 0)}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Farbe 2:"), Color(150, 95, 40)}},
             [bits](const QImage& i, V v, C c) { return filters::duotone(i, c[0], c[1], v[0], bits); }});
    // 3D-Effekte
    add({QStringLiteral("3d.rotate"), QStringLiteral("3d"), QT_TRANSLATE_NOOP("vek::MainWindow", "3D-Drehung"), QT_TRANSLATE_NOOP("vek::MainWindow", "3D-Drehung"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Senkrecht (Grad):"), -75, 75, 20}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Waagrecht (Grad):"), -75, 75, 20}}, {},
         [](const QImage& i, V v, C) { return filters::rotate3d(i, v[0], v[1]); }});
    add({QStringLiteral("3d.perspective"), QStringLiteral("3d"), QT_TRANSLATE_NOOP("vek::MainWindow", "Perspektive"), QT_TRANSLATE_NOOP("vek::MainWindow", "Perspektive"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Waagrecht (%):"), -100, 100, 40}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Senkrecht (%):"), -100, 100, 0}}, {},
         [](const QImage& i, V v, C) { return filters::perspective(i, v[0], v[1]); }});
    add({QStringLiteral("3d.sphere"), QStringLiteral("3d"), QT_TRANSLATE_NOOP("vek::MainWindow", "Sphäre"), QT_TRANSLATE_NOOP("vek::MainWindow", "Sphäre"), Dialog::Sliders, {{QT_TRANSLATE_NOOP("vek::MainWindow", "Stärke:"), 0, 100, 60}}, {},
         [](const QImage& i, V v, C) { return filters::sphere(i, v[0]); }});
    // Zylinder und Seite aufrollen: eigene Dialoge (Auswahlfelder statt Regler), Werte wie unten
    add({QStringLiteral("3d.cylinder"), QStringLiteral("3d"), QT_TRANSLATE_NOOP("vek::MainWindow", "Zylinder"), QT_TRANSLATE_NOOP("vek::MainWindow", "Zylinder"), Dialog::Custom,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Prozent:"), -100, 100, 40}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Senkrecht (1/0):"), 0, 1, 0}}, {},
         [](const QImage& i, V v, C) { return filters::cylinder(i, v[0], v[1] == 1); }});
    add({QStringLiteral("3d.pagecurl"), QStringLiteral("3d"), QT_TRANSLATE_NOOP("vek::MainWindow", "Seite aufrollen"), QT_TRANSLATE_NOOP("vek::MainWindow", "Seite aufrollen"), Dialog::Custom,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Ecke:"), 0, 3, 2},
          {QT_TRANSLATE_NOOP("vek::MainWindow", "Senkrecht (1/0):"), 0, 1, 1},
          {QT_TRANSLATE_NOOP("vek::MainWindow", "Undurchsichtig (1/0):"), 0, 1, 1},
          {QT_TRANSLATE_NOOP("vek::MainWindow", "Hintergrund durchsichtig (1/0):"), 0, 1, 0},
          {QT_TRANSLATE_NOOP("vek::MainWindow", "Breite (%):"), 1, 100, 50},
          {QT_TRANSLATE_NOOP("vek::MainWindow", "Höhe (%):"), 1, 100, 50}},
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Rollenfarbe:"), Color(255, 255, 255)}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Hintergrundfarbe:"), Color(255, 255, 255)}},
         [](const QImage& i, V v, C c) { return filters::pageCurl(i, pageCurlSettings(v, c)); }});
    // Künstlerische Striche
    add({QStringLiteral("art.posterize"), QStringLiteral("art"), QT_TRANSLATE_NOOP("vek::MainWindow", "Posterisieren"), QT_TRANSLATE_NOOP("vek::MainWindow", "Posterisieren"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Stufen:"), 2, 32, 6}}, {}, [](const QImage& i, V v, C) { return filters::posterize(i, v[0]); }});
    add({QStringLiteral("art.charcoal"), QStringLiteral("art"), QT_TRANSLATE_NOOP("vek::MainWindow", "Kohle"), QT_TRANSLATE_NOOP("vek::MainWindow", "Kohle"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Grösse:"), 1, 20, 3, true}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Kante:"), 0, 100, 50}}, {},
         [](const QImage& i, V v, C) { return filters::charcoal(i, v[0], v[1]); }});
    add({QStringLiteral("art.impressionism"), QStringLiteral("art"), QT_TRANSLATE_NOOP("vek::MainWindow", "Impressionismus"), QT_TRANSLATE_NOOP("vek::MainWindow", "Impressionismus"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Strichgrösse (Pixel):"), 3, 100, 16, true}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Färbung:"), 0, 100, 30}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Helligkeit:"), -100, 100, 0}}, {},
         [](const QImage& i, V v, C) { return filters::impressionism(i, v[0], v[1], v[2]); }});
    add({QStringLiteral("art.watercolor"), QStringLiteral("art"), QT_TRANSLATE_NOOP("vek::MainWindow", "Wasserfarben"), QT_TRANSLATE_NOOP("vek::MainWindow", "Wasserfarben"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Pinselgrösse:"), 1, 50, 5, true}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Körnung:"), 0, 100, 50}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Wassermenge:"), 0, 100, 50}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Helligkeit:"), -100, 100, 0}},
         {}, [](const QImage& i, V v, C) { return filters::watercolor(i, v[0], v[1], v[2], v[3]); }});
    add({QStringLiteral("art.scraperboard"), QStringLiteral("art"), QT_TRANSLATE_NOOP("vek::MainWindow", "Papierschnitzel"), QT_TRANSLATE_NOOP("vek::MainWindow", "Papierschnitzel"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Dichte:"), 1, 100, 50}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Strichbreite:"), 1, 100, 50}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Farbig (1/0):"), 0, 1, 0}}, {},
         [](const QImage& i, V v, C) { return filters::scraperboard(i, v[0], v[1], v[2] == 1); }});
    add({QStringLiteral("art.wavypaper"), QStringLiteral("art"), QT_TRANSLATE_NOOP("vek::MainWindow", "Wellenpapier"), QT_TRANSLATE_NOOP("vek::MainWindow", "Wellenpapier"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Pinseldruck:"), 0, 100, 50}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Farbig (1/0):"), 0, 1, 1}}, {},
         [](const QImage& i, V v, C) { return filters::wavyPaper(i, v[0], v[1] == 1); }});
    // Unschärfe
    add({QStringLiteral("blur.smooth"), QStringLiteral("blur"), QT_TRANSLATE_NOOP("vek::MainWindow", "Glätten"), QT_TRANSLATE_NOOP("vek::MainWindow", "Glätten"), Dialog::Sliders, {{QT_TRANSLATE_NOOP("vek::MainWindow", "Stärke:"), 1, 100, 40}}, {},
         [](const QImage& i, V v, C) { return filters::smooth(i, v[0]); }, 6.0});
    add({QStringLiteral("blur.soften"), QStringLiteral("blur"), QT_TRANSLATE_NOOP("vek::MainWindow", "Weichzeichnen"), QT_TRANSLATE_NOOP("vek::MainWindow", "Weichzeichnen"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Prozent:"), 1, 100, 50}}, {}, [](const QImage& i, V v, C) { return filters::soften(i, v[0]); }, 4.0});
    // Kreativ
    add({QStringLiteral("creative.fabric"), QStringLiteral("creative"), QT_TRANSLATE_NOOP("vek::MainWindow", "Gewebe"), QT_TRANSLATE_NOOP("vek::MainWindow", "Gewebe"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Grösse (Pixel):"), 3, 200, 16, true}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Komplexität:"), 0, 100, 50}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Helligkeit:"), -100, 100, 0}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Drehung (Grad):"), -90, 90, 0}},
         {}, [](const QImage& i, V v, C) { return filters::fabric(i, v[0], v[1], v[2], v[3]); }});
    add({QStringLiteral("creative.glassblock"), QStringLiteral("creative"), QT_TRANSLATE_NOOP("vek::MainWindow", "Glasblock"), QT_TRANSLATE_NOOP("vek::MainWindow", "Glasblock"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Blockbreite (Pixel):"), 4, 2000, 60, true}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Blockhöhe (Pixel):"), 4, 2000, 60, true}}, {},
         [](const QImage& i, V v, C) { return filters::glassBlock(i, v[0], v[1]); }});
    add({QStringLiteral("creative.frame"), QStringLiteral("creative"), QT_TRANSLATE_NOOP("vek::MainWindow", "Rahmen"), QT_TRANSLATE_NOOP("vek::MainWindow", "Rahmen"), Dialog::Frame,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Vorlage:"), 0, filters::frameShapeCount() - 1, 0},
          {QT_TRANSLATE_NOOP("vek::MainWindow", "Randbreite (%):"), 1, 50, 12},
          {QT_TRANSLATE_NOOP("vek::MainWindow", "Deckkraft (%):"), 0, 100, 100},
          {QT_TRANSLATE_NOOP("vek::MainWindow", "Weichheit (%):"), 0, 100, 10},
          {QT_TRANSLATE_NOOP("vek::MainWindow", "Skalierung (%):"), 10, 300, 100},
          {QT_TRANSLATE_NOOP("vek::MainWindow", "Drehung (Grad):"), -180, 180, 0},
          {QT_TRANSLATE_NOOP("vek::MainWindow", "Farbrand statt Maske (1/0):"), 0, 1, 0},
          {QT_TRANSLATE_NOOP("vek::MainWindow", "Variation:"), 1, 100, 1}},
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Rahmenfarbe:"), Color(255, 255, 255)}},
         [](const QImage& i, V v, C c) {
             filters::FrameSettings s;
             s.shape = v[0];
             s.width = v[1];
             s.opacity = v[2];
             s.softness = v[3];
             s.scale = v[4];
             s.rotation = v[5];
             s.border = v[6] == 1;
             s.seed = quint32(v[7]);
             s.color = c[0];
             return filters::frame(i, s);
         }});
    add({QStringLiteral("creative.smokedglass"), QStringLiteral("creative"), QT_TRANSLATE_NOOP("vek::MainWindow", "Rauchglas"), QT_TRANSLATE_NOOP("vek::MainWindow", "Rauchglas"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Färbung (%):"), 0, 100, 50}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Unschärfe (%):"), 0, 100, 20}}, {{QT_TRANSLATE_NOOP("vek::MainWindow", "Farbe:"), Color(32, 32, 32)}},
         [](const QImage& i, V v, C c) { return filters::smokedGlass(i, c[0], v[0], v[1]); }});
    // 3D-Effekte (Fortsetzung): Relief, Ein-/Ausbuchtung
    add({QStringLiteral("3d.emboss"), QStringLiteral("3d"), QT_TRANSLATE_NOOP("vek::MainWindow", "Relief"), QT_TRANSLATE_NOOP("vek::MainWindow", "Relief"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Tiefe:"), 1, 100, 20}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Richtung (Grad):"), 0, 359, 135},
          {QT_TRANSLATE_NOOP("vek::MainWindow", "Farbe (0 Original, 1 Grau, 2 Schwarz, 3 andere):"), 0, 3, 0}},
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Andere Farbe:"), Color(200, 160, 60)}},
         [](const QImage& i, V v, C c) { return filters::embossColored(i, v[0], v[1], v[2], c[0]); }, 2.0});
    add({QStringLiteral("3d.pinch"), QStringLiteral("3d"), QT_TRANSLATE_NOOP("vek::MainWindow", "Ein-/Ausbuchten"), QT_TRANSLATE_NOOP("vek::MainWindow", "Ein-/Ausbuchten"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Stärke (− ein, + aus):"), -100, 100, 50}}, {},
         [](const QImage& i, V v, C) { return filters::pinch(i, v[0]); }});
    // Künstlerische Striche (Fortsetzung)
    add({QStringLiteral("art.sketch"), QStringLiteral("art"), QT_TRANSLATE_NOOP("vek::MainWindow", "Skizzenblock"), QT_TRANSLATE_NOOP("vek::MainWindow", "Skizzenblock"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Strichbreite (Pixel):"), 1, 20, 3, true}}, {},
         [](const QImage& i, V v, C) { return filters::pencil(i, v[0]); }, 2.0});
    add({QStringLiteral("art.oilpaint"), QStringLiteral("art"), QT_TRANSLATE_NOOP("vek::MainWindow", "Ölfarbe"), QT_TRANSLATE_NOOP("vek::MainWindow", "Ölfarbe"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Pinselgrösse:"), 1, 8, 3}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Farbstufen:"), 2, 64, 20}}, {},
         [](const QImage& i, V v, C) { return filters::oilPaint(i, v[0], v[1]); }, 4.0});
    // Unschärfe (Fortsetzung)
    add({QStringLiteral("blur.gaussian"), QStringLiteral("blur"), QT_TRANSLATE_NOOP("vek::MainWindow", "Gaußsche Unschärfe"), QT_TRANSLATE_NOOP("vek::MainWindow", "Gaußsche Unschärfe"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Radius (Pixel):"), 1, 200, 10, true}}, {},
         [](const QImage& i, V v, C) { return filters::gaussian(i, v[0]); }, 40.0});
    // Gauss mit Sigma = Radius/2 reicht etwa 3 Sigma weit; sonst wird der Rand hart abgeschnitten
    d.last().reach = [](const QList<int>& v) { return v.isEmpty() ? 40.0 : 1.5 * v[0] + 2.0; };
    add({QStringLiteral("blur.motion"), QStringLiteral("blur"), QT_TRANSLATE_NOOP("vek::MainWindow", "Bewegungsunschärfe"), QT_TRANSLATE_NOOP("vek::MainWindow", "Bewegungsunschärfe"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Abstand (Pixel):"), 1, 200, 20, true}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Richtung (Grad):"), 0, 359, 0}}, {},
         [](const QImage& i, V v, C) { return filters::motion(i, v[0], v[1]); }, 40.0});
    d.last().reach = [](const QList<int>& v) { return v.isEmpty() ? 40.0 : v[0] / 2.0 + 2.0; };
    add({QStringLiteral("blur.radial"), QStringLiteral("blur"), QT_TRANSLATE_NOOP("vek::MainWindow", "Kreisförmige Unschärfe"), QT_TRANSLATE_NOOP("vek::MainWindow", "Kreisförmige Unschärfe"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Stärke:"), 1, 100, 20}}, {}, [](const QImage& i, V v, C) { return filters::radialBlur(i, v[0]); }});
    add({QStringLiteral("blur.zoom"), QStringLiteral("blur"), QT_TRANSLATE_NOOP("vek::MainWindow", "Zoom-Unschärfe"), QT_TRANSLATE_NOOP("vek::MainWindow", "Zoom-Unschärfe"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Stärke:"), 1, 100, 30}}, {}, [](const QImage& i, V v, C) { return filters::zoomBlur(i, v[0]); }});
    // Farbtransformation
    add({QStringLiteral("color.solarize"), QStringLiteral("color"), QT_TRANSLATE_NOOP("vek::MainWindow", "Solarisation"), QT_TRANSLATE_NOOP("vek::MainWindow", "Solarisation"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Stufe:"), 0, 255, 128}}, {}, [](const QImage& i, V v, C) { return filters::solarize(i, v[0]); }});
    add({QStringLiteral("color.psychedelic"), QStringLiteral("color"), QT_TRANSLATE_NOOP("vek::MainWindow", "Psychedelisch"), QT_TRANSLATE_NOOP("vek::MainWindow", "Psychedelisch"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Stufe:"), 0, 255, 60}}, {}, [](const QImage& i, V v, C) { return filters::psychedelic(i, v[0]); }});
    add({QStringLiteral("color.halftone"), QStringLiteral("color"), QT_TRANSLATE_NOOP("vek::MainWindow", "Farbraster"), QT_TRANSLATE_NOOP("vek::MainWindow", "Farbraster"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Rasterweite (Pixel):"), 2, 100, 8, true}}, {}, [](const QImage& i, V v, C) { return filters::halftone(i, v[0]); }});
    // Kontur
    add({QStringLiteral("contour.findedges"), QStringLiteral("contour"), QT_TRANSLATE_NOOP("vek::MainWindow", "Kanten suchen"), QT_TRANSLATE_NOOP("vek::MainWindow", "Kanten suchen"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Stärke:"), 1, 100, 50}}, {}, [](const QImage& i, V v, C) { return filters::findEdges(i, v[0]); }, 2.0});
    add({QStringLiteral("contour.detectedges"), QStringLiteral("contour"), QT_TRANSLATE_NOOP("vek::MainWindow", "Kanten erkennen"), QT_TRANSLATE_NOOP("vek::MainWindow", "Kanten erkennen"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Stärke:"), 1, 100, 50}}, {}, [](const QImage& i, V v, C) { return filters::detectEdges(i, v[0]); }, 2.0});
    add({QStringLiteral("contour.trace"), QStringLiteral("contour"), QT_TRANSLATE_NOOP("vek::MainWindow", "Kontur verfolgen"), QT_TRANSLATE_NOOP("vek::MainWindow", "Kontur verfolgen"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Schwelle:"), 0, 255, 128}}, {}, [](const QImage& i, V v, C) { return filters::traceContour(i, v[0]); }, 2.0});
    // Kreativ (Fortsetzung)
    add({QStringLiteral("creative.crystallize"), QStringLiteral("creative"), QT_TRANSLATE_NOOP("vek::MainWindow", "Kristallisieren"), QT_TRANSLATE_NOOP("vek::MainWindow", "Kristallisieren"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Zellgrösse (Pixel):"), 2, 200, 16, true}}, {}, [](const QImage& i, V v, C) { return filters::crystallize(i, v[0]); }});
    add({QStringLiteral("creative.mosaic"), QStringLiteral("creative"), QT_TRANSLATE_NOOP("vek::MainWindow", "Mosaik"), QT_TRANSLATE_NOOP("vek::MainWindow", "Mosaik"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Steingrösse (Pixel):"), 2, 200, 12, true}}, {}, [](const QImage& i, V v, C) { return filters::mosaic(i, v[0]); }});
    add({QStringLiteral("creative.vignette"), QStringLiteral("creative"), QT_TRANSLATE_NOOP("vek::MainWindow", "Vignette"), QT_TRANSLATE_NOOP("vek::MainWindow", "Vignette"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Stärke (%):"), 0, 100, 60}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Innenbereich (%):"), 5, 100, 50}}, {{QT_TRANSLATE_NOOP("vek::MainWindow", "Farbe:"), Color(0, 0, 0)}},
         [](const QImage& i, V v, C c) { return filters::vignette(i, v[0], v[1], c[0]); }});
    // Verzerren
    add({QStringLiteral("distort.pixelate"), QStringLiteral("distort"), QT_TRANSLATE_NOOP("vek::MainWindow", "Verpixeln"), QT_TRANSLATE_NOOP("vek::MainWindow", "Verpixeln"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Pixelgrösse:"), 1, 200, 10, true}}, {}, [](const QImage& i, V v, C) { return filters::pixelate(i, v[0]); }});
    add({QStringLiteral("distort.ripple"), QStringLiteral("distort"), QT_TRANSLATE_NOOP("vek::MainWindow", "Welle"), QT_TRANSLATE_NOOP("vek::MainWindow", "Welle"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Amplitude (Pixel):"), 0, 200, 8, true}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Periode (Pixel):"), 2, 1000, 60, true}}, {},
         [](const QImage& i, V v, C) { return filters::ripple(i, v[0], v[1]); }, 8.0});
    add({QStringLiteral("distort.swirl"), QStringLiteral("distort"), QT_TRANSLATE_NOOP("vek::MainWindow", "Strudel"), QT_TRANSLATE_NOOP("vek::MainWindow", "Strudel"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Ganze Umdrehungen:"), -10, 10, 0}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Zusätzliche Grad:"), -359, 359, 120}}, {},
         [](const QImage& i, V v, C) { return filters::swirl(i, v[0] * 360.0 + v[1]); }});
    add({QStringLiteral("distort.wind"), QStringLiteral("distort"), QT_TRANSLATE_NOOP("vek::MainWindow", "Wind"), QT_TRANSLATE_NOOP("vek::MainWindow", "Wind"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Stärke:"), 1, 200, 30, true}}, {}, [](const QImage& i, V v, C) { return filters::wind(i, v[0]); }, 30.0});
    add({QStringLiteral("distort.tiles"), QStringLiteral("distort"), QT_TRANSLATE_NOOP("vek::MainWindow", "Kacheln"), QT_TRANSLATE_NOOP("vek::MainWindow", "Kacheln"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Kacheln je Zeile:"), 1, 100, 8}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Versatz (%):"), 0, 100, 20}}, {},
         [](const QImage& i, V v, C) { return filters::tiles(i, v[0], v[1] / 100.0); }});
    add({QStringLiteral("distort.diffuse"), QStringLiteral("distort"), QT_TRANSLATE_NOOP("vek::MainWindow", "Streuen"), QT_TRANSLATE_NOOP("vek::MainWindow", "Streuen"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Abstand (Pixel):"), 1, 50, 4, true}}, {}, [](const QImage& i, V v, C) { return filters::diffuse(i, v[0]); }, 4.0});
    // Rauschen
    add({QStringLiteral("noise.add"), QStringLiteral("noise"), QT_TRANSLATE_NOOP("vek::MainWindow", "Rauschen hinzufügen"), QT_TRANSLATE_NOOP("vek::MainWindow", "Rauschen hinzufügen"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Stärke (%):"), 0, 100, 25}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Einfarbig (1/0):"), 0, 1, 0}}, {},
         [](const QImage& i, V v, C) { return filters::addNoise(i, v[0], v[1] == 1); }});
    add({QStringLiteral("noise.median"), QStringLiteral("noise"), QT_TRANSLATE_NOOP("vek::MainWindow", "Rauschen entfernen"), QT_TRANSLATE_NOOP("vek::MainWindow", "Rauschen entfernen"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Radius (Pixel):"), 1, 10, 2}}, {}, [](const QImage& i, V v, C) { return filters::median(i, v[0]); }});
    // Schärfe
    add({QStringLiteral("sharpen.sharpen"), QStringLiteral("sharpen"), QT_TRANSLATE_NOOP("vek::MainWindow", "Schärfen"), QT_TRANSLATE_NOOP("vek::MainWindow", "Schärfen"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Stärke:"), 1, 100, 40}}, {}, [](const QImage& i, V v, C) { return filters::sharpen(i, v[0]); }});
    add({QStringLiteral("sharpen.unsharp"), QStringLiteral("sharpen"), QT_TRANSLATE_NOOP("vek::MainWindow", "Unscharf maskieren"), QT_TRANSLATE_NOOP("vek::MainWindow", "Unscharf maskieren"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Prozent:"), 1, 500, 100}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Radius (Pixel):"), 1, 50, 2, true}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Schwelle:"), 0, 255, 0}}, {},
         [](const QImage& i, V v, C) { return filters::unsharpMask(i, v[0], v[1], v[2]); }});
    // Effekte > Anpassen
    add({QStringLiteral("adjust.contrast"), QStringLiteral("effects-adjust"), QT_TRANSLATE_NOOP("vek::MainWindow", "&Kontrastverbesserung"), QT_TRANSLATE_NOOP("vek::MainWindow", "Kontrastverbesserung"), Dialog::None, {}, {},
         [](const QImage& i, V, C) { return adjust::stretchContrast(i); }});
    add({QStringLiteral("adjust.bci"), QStringLiteral("effects-adjust"), QT_TRANSLATE_NOOP("vek::MainWindow", "&Helligkeit/Kontrast/Intensität …"), QT_TRANSLATE_NOOP("vek::MainWindow", "Helligkeit/Kontrast/Intensität"),
         Dialog::Sliders, {{QT_TRANSLATE_NOOP("vek::MainWindow", "Helligkeit:")}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Kontrast:")}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Intensität:")}}, {},
         [](const QImage& i, V v, C) { return adjust::brightnessContrastIntensity(i, v[0], v[1], v[2]); }, 0.0, QKeySequence(Qt::CTRL | Qt::Key_B)});
    add({QStringLiteral("adjust.colorbalance"), QStringLiteral("effects-adjust"), QT_TRANSLATE_NOOP("vek::MainWindow", "&Farbbalance …"), QT_TRANSLATE_NOOP("vek::MainWindow", "Farbbalance"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Cyan – Rot:")}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Magenta – Grün:")}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Gelb – Blau:")}}, {},
         [](const QImage& i, V v, C) { return adjust::colorBalance(i, v[0], v[1], v[2]); }, 0.0, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B)});
    add({QStringLiteral("adjust.gamma"), QStringLiteral("effects-adjust"), QT_TRANSLATE_NOOP("vek::MainWindow", "&Gamma …"), QT_TRANSLATE_NOOP("vek::MainWindow", "Gamma"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Gamma-Wert (×100):"), 10, 500, 100}}, {}, [](const QImage& i, V v, C) { return adjust::gamma(i, v[0] / 100.0); }});
    add({QStringLiteral("adjust.hsl"), QStringLiteral("effects-adjust"), QT_TRANSLATE_NOOP("vek::MainWindow", "Farbton/&Sättigung/Helligkeit …"), QT_TRANSLATE_NOOP("vek::MainWindow", "Farbton/Sättigung/Helligkeit"),
         Dialog::Sliders, {{QT_TRANSLATE_NOOP("vek::MainWindow", "Farbton:"), -180, 180, 0}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Sättigung:")}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Helligkeit:")}}, {},
         [](const QImage& i, V v, C) { return adjust::hueSaturationLightness(i, v[0], v[1], v[2]); }, 0.0,
         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_U)});
    add({QStringLiteral("adjust.localequalize"), QStringLiteral("effects-adjust"), QT_TRANSLATE_NOOP("vek::MainWindow", "&Lokaler Ausgleich …"), QT_TRANSLATE_NOOP("vek::MainWindow", "Lokaler Ausgleich"),
         Dialog::Sliders, {{QT_TRANSLATE_NOOP("vek::MainWindow", "Breite (Pixel):"), 4, 2000, 100, true}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Höhe (Pixel):"), 4, 2000, 100, true}}, {},
         [](const QImage& i, V v, C) { return adjust::localEqualize(i, v[0], v[1]); }});
    // Proben-/Zielabgleich: Farben = drei Proben, dann drei Ziele (Schatten, Mitteltöne, Glanzlichter)
    add({QStringLiteral("adjust.sampletarget"), QStringLiteral("effects-adjust"), QT_TRANSLATE_NOOP("vek::MainWindow", "&Proben-/Zielabgleich …"), QT_TRANSLATE_NOOP("vek::MainWindow", "Proben-/Zielabgleich"),
         Dialog::Custom, {},
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Probe Schatten:"), Color(0, 0, 0)}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Probe Mitteltöne:"), Color(128, 128, 128)},
          {QT_TRANSLATE_NOOP("vek::MainWindow", "Probe Glanzlichter:"), Color(255, 255, 255)}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Ziel Schatten:"), Color(0, 0, 0)},
          {QT_TRANSLATE_NOOP("vek::MainWindow", "Ziel Mitteltöne:"), Color(128, 128, 128)}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Ziel Glanzlichter:"), Color(255, 255, 255)}},
         [](const QImage& i, V, C c) { return adjust::sampleTargetBalance(i, {c[0], c[1], c[2]}, {c[3], c[4], c[5]}); }});
    // Selektive Farbe: je Farbbereich Cyan, Magenta, Gelb, Schwarz, zuletzt die Methode
    add({QStringLiteral("adjust.selectivecolor"), QStringLiteral("effects-adjust"), QT_TRANSLATE_NOOP("vek::MainWindow", "Se&lektive Farbe …"), QT_TRANSLATE_NOOP("vek::MainWindow", "Selektive Farbe"),
         Dialog::Custom, selectiveColorSliders(), {},
         [](const QImage& i, V v, C) { return adjust::selectiveColor(i, selectiveColorSettings(v)); }});
    add({QStringLiteral("adjust.replacecolors"), QStringLiteral("effects-adjust"), QT_TRANSLATE_NOOP("vek::MainWindow", "Farben &ersetzen …"), QT_TRANSLATE_NOOP("vek::MainWindow", "Farben ersetzen"),
         Dialog::Custom, {{QT_TRANSLATE_NOOP("vek::MainWindow", "Bereich:"), 0, 100, 30}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Farbton:"), -180, 180, 0}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Sättigung:")}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Helligkeit:")}},
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Alte Farbe:"), Color(255, 0, 0)}, {QT_TRANSLATE_NOOP("vek::MainWindow", "Neue Farbe:"), Color(0, 0, 255)}},
         [](const QImage& i, V v, C c) { return adjust::replaceColors(i, replaceColorsSettings(v, c)); }});
    // Kanalmixer: je Ausgabekanal Rot, Grün, Blau, Konstante
    add({QStringLiteral("adjust.channelmixer"), QStringLiteral("effects-adjust"), QT_TRANSLATE_NOOP("vek::MainWindow", "Kanal&mixer …"), QT_TRANSLATE_NOOP("vek::MainWindow", "Kanalmixer"), Dialog::Custom,
         channelMixerSliders(), {}, [](const QImage& i, V v, C) { return adjust::channelMixer(i, channelMixerSettings(v)); }});
    add({QStringLiteral("adjust.desaturate"), QStringLiteral("effects-adjust"), QT_TRANSLATE_NOOP("vek::MainWindow", "Sättigung &reduzieren"), QT_TRANSLATE_NOOP("vek::MainWindow", "Sättigung reduzieren"), Dialog::None, {},
         {}, [](const QImage& i, V, C) { return adjust::desaturate(i); }});
    // Effekte > Korrektur
    add({QStringLiteral("correction.invert"), QStringLiteral("effects-correction"), QT_TRANSLATE_NOOP("vek::MainWindow", "&Invertieren"), QT_TRANSLATE_NOOP("vek::MainWindow", "Invertieren"), Dialog::None, {}, {},
         [](const QImage& i, V, C) { return adjust::invert(i); }});
    add({QStringLiteral("correction.posterize"), QStringLiteral("effects-correction"), QT_TRANSLATE_NOOP("vek::MainWindow", "&Abstufen …"), QT_TRANSLATE_NOOP("vek::MainWindow", "Abstufen"), Dialog::Sliders,
         {{QT_TRANSLATE_NOOP("vek::MainWindow", "Stufen:"), 2, 32, 4}}, {}, [](const QImage& i, V v, C) { return adjust::posterize(i, v[0]); }});
    return d;
}

#undef TR

} // namespace

filters::PageCurl pageCurlSettings(const QList<int>& v, const QList<Color>& c)
{
    filters::PageCurl p;
    p.corner = filters::PageCurl::Corner(std::clamp(v.value(0, 2), 0, 3));
    p.vertical = v.value(1, 1) == 1;
    p.opaque = v.value(2, 1) == 1;
    p.transparentBackground = v.value(3, 0) == 1;
    p.width = v.value(4, 50);
    p.height = v.value(5, 50);
    p.curlColor = c.value(0, Color(255, 255, 255));
    p.background = c.value(1, Color(255, 255, 255));
    return p;
}

QList<int> pageCurlValues(const filters::PageCurl& p)
{
    return {int(p.corner), p.vertical ? 1 : 0, p.opaque ? 1 : 0, p.transparentBackground ? 1 : 0, p.width, p.height};
}

adjust::SelectiveColor selectiveColorSettings(const QList<int>& v)
{
    adjust::SelectiveColor s;
    for (int range = 0; range < adjust::kColorRanges; ++range)
        for (int k = 0; k < 4; ++k)
            s.cmyk[std::size_t(range)][std::size_t(k)] = v.value(range * 4 + k, 0);
    s.relative = v.value(adjust::kColorRanges * 4, 1) == 1;
    return s;
}

QList<int> selectiveColorValues(const adjust::SelectiveColor& s)
{
    QList<int> v;
    for (const auto& range : s.cmyk)
        for (int x : range)
            v << x;
    v << (s.relative ? 1 : 0);
    return v;
}

adjust::ReplaceColors replaceColorsSettings(const QList<int>& v, const QList<Color>& c)
{
    adjust::ReplaceColors s;
    s.range = v.value(0, s.range);
    s.hue = v.value(1, 0);
    s.saturation = v.value(2, 0);
    s.lightness = v.value(3, 0);
    s.oldColor = c.value(0, s.oldColor);
    s.newColor = c.value(1, s.newColor);
    return s;
}

adjust::ChannelMixer channelMixerSettings(const QList<int>& v)
{
    adjust::ChannelMixer m;
    for (int out = 0; out < 3; ++out)
        for (int k = 0; k < 4; ++k)
            m.rows[std::size_t(out)][std::size_t(k)] = v.value(out * 4 + k, m.rows[std::size_t(out)][std::size_t(k)]);
    return m;
}

QList<int> channelMixerValues(const adjust::ChannelMixer& m)
{
    QList<int> v;
    for (const auto& row : m.rows)
        for (int x : row)
            v << x;
    return v;
}

const QList<Definition>& definitions()
{
    static const QList<Definition> list = makeDefinitions();
    return list;
}

const Definition* find(const QString& id)
{
    for (const Definition& d : definitions())
        if (d.id == id)
            return &d;
    return nullptr;
}

QString translate(const char* text)
{
    return QCoreApplication::translate("vek::MainWindow", text);
}

QString displayName(const QString& id)
{
    const Definition* d = find(id);
    return d ? translate(d->name) : id;
}

BitmapEffect defaults(const Definition& def)
{
    BitmapEffect e;
    e.id = def.id;
    for (const Slider& s : def.sliders)
        e.values << s.value;
    for (const ColorField& c : def.colors)
        e.colors << c.value;
    return e;
}

BitmapEffect normalized(const BitmapEffect& e)
{
    const Definition* def = find(e.id);
    if (!def)
        return e;
    BitmapEffect n = defaults(*def);
    n.enabled = e.enabled;
    for (int i = 0; i < n.values.size() && i < e.values.size(); ++i)
        n.values[i] = std::clamp(e.values[i], def->sliders[i].minimum, def->sliders[i].maximum);
    for (int i = 0; i < n.colors.size() && i < e.colors.size(); ++i)
        n.colors[i] = e.colors[i];
    return n;
}

QList<int> scaledValues(const Definition& def, QList<int> values, double scale)
{
    for (int i = 0; i < def.sliders.size() && i < values.size(); ++i)
        if (def.sliders[i].pixels)
            values[i] = std::max(1, int(std::lround(values[i] * scale)));
    return values;
}

QImage apply(const QImage& in, const BitmapEffect& e, double scale)
{
    const Definition* def = find(e.id);
    if (!def || !def->apply || in.isNull())
        return in;
    const BitmapEffect n = normalized(e);
    QImage out = def->apply(in, scaledValues(*def, n.values, scale), n.colors);
    if (out.size() != in.size())
        out = out.scaled(in.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return out;
}

QImage applyStack(const QImage& in, const QList<BitmapEffect>& stack, double scale)
{
    QImage image = in;
    for (const BitmapEffect& e : stack)
        if (e.enabled)
            image = apply(image, e, scale);
    return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

bool anyActive(const QList<BitmapEffect>& stack)
{
    return std::any_of(stack.begin(), stack.end(), [](const BitmapEffect& e) { return e.enabled && find(e.id); });
}

double margin(const QList<BitmapEffect>& stack)
{
    double m = 0.0;
    for (const BitmapEffect& e : stack)
        if (const Definition* d = find(e.id); d && e.enabled)
            m += d->reach ? d->reach(normalized(e).values) : d->margin;
    return m;
}

} // namespace fx

} // namespace vek

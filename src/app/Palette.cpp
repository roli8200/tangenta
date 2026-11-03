#include "Palette.h"

#include <QCoreApplication>

namespace vek {

namespace {

QString tr(const char* s)
{
    return QCoreApplication::translate("vek::Palette", s);
}

} // namespace

const QList<NamedColor>& defaultPalette()
{
    static const QList<NamedColor> palette = [] {
        QList<NamedColor> p;
        auto add = [&p](const char* name, int r, int g, int b) {
            p.append({tr(name), Color(static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g),
                                      static_cast<std::uint8_t>(b))});
        };
        // Graustufen
        add(QT_TRANSLATE_NOOP("vek::Palette", "Schwarz"), 0, 0, 0);
        add(QT_TRANSLATE_NOOP("vek::Palette", "90 % Schwarz"), 26, 26, 26);
        add(QT_TRANSLATE_NOOP("vek::Palette", "80 % Schwarz"), 51, 51, 51);
        add(QT_TRANSLATE_NOOP("vek::Palette", "70 % Schwarz"), 77, 77, 77);
        add(QT_TRANSLATE_NOOP("vek::Palette", "60 % Schwarz"), 102, 102, 102);
        add(QT_TRANSLATE_NOOP("vek::Palette", "50 % Schwarz"), 128, 128, 128);
        add(QT_TRANSLATE_NOOP("vek::Palette", "40 % Schwarz"), 153, 153, 153);
        add(QT_TRANSLATE_NOOP("vek::Palette", "30 % Schwarz"), 179, 179, 179);
        add(QT_TRANSLATE_NOOP("vek::Palette", "20 % Schwarz"), 204, 204, 204);
        add(QT_TRANSLATE_NOOP("vek::Palette", "10 % Schwarz"), 230, 230, 230);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Weiss"), 255, 255, 255);
        // Grundfarben
        add(QT_TRANSLATE_NOOP("vek::Palette", "Blau"), 0, 0, 255);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Cyan"), 0, 255, 255);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Grün"), 0, 255, 0);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Gelb"), 255, 255, 0);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Rot"), 255, 0, 0);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Magenta"), 255, 0, 255);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Violett"), 153, 0, 204);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Orange"), 255, 102, 0);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Rosa"), 255, 153, 204);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Schokolade"), 102, 51, 51);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Lavendel"), 204, 204, 255);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Pastellblau"), 153, 153, 255);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Himmelblau"), 102, 153, 255);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Kornblumenblau"), 102, 102, 204);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Marineblau"), 0, 51, 153);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Mitternachtsblau"), 0, 0, 102);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Stahlblau"), 51, 102, 153);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Hellblau"), 0, 204, 255);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Eisblau"), 153, 255, 255);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Graublau"), 102, 153, 153);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Petrol"), 51, 102, 102);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Dunkeltürkis"), 0, 51, 51);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Waldgrün"), 0, 102, 51);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Smaragdgrün"), 0, 153, 51);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Seegrün"), 51, 153, 102);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Frühlingsgrün"), 51, 204, 102);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Mint"), 102, 255, 204);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Türkis"), 51, 204, 153);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Salbei"), 153, 204, 153);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Blassgrün"), 204, 255, 204);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Hellgrün"), 153, 255, 153);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Olivgrün"), 102, 153, 51);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Moosgrün"), 102, 153, 0);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Limette"), 153, 204, 0);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Gelbgrün"), 153, 255, 0);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Chartreuse"), 204, 255, 102);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Dunkeloliv"), 51, 51, 0);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Oliv"), 102, 102, 51);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Khaki"), 153, 153, 102);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Senf"), 153, 153, 51);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Ocker"), 204, 204, 51);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Zitrone"), 255, 255, 102);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Hellgelb"), 255, 255, 153);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Elfenbein"), 255, 255, 204);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Hellbraun"), 153, 102, 51);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Terrakotta"), 204, 102, 51);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Gold"), 204, 153, 51);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Lachs"), 255, 102, 51);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Mandarine"), 255, 153, 51);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Apricot"), 255, 153, 102);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Goldgelb"), 255, 204, 0);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Pfirsich"), 255, 204, 153);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Braun"), 102, 51, 0);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Dunkelrot"), 153, 0, 0);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Ziegelrot"), 204, 51, 0);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Korallenrot"), 255, 102, 102);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Altrosa"), 255, 153, 153);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Puderrosa"), 255, 204, 204);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Weinrot"), 153, 51, 102);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Himbeere"), 204, 51, 102);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Fuchsie"), 204, 51, 153);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Kirschrot"), 255, 0, 102);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Pink"), 255, 102, 153);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Rosarot"), 255, 51, 153);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Altrosé"), 204, 102, 153);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Aubergine"), 102, 0, 102);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Purpur"), 153, 0, 153);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Orchidee"), 255, 153, 255);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Mauve"), 204, 102, 204);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Malve"), 153, 102, 153);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Flieder"), 204, 153, 204);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Dunkelviolett"), 153, 51, 204);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Amethyst"), 204, 102, 255);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Lila"), 153, 102, 204);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Blasslila"), 204, 153, 255);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Indigo"), 51, 0, 102);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Dunkellila"), 102, 51, 153);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Blauviolett"), 153, 102, 255);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Ultramarin"), 102, 0, 255);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Königsblau"), 102, 0, 204);
        add(QT_TRANSLATE_NOOP("vek::Palette", "Kobaltblau"), 102, 51, 255);
        return p;
    }();
    return palette;
}

QString colorName(const Color& c)
{
    for (const NamedColor& n : defaultPalette()) {
        if (n.color.r == c.r && n.color.g == c.g && n.color.b == c.b)
            return n.name;
    }
    return {};
}

} // namespace vek

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
        add("Schwarz", 0, 0, 0);
        add("90 % Schwarz", 26, 26, 26);
        add("80 % Schwarz", 51, 51, 51);
        add("70 % Schwarz", 77, 77, 77);
        add("60 % Schwarz", 102, 102, 102);
        add("50 % Schwarz", 128, 128, 128);
        add("40 % Schwarz", 153, 153, 153);
        add("30 % Schwarz", 179, 179, 179);
        add("20 % Schwarz", 204, 204, 204);
        add("10 % Schwarz", 230, 230, 230);
        add("Weiss", 255, 255, 255);
        // Grundfarben
        add("Blau", 0, 0, 255);
        add("Cyan", 0, 255, 255);
        add("Grün", 0, 255, 0);
        add("Gelb", 255, 255, 0);
        add("Rot", 255, 0, 0);
        add("Magenta", 255, 0, 255);
        add("Violett", 153, 0, 204);
        add("Orange", 255, 102, 0);
        add("Rosa", 255, 153, 204);
        add("Schokolade", 102, 51, 51);
        add("Lavendel", 204, 204, 255);
        add("Pastellblau", 153, 153, 255);
        add("Himmelblau", 102, 153, 255);
        add("Kornblumenblau", 102, 102, 204);
        add("Marineblau", 0, 51, 153);
        add("Mitternachtsblau", 0, 0, 102);
        add("Stahlblau", 51, 102, 153);
        add("Hellblau", 0, 204, 255);
        add("Eisblau", 153, 255, 255);
        add("Graublau", 102, 153, 153);
        add("Petrol", 51, 102, 102);
        add("Dunkeltürkis", 0, 51, 51);
        add("Waldgrün", 0, 102, 51);
        add("Smaragdgrün", 0, 153, 51);
        add("Seegrün", 51, 153, 102);
        add("Frühlingsgrün", 51, 204, 102);
        add("Mint", 102, 255, 204);
        add("Türkis", 51, 204, 153);
        add("Salbei", 153, 204, 153);
        add("Blassgrün", 204, 255, 204);
        add("Hellgrün", 153, 255, 153);
        add("Olivgrün", 102, 153, 51);
        add("Moosgrün", 102, 153, 0);
        add("Limette", 153, 204, 0);
        add("Gelbgrün", 153, 255, 0);
        add("Chartreuse", 204, 255, 102);
        add("Dunkeloliv", 51, 51, 0);
        add("Oliv", 102, 102, 51);
        add("Khaki", 153, 153, 102);
        add("Senf", 153, 153, 51);
        add("Ocker", 204, 204, 51);
        add("Zitrone", 255, 255, 102);
        add("Hellgelb", 255, 255, 153);
        add("Elfenbein", 255, 255, 204);
        add("Hellbraun", 153, 102, 51);
        add("Terrakotta", 204, 102, 51);
        add("Gold", 204, 153, 51);
        add("Lachs", 255, 102, 51);
        add("Mandarine", 255, 153, 51);
        add("Apricot", 255, 153, 102);
        add("Goldgelb", 255, 204, 0);
        add("Pfirsich", 255, 204, 153);
        add("Braun", 102, 51, 0);
        add("Dunkelrot", 153, 0, 0);
        add("Ziegelrot", 204, 51, 0);
        add("Korallenrot", 255, 102, 102);
        add("Altrosa", 255, 153, 153);
        add("Puderrosa", 255, 204, 204);
        add("Weinrot", 153, 51, 102);
        add("Himbeere", 204, 51, 102);
        add("Fuchsie", 204, 51, 153);
        add("Kirschrot", 255, 0, 102);
        add("Pink", 255, 102, 153);
        add("Rosarot", 255, 51, 153);
        add("Altrosé", 204, 102, 153);
        add("Aubergine", 102, 0, 102);
        add("Purpur", 153, 0, 153);
        add("Orchidee", 255, 153, 255);
        add("Mauve", 204, 102, 204);
        add("Malve", 153, 102, 153);
        add("Flieder", 204, 153, 204);
        add("Dunkelviolett", 153, 51, 204);
        add("Amethyst", 204, 102, 255);
        add("Lila", 153, 102, 204);
        add("Blasslila", 204, 153, 255);
        add("Indigo", 51, 0, 102);
        add("Dunkellila", 102, 51, 153);
        add("Blauviolett", 153, 102, 255);
        add("Ultramarin", 102, 0, 255);
        add("Königsblau", 102, 0, 204);
        add("Kobaltblau", 102, 51, 255);
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

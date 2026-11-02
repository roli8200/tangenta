#pragma once

#include <QString>
#include <QStringList>

namespace vek {

// Beschreibt eine Eigenschaft oder einen Befehl eines Objekttyps, ohne Oberfläche zu kennen.
// Die Eigenschaftsleiste baut daraus ihre Bedienelemente: Jeder Objekttyp bringt so seine
// eigene "Icon-Leiste" mit.
struct PropertyDescriptor {
    enum class Kind {
        Length, // Zahl in mm
        Angle,  // Grad
        Number,
        Toggle, // Ein/Aus-Knopf
        Choice, // Gruppe sich ausschliessender Knöpfe; Wert = Index
        Action, // Befehlsknopf ohne Wert
        List,   // Auswahlliste (choiceLabels); Wert = Index
        Text,   // kurzes Textfeld
    };

    QString id;
    QString label;            // Tooltip
    QString icon;             // Symbolname unter :/icons, leer = Text statt Symbol
    Kind kind = Kind::Length;
    QStringList choiceIcons{};  // nur Choice
    QStringList choiceLabels{}; // nur Choice
    double minimum = 0.0;
    double maximum = 100000.0;
    bool stackWithNext = false; // mit der nächsten Eigenschaft übereinander anordnen (wie x/y)
    bool separatorBefore = false;
};

} // namespace vek

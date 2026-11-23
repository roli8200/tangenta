#pragma once

#include <QList>

namespace vek {

// Oberflächengrösse wie eine Lupe über die ganze Oberfläche: Symbole, Leisten, Felder,
// Farbpalette und Schrift wachsen linear mit. Gilt ab dem nächsten Start.
//  - fitScreen: Grösse nach der physischen Bildschirmgrösse statt nach der Skalierung des
//    Systems (Vorgabe aus: die Systemskalierung passt zu den übrigen Programmen)
//  - factor: zusätzliche Vergrösserung (1.0 = 100 %)
struct InterfaceScale {
    bool fitScreen = false;
    double factor = 1.0;
};

InterfaceScale interfaceScale();
void setInterfaceScale(const InterfaceScale& scale);
QList<double> interfaceScaleSteps();
// Vor dem Anlegen der QApplication aufrufen
void applyInterfaceScale();

} // namespace vek

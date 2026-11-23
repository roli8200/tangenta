#pragma once

#include <QPixmap>
#include <QSplashScreen>

class QWidget;

namespace vek {

namespace splash {
// Startbild (680x420 Punkte): Zeichen und Name auf dunklem Grund, rechts Kurven mit Knoten und
// Anfassern, unten Statuszeile, Version und Herausgeber
QSize size();
QPixmap render(qreal devicePixelRatio, const QString& status = {});
} // namespace splash

// Startbild während des Programmstarts; setStatus zeichnet die Statuszeile neu
class SplashScreen : public QSplashScreen {
public:
    SplashScreen();
    void setStatus(const QString& status);
};

// Hilfe > Über TANGENTA: das Startbild mit Version, Qt, Lizenz und Verweisen
void showAbout(QWidget* parent);

} // namespace vek

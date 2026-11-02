#pragma once

#include <QString>

class QScreen;

namespace vek {

// Farbtiefe eines Bildschirms, wie das Betriebssystem sie ausgibt
struct DisplayDepth {
    int bitsPerColor = 8; // je Kanal: 8, 10, 12, 16
    bool hdr = false;     // HDR/"Erweiterte Farbe" eingeschaltet (Windows)
    QString name;         // Gerätename bzw. Bildschirm
    QString source;       // woher die Angabe stammt (DXGI, Qt)
};

// Windows: DXGI (IDXGIOutput6::GetDesc1); sonst QScreen::depth()
DisplayDepth displayDepth(const QScreen* screen);

} // namespace vek

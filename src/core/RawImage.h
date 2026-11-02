#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>
#include <QStringList>

namespace vek::raw {

// Kamera-Rohdaten (Nikon NEF, Canon CR2/CR3, Sony ARW, DNG, …) mit LibRaw entwickeln:
// 16 Bit je Kanal (RGBX64), Weissabgleich der Kamera, sRGB-Farbraum und -Gamma,
// Ausrichtung wie aufgenommen. Damit bleiben 12- und 14-Bit-Aufnahmen ohne Stufen.

// Dateiendungen für Datei-Dialoge und Drag&Drop (klein, ohne Punkt)
QStringList suffixes();
// null, wenn die Daten keine unterstützte Rohdatei sind. suffix: passende Endung ("nef", …)
QImage decode(const QByteArray& data, QString* suffix = nullptr);

} // namespace vek::raw

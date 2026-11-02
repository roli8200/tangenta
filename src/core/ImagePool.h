#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>
#include <QStringList>

namespace vek::imagepool {

// Bilddaten aller Dokumente dieser Sitzung, Schlüssel = SHA-1 der Datei.
//
// Bildobjekte speichern nur den Schlüssel. Dadurch bleiben Undo-Schritte und Kopien klein,
// und ein bearbeitetes Bild (Bildanpassung) ist einfach ein neuer Eintrag: Rückgängig
// schaltet nur den Schlüssel zurück. Einträge werden während der Sitzung nie gelöscht.

// Datei unverändert aufnehmen (JPEG bleibt JPEG, NEF bleibt NEF); leer, wenn sie kein
// lesbares Bild ist. Kamera-Rohdaten werden mit LibRaw in 16 Bit entwickelt.
QString add(const QByteArray& encoded);
// Endungen, die sich importieren lassen (Qt-Bildformate und Kamera-Rohdaten)
QStringList importSuffixes();
// Bearbeitetes Bild aufnehmen (als PNG, bei 16 Bit je Kanal als 16-Bit-PNG)
QString addImage(const QImage& image);
bool contains(const QString& key);
// Dekodiertes Bild in voller Genauigkeit: 16 Bit je Kanal (RGBA64, nicht vormultipliziert),
// auch wenn die Datei nur 8 Bit hat; null bei unbekanntem Schlüssel
QImage image(const QString& key);
// Fürs Zeichnen am Bildschirm: 8 Bit vormultipliziert, einmal berechnet und aufbewahrt.
// Feinere Abstufungen werden gerastert (geordnetes Dithering), damit Verläufe aus 12- und
// 14-Bit-Fotos auf 8-Bit-Bildschirmen keine Stufen zeigen; 8-Bit-Quellen bleiben exakt.
QImage displayImage(const QString& key);
// 16 Bit je Kanal -> 8 Bit mit geordnetem Dithering (siehe displayImage)
QImage ditherTo8Bit(const QImage& image);
// Bits je Kanal der Quelle (8, 16, …)
int sourceBits(const QString& key);
QByteArray encoded(const QString& key);
// Dateiendung passend zum Inhalt ("png", "jpg", …)
QString suffix(const QString& key);

} // namespace vek::imagepool

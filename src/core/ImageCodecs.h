#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>

namespace vek::codecs {

// Bildformate, die Qt nicht (oder nicht auf allen Systemen) lesen bzw. schreiben kann:
// Photoshop (PSD, das zusammengesetzte Bild), PCX und das Schreiben von GIF.

// PSD/PCX an den ersten Bytes erkennen und lesen; suffix = "psd"/"pcx"; null = kein solches Bild
QImage decode(const QByteArray& data, QString* suffix = nullptr);

QImage readPsd(const QByteArray& data);
// RGB(A) mit 8 oder 16 Bit je Kanal (16 Bit, wenn das Bild so viele hat), unkomprimiert
QByteArray writePsd(const QImage& image);
QImage readPcx(const QByteArray& data);
// 24 Bit (drei Ebenen) mit Lauflängen-Kodierung
QByteArray writePcx(const QImage& image);
// GIF89a mit höchstens 256 Farben (Median-Cut), Transparenz über einen Palettenplatz
QByteArray writeGif(const QImage& image);

// Endungen, die diese Codecs zusätzlich zu Qt lesen bzw. schreiben
QStringList readSuffixes();
QStringList writeSuffixes();

} // namespace vek::codecs

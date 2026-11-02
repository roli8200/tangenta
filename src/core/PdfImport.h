#pragma once

#include "SvgImport.h"

#include <QByteArray>
#include <QList>

#include <vector>
#include <QString>
#include <QStringList>

namespace vek::pdf {

// PDF und PDF-kompatible Illustrator-Dateien (.ai) als bearbeitbare Objekte einlesen: Pfade mit
// Füllung und Kontur (Grau, RGB, CMYK), Beschneidungen, Bilder (JPEG und unkomprimierte bzw.
// Flate-Pixel), einfacher Text, Formular-XObjects, Objektströme. Eine Gruppe je Seite.
// Verschlüsselte Dateien werden abgelehnt.
std::vector<svg::Import> read(const QByteArray& data, QString* error = nullptr);

// EPS/PostScript über Ghostscript in PDF wandeln: gsdll64.dll im Programmordner (im eigenen Prozess),
// sonst ein installiertes gswin64c/gs; leer bei Fehler
QByteArray postscriptToPdf(const QByteArray& ps, QString* error = nullptr);
// Ghostscript mit eigenen Argumenten (ohne Programmnamen) ausführen – im Prozess über die
// Bibliothek, sonst als Programm; true bei Erfolg
bool runGhostscript(const QStringList& args, QString* error = nullptr);
// Pfad zur Ghostscript-Bibliothek bzw. zum Programm, oder leer
QString ghostscript();

} // namespace vek::pdf

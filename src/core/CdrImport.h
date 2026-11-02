#pragma once

#include "SvgImport.h"

#include <QByteArray>
#include <QString>

#include <vector>

namespace vek::cdr {

// Zeichnungen im CDR- und CMX-Format lesen (Versionen 7 bis heute, auch die ZIP-Container
// neuerer Fassungen). Grundlage ist libcdr; jede Seite wird eine Gruppe aus Kurven, Bildern
// und Mengentext, in mm mit (0,0) oben links. Schriften stehen in fonts der ersten Seite,
// wie beim PDF-Import, damit fehlende ersetzt werden können.
bool isSupported(const QByteArray& data);
std::vector<svg::Import> read(const QByteArray& data, QString* error = nullptr);

} // namespace vek::cdr

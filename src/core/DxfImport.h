#pragma once

#include "SvgImport.h"

#include <QByteArray>
#include <QColor>
#include <QString>

namespace vek::dxf {

// AutoCAD DXF (ASCII) als Objekte einlesen: LINE, LWPOLYLINE/POLYLINE (mit Bögen), CIRCLE, ARC,
// ELLIPSE, SPLINE (auch rational), TEXT/MTEXT, INSERT (Blöcke), HATCH (Polylinien-, Linien-,
// Bogen-, Ellipsen- und Splineränder; einfarbig oder Verlauf), SOLID/3DFACE, DIMENSION (über ihren
// Block), IMAGE (Bilddatei relativ zu baseDir). Je Ebene eine Gruppe; Farben aus ACI oder
// Echtfarbe, sonst von der Ebene; Transparenz (440). Einheit aus $INSUNITS (ohne Angabe mm).
svg::Import read(const QByteArray& data, QString* error = nullptr, const QString& baseDir = QString());

// AutoCAD-Farbindex (ACI) <-> Farbe
QColor aciColor(int index);
int nearestAci(const QColor& color);

} // namespace vek::dxf

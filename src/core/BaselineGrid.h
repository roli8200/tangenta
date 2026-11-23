#pragma once

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QVariantMap>

namespace vek {

// Grundlinienraster (Optionen > Dokument > Gitter): waagrechte Linien im Abstand step von der
// obersten Linie (top mm unter der Seitenoberkante) bis höchstens bottom mm über der
// Seitenunterkante. Einzelne Linien lassen sich verschieben (z.B. für Titel und Untertitel);
// sie behalten ihren Versatz gegen ihren Platz im Raster, auch wenn sich das Raster ändert.
struct BaselineGrid {
    double top = 15.0;
    double step = 5.0;
    double bottom = 15.0;
    QMap<int, double> moved; // Index der Linie -> Versatz in mm

    // Aus den Dokument-Optionen bzw. als Werte für Options::apply (in der App, BaselineGridOptions.cpp)
    static BaselineGrid fromOptions();
    QVariantMap toOptions() const;
    // Aus den Dokument-Optionen der Masterseite (Kern, ohne Options)
    static BaselineGrid fromJson(const QJsonObject& docOptions);
    // Mengentext richtet seine Zeilen am Raster aus: Option doc/grid/baseline/text, fehlt sie, solange angezeigt
    static bool alignsText(const QJsonObject& docOptions);
    // "3=2.5;7=-1" <-> moved
    static QMap<int, double> parseMoved(const QString& text);
    static QString formatMoved(const QMap<int, double>& moved);

    int count(double pageHeight) const;
    double regular(int index) const { return top + index * step; }
    double line(int index) const { return regular(index) + moved.value(index); }
    QList<double> lines(double pageHeight) const;

    // Ziehen mit der Maus (y = neue Lage der gezogenen Linie, mm ab Seitenoberkante):
    // erste bzw. letzte Linie dehnen oder stauchen das Raster, die andere Endlinie bleibt stehen
    BaselineGrid withFirstAt(double y, double pageHeight) const;
    BaselineGrid withLastAt(double y, double pageHeight) const;
    // Ganzes Raster verschieben
    BaselineGrid shifted(double dy) const;
    // Nur diese Linie verschieben (Versatz 0 = wieder an ihrem Platz)
    BaselineGrid withLineAt(int index, double y) const;

    friend bool operator==(const BaselineGrid&, const BaselineGrid&) = default;
};

} // namespace vek

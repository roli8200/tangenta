#pragma once

#include <QList>
#include <QPainterPath>
#include <QRectF>
#include <QStringList>
#include <QPointF>
#include <QSizeF>
#include <QString>

class QImage;
class QPainter;
class QPrinter;

#include "Imposition.h"
#include "Node.h"

namespace vek {

class Canvas;

// Ausgabe ausserhalb der Zeichenfläche: Drucken (mit Etiketten- und Layout-Ausschiessen) und Export als PDF, Bitmap, SVG und HTML. Alles läuft über Canvas::renderPage,
// damit Druck und Export genau so aussehen wie die Ansicht.
namespace output {

// Verteilung der Dokumentseiten auf Bögen: im Etikettenmodus ein Etikett je Seite, bei
// Layouts wie "Buch" mehrere Seiten nebeneinander, sonst eine Seite je Bogen
struct Sheet {
    QList<std::pair<int, QPointF>> pages; // Seitenindex, Lage der Seite auf dem Bogen (mm)
};
struct Imposition {
    QSizeF sheetSize; // mm
    QList<Sheet> sheets;
};
Imposition impose(const Canvas& canvas, const QList<int>& pages);
// Spalten x Zeilen je Bogen für die Layouts unter Dokument > Layout
QSizeF layoutSplit(int layoutType);

// Bogen in den Painter zeichnen, pxPerMm = Auflösung des Geräts
void paintSheet(const Canvas& canvas, QPainter& p, const Imposition& imposition, int sheet, double pxPerMm);
bool print(const Canvas& canvas, QPrinter& printer, const QList<int>& pages, QString* error = nullptr);

bool exportPdf(const Canvas& canvas, const QString& path, const QList<int>& pages, QString* error = nullptr);

// Ausschiessen für die Druckvorstufe (impose::arrange): pages bildet die Seitenfolge des
// Ausschiessens auf Dokumentseiten ab. Eine Bogenseite zeichnen (Seiten mit Anschnitt, gedreht,
// mit Bundzuwachs; Schnitt- und Falzmarken; Dateiinformation im Rand)
void paintImposedSide(const Canvas& canvas, QPainter& p, const impose::Result& result, const impose::Side& side, const QList<int>& pages,
                      double pxPerMm, const QString& info = QString());
// Drucken: je Bogen Vorder- und (beidseitig) Rückseite; Papier und Duplex stellt der Aufrufer
// am QPrinter ein (setupImposedPrinter). title leer = ohne Dateiinformation
void setupImposedPrinter(QPrinter& printer, const impose::Settings& settings, const impose::Result& result);
bool printImposed(const Canvas& canvas, QPrinter& printer, const impose::Result& result, const QList<int>& pages, const QString& title,
                  QString* error = nullptr);
bool exportImposedPdf(const Canvas& canvas, const QString& path, const impose::Result& result, const QList<int>& pages, const QString& title,
                      QString* error = nullptr);

// Einstellungen des Exportdialogs. Mit "Nur markierte Objekte" (only nicht leer) wird nur das
// Rechteck um diese Objekte ausgegeben.
struct ExportOptions {
    enum class ColorMode { Rgb, Grayscale, BlackWhite };
    int page = 0;
    QList<int> pages;    // PDF: diese Seiten (leer = nur page)
    QList<NodeId> only;  // leer = ganze Seite
    double dpi = 300.0;
    bool transparent = false;
    int quality = 90;    // JPEG, WebP
    ColorMode colorMode = ColorMode::Rgb;
    // EPS/DXF: Auflösung, mit der durchsichtige Stellen (Transparenz, Schatten, Unschärfe,
    // Mischmodi) samt Untergrund gerastert werden
    double flattenDpi = 300.0;
};
// Ausgegebener Bereich in Seitenkoordinaten (mm): Seite oder Rechteck um die Objekte samt Kontur
QRectF exportArea(const Canvas& canvas, const ExportOptions& options);
// Bereich als Bild in der gewünschten Auflösung und Farbart
QImage renderImage(const Canvas& canvas, const ExportOptions& options, QString* error = nullptr);
// Bitmap nach Dateiendung: jpg, png, tif, webp, bmp, gif, ico, psd, pcx
bool exportImage(const Canvas& canvas, const QString& path, const ExportOptions& options, QString* error = nullptr);
QStringList imageExportSuffixes();
// PDF bzw. SVG mit Auswahl
bool exportPdf(const Canvas& canvas, const QString& path, const ExportOptions& options, QString* error = nullptr);
bool exportSvg(const Canvas& canvas, const QString& path, const ExportOptions& options, QString* error = nullptr);
QByteArray areaSvg(const Canvas& canvas, const ExportOptions& options);
// Encapsulated PostScript (Level 3): Pfade, Verläufe als Schattierung, Bilder. PostScript kennt keine
// Transparenz: Durchsichtiges wird reduziert wie in Illustrator – Deckendes bleibt Vektor,
// jede durchsichtige Stelle wird als Bild des fertigen Ergebnisses (flattenDpi) genau in ihrer Form
// beschnitten eingesetzt
QByteArray areaEps(const Canvas& canvas, const ExportOptions& options);
bool exportEps(const Canvas& canvas, const QString& path, const ExportOptions& options, QString* error = nullptr);
// AutoCAD DXF 2004 (mm), Ebenen wie im Dokument: Flächen als Schraffur (einfarbig oder Verlauf,
// mit Transparenz), Konturen als Linienzug oder Spline (Bézierkurven exakt) mit Linienstärke,
// Bilder als IMAGE mit PNG-Datei daneben (Beschneidung im Alphakanal). Was DXF nicht kennt
// (Mischmodi, Muster, mehrfarbige Verläufe) kommt gerastert als Bild in genau der Form.
// images: die zu speichernden PNG-Dateien (Name relativ zur Zeichnung)
QByteArray areaDxf(const Canvas& canvas, const ExportOptions& options, QList<std::pair<QString, QImage>>* images = nullptr,
                   const QString& imageBaseName = QString());
bool exportDxf(const Canvas& canvas, const QString& path, const ExportOptions& options, QString* error = nullptr);
// Umriss der Auswahl als eine Fläche in mm (Konturen als Flächen, Überlappungen vereinigt) – für
// den Export als Schriftzeichen
QPainterPath selectionOutline(const Canvas& canvas, const ExportOptions& options);
// Adobe Illustrator: PDF-kompatible AI-Datei (wie Illustrator selbst sie speichert)
bool exportAi(const Canvas& canvas, const QString& path, const ExportOptions& options, QString* error = nullptr);
// PNG, JPEG, TIFF, BMP … nach Dateiendung; transparent = ohne Hintergrund (falls das Format kann)
bool exportBitmap(const Canvas& canvas, const QString& path, int page, double dpi, bool transparent, QString* error = nullptr);
bool exportSvg(const Canvas& canvas, const QString& path, int page, QString* error = nullptr);
QByteArray pageSvg(const Canvas& canvas, int page);
// Eine HTML-Datei mit allen Seiten als eingebettetes SVG und Seitennavigation
bool exportHtml(const Canvas& canvas, const QString& path, const QString& title, QString* error = nullptr);

} // namespace output

} // namespace vek

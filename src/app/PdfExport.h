#pragma once

#include "PdfSettings.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace vek {

class Canvas;

namespace pdfexport {

// Befund der Vorabkontrolle (Registerkarte "Keine Probleme" / "n Probleme")
struct Issue {
    enum class Severity { Info, Warning, Error };
    Severity severity = Severity::Warning;
    QString title;
    QString detail;
};

// Vorabkontrolle: Bitmaps mit zu wenig Auflösung, Haarlinien, Objekte über den
// Seitenrand ohne Anschnitt, Transparenz bei PDF/X-1a und PDF/A-1, fehlende oder gesperrte
// Schriften, RGB in CMYK-Ausgabe, Verschlüsselung bei PDF/X/A, fehlendes Ghostscript …
QList<Issue> preflight(const Canvas& canvas, const PdfSettings& settings);
int problemCount(const QList<Issue>& issues); // Warnungen und Fehler

// "Als PDF freigeben": Qt schreibt die Seiten (mit Anschnitt, Druckermarken und echter Schrift),
// Ghostscript übernimmt Farbumwandlung, PDF/X/A, Bildkomprimierung, Schriften, Verschlüsselung,
// Dokumentinfo, Lesezeichen, Startansicht, Endformat-/Anschnittrahmen und Skizzen.
// notes: was ohne Ghostscript nicht umgesetzt werden konnte
bool exportPdf(const Canvas& canvas, const QString& path, const PdfSettings& settings, QString* error = nullptr,
               QStringList* notes = nullptr);

} // namespace pdfexport
} // namespace vek

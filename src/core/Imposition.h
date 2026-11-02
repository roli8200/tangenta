#pragma once

#include <QLineF>
#include <QList>
#include <QMarginsF>
#include <QRectF>
#include <QSizeF>
#include <QString>

namespace vek::impose {

// Ausschiessen für die Druckvorstufe: Seiten so auf Druckbogen verteilen, dass nach dem Drucken
// (ein- oder beidseitig), Falzen und Beschneiden das fertige Produkt entsteht.
enum class Layout {
    Single,     // je Bogenseite eine Seite (mit Anschnitt und Schnittmarken auf grösserem Papier)
    NUp,        // Mehrfachnutzen: Seiten der Reihe nach, Spalten x Zeilen je Bogen
    StepRepeat, // Nutzen wiederholen: jede Seite füllt einen Bogen (Visitenkarten, Etiketten)
    Booklet,    // Heft mit Rückendrahtheftung: zwei Seiten je Bogenseite, Bogen ineinander
    Signature8, // Falzbogen 8 Seiten (Kreuzbruch): vier Seiten je Bogenseite, falzen und beschneiden
};

// Wie der Drucker beim beidseitigen Druck das Blatt wendet
enum class Flip { ShortEdge, LongEdge };

struct Settings {
    Layout layout = Layout::Booklet;
    QSizeF paper{210.0, 297.0}; // Papierformat in mm (die Ausrichtung wählt das Ausschiessen)
    int columns = 0;            // Mehrfachnutzen: 0 = so viele wie passen
    int rows = 0;
    double gap = 0.0;           // Abstand zwischen Nutzen; beim Falzbogen der Kopfbeschnitt
    double bleed = 0.0;         // Anschnitt in mm (nur an Schnittkanten, nicht im Bund)
    double margin = 5.0;        // nicht bedruckbarer Rand des Papiers
    bool cropMarks = true;
    bool foldMarks = true;
    bool info = true;           // Dateiinformation im Rand
    double markOffset = 3.0;    // Abstand der Marken vom Endformat
    double markLength = 5.0;
    Flip flip = Flip::ShortEdge;
    bool nested = true;         // Falzbogen ineinander (Rückenheftung) statt hintereinander (Klebebindung)
    double creep = 0.0;         // Bundzuwachs: innerster Bogen so viel mm zum Bund verschoben
};

struct Placement {
    int page = -1;              // Index in der Seitenfolge, -1 = Leerseite
    QRectF trim;                // Endformat auf dem Bogen (mm)
    int rotation = 0;           // 0 oder 180 Grad
    QMarginsF bleed;            // sichtbarer Anschnitt je Kante (Bund: 0)
    double shift = 0.0;         // Bundzuwachs: Inhalt waagrecht verschoben (mm)
};

struct Side {
    QList<Placement> pages;
    QList<QLineF> cropMarks;
    QList<QLineF> foldMarks;
};

struct Sheet {
    Side front;
    Side back;
    bool hasBack = false;
};

struct Result {
    QSizeF sheet;               // Bogen in mm, wie er gedruckt wird (Hoch- oder Querformat)
    QList<Sheet> sheets;
    int pageCount = 0;          // mit ergänzten Leerseiten
    int blankPages = 0;
    bool duplex = false;
    QString error;              // gesetzt, wenn die Seiten nicht auf das Papier passen
    QString warning;            // passt nur randlos
    bool ok() const { return error.isEmpty() && !sheets.isEmpty(); }
};

Result arrange(const Settings& settings, int pageCount, const QSizeF& page);

// Name der Anordnung für Menüs und Dialoge
QString layoutName(Layout layout);
// Kurzbeschreibung des Ergebnisses ("4 Bogen beidseitig, 2 Leerseiten ergänzt")
QString describe(const Result& result);

} // namespace vek::impose

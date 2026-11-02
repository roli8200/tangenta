#pragma once

#include "SvgImport.h"

#include <QList>
#include <QString>
#include <QUrl>

#include <optional>

namespace vek::fontmatch {

// "ABCDEF+Garamond-BoldItalic" -> "Garamond" (fett, kursiv); "TimesNewRomanPSMT" -> "Times New Roman"
QString familyFromBaseName(const QString& baseName, bool* bold = nullptr, bool* italic = nullptr);
// Vergleichsschlüssel: klein, ohne Leerzeichen/Bindestriche und ohne Anhängsel wie MT, PS, Std
QString normalized(const QString& family);
// Installierte Familie gleichen Namens (auch PostScript-Schreibweise) oder leer
QString installedFamily(const QString& family);

struct Candidate {
    QString family;
    double score = 0;      // 0..1
    double shape = -1;     // Formähnlichkeit 0..1, -1 = ohne Umrisse verglichen
    double widths = -1;    // Breitenähnlichkeit 0..1, -1 = ohne Breiten verglichen
};
// Die ähnlichsten installierten Schriften: Umrisse (falls eingebettet) und Zeichenbreiten
// werden mit jeder installierten Familie verglichen, Schnitt (fett/kursiv) passend gewählt
QList<Candidate> suggest(const ImportedFont& font, int count = 8);

struct WebFont {
    QString family;
    QUrl url;
    QString note; // z.B. "metrisch gleich zu Arial"
};
// Bei Google Fonts erhältlich (gleiche Familie) bzw. bekannte freie Gegenstücke
std::optional<WebFont> googleFont(const QString& family);
QList<WebFont> freeAlternatives(const QString& family);
// Websuche nach der Schrift
QUrl searchUrl(const QString& family);

// Was mit den Texten einer Schrift geschieht
struct Choice {
    enum class Kind { Family, Curves };
    Kind kind = Kind::Family;
    QString family; // Kind::Family: diese installierte Schrift
};
// Vorschlag: gleichnamige installierte Schrift; sonst die ähnlichste, wenn sie gut genug passt
// (ab 85 %); sonst – falls eingebettet – exakt als Kurven
Choice defaultChoice(const ImportedFont& font, const QList<Candidate>& candidates);
// Grafiktext mit seiner ersten Grundlinie auf baseline legen (Aufstieg der tatsächlich gewählten
// Schrift) und auf runWidthMm Breite bringen (0 = Breite lassen) – so bleibt der Text nach dem
// Ersetzen der Schrift an seiner Stelle und in seiner Länge
void placeOnBaseline(TextObject& text, const QTransform& baseline, double runWidthMm);
// Auf die importierten Texte anwenden (vor dem Einfügen ins Dokument)
void apply(ImportedFont& font, const Choice& choice);

// Seit dem Programmstart installierte Schriftdateien nachladen (ohne Neustart); Anzahl neuer Familien
int reloadInstalledFonts();

} // namespace vek::fontmatch

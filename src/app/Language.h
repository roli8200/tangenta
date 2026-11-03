#pragma once

#include <QList>
#include <QString>

namespace vek {

// Sprache der Oberfläche. Die Quelltexte sind deutsch; Englisch, Französisch, Italienisch, Spanisch,
// Portugiesisch (Brasilien) und vereinfachtes Chinesisch kommen als Übersetzung (translations/tangenta_<code>.ts, eingebettet als :/i18n/tangenta_<code>.qm).
// Gilt ab dem nächsten Start.
struct Language {
    QString code; // "de", "en", "fr", "it", "es", "pt", "zh"
    QString name; // in der eigenen Sprache, z.B. "Français"
};

QList<Language> languages();
// Gespeicherte Wahl, leer = Sprache des Systems
QString languageSetting();
void setLanguageSetting(const QString& code);
// Tatsächlich verwendete Sprache: Wahl, sonst Systemsprache, sonst Englisch
QString activeLanguage();
// Nach dem Anlegen der QApplication aufrufen: lädt die Übersetzungen von TANGENTA und Qt
void installTranslations();
// Schrift der Oberfläche für die Sprache: Chinesisch braucht chinesische Ersatzschriften, sonst
// nimmt Qt eine japanische, der die vereinfachten Zeichen fehlen
void applyLanguageFont(const QString& code);

} // namespace vek

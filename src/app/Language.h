#pragma once

#include <QList>
#include <QString>

namespace vek {

// Sprache der Oberfläche. Die Quelltexte sind deutsch; Englisch, Französisch und Italienisch
// kommen als Übersetzung (translations/tangenta_<code>.ts, eingebettet als :/i18n/tangenta_<code>.qm).
// Gilt ab dem nächsten Start.
struct Language {
    QString code; // "de", "en", "fr", "it"
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

} // namespace vek

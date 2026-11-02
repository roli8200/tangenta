#pragma once

#include <QHash>
#include <QString>

namespace vek::autocorrect {

// Blitzkorrektur (Optionen > Text > Blitzkorrektur): wirkt beim Tippen auf das
// eben beendete Wort bzw. auf Anführungszeichen.
struct Settings {
    enum class Language { GermanSwiss, German, English, French };

    bool capitalizeSentences = false;
    bool fixTwoCaps = true;
    bool capitalizeDays = true;
    Language language = Language::GermanSwiss;
    bool typographicQuotes = true;
    bool straightAfterNumbers = true;
    QString singleOpen = QStringLiteral("‹");
    QString singleClose = QStringLiteral("›");
    QString doubleOpen = QStringLiteral("«");
    QString doubleClose = QStringLiteral("»");
    bool replaceText = true;
    QHash<QString, QString> replacements; // Ersetzen → Durch

    // Zeilen "alt=neu" wie im Optionsfeld
    static QHash<QString, QString> parseReplacements(const QString& lines);
};

// Ersetzung vor der Schreibmarke: remove Zeichen links löschen, dann insert einfügen. insert
// enthält das getippte Zeichen bereits (ausser bei einem Absatzende, typed = '\n').
struct Edit {
    int remove = 0;
    QString insert;
};

// before = Text des Absatzes links der Schreibmarke, typed = eben getipptes Zeichen
Edit onType(const QString& before, QChar typed, const Settings& settings);

} // namespace vek::autocorrect

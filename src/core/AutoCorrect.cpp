#include "AutoCorrect.h"

#include <QRegularExpression>
#include <QStringList>

namespace vek::autocorrect {

namespace {

bool isWordChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('\'') || c == QChar(0x2019);
}

bool endsWord(QChar c)
{
    return c.isSpace() || c == QLatin1Char('\n') || (c.isPunct() && c != QLatin1Char('-') && c != QLatin1Char('\''));
}

const QStringList& dayNames(Settings::Language lang)
{
    static const QStringList german{QStringLiteral("montag"), QStringLiteral("dienstag"), QStringLiteral("mittwoch"),
                                    QStringLiteral("donnerstag"), QStringLiteral("freitag"), QStringLiteral("samstag"),
                                    QStringLiteral("sonntag"), QStringLiteral("sonnabend")};
    static const QStringList english{QStringLiteral("monday"), QStringLiteral("tuesday"), QStringLiteral("wednesday"),
                                     QStringLiteral("thursday"), QStringLiteral("friday"), QStringLiteral("saturday"),
                                     QStringLiteral("sunday")};
    static const QStringList none;
    switch (lang) {
    case Settings::Language::GermanSwiss:
    case Settings::Language::German: return german;
    case Settings::Language::English: return english;
    case Settings::Language::French: return none; // französische Tagesnamen werden klein geschrieben
    }
    return none;
}

// Steht ab Position pos ein Satzanfang? (Absatzanfang oder nach . ! ? und Leerraum)
bool sentenceStart(const QString& text, int pos)
{
    int i = pos - 1;
    while (i >= 0 && (text.at(i).isSpace() || text.at(i) == QLatin1Char('"') || text.at(i) == QChar(0x00AB)
                      || text.at(i) == QChar(0x201E) || text.at(i) == QLatin1Char('(')))
        --i;
    if (i < 0)
        return true;
    if (i == pos - 1)
        return false; // kein Leerraum davor: mitten im Wort
    const QChar c = text.at(i);
    return c == QLatin1Char('.') || c == QLatin1Char('!') || c == QLatin1Char('?');
}

QString capitalized(const QString& word)
{
    return word.isEmpty() ? word : word.left(1).toUpper() + word.mid(1);
}

} // namespace

QHash<QString, QString> Settings::parseReplacements(const QString& lines)
{
    QHash<QString, QString> map;
    for (const QString& raw : lines.split(QRegularExpression(QStringLiteral("[\\n;]")), Qt::SkipEmptyParts)) {
        const int eq = raw.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString from = raw.left(eq).trimmed();
        if (!from.isEmpty())
            map.insert(from, raw.mid(eq + 1).trimmed());
    }
    return map;
}

Edit onType(const QString& before, QChar typed, const Settings& s)
{
    Edit edit;
    const QString typedText = typed == QLatin1Char('\n') ? QString() : QString(typed);
    edit.insert = typedText;

    // Anführungszeichen: öffnend am Wortanfang, sonst schliessend
    if (typed == QLatin1Char('"') || typed == QLatin1Char('\'')) {
        if (!s.typographicQuotes)
            return edit;
        const QChar prev = before.isEmpty() ? QChar() : before.back();
        if (s.straightAfterNumbers && prev.isDigit())
            return edit; // 5' 10" bleiben Fuss und Zoll
        const bool open = prev.isNull() || prev.isSpace() || prev == QLatin1Char('(') || prev == QLatin1Char('[')
                          || prev == QChar(0x2014) || prev == QChar(0x2013)
                          || (typed == QLatin1Char('\'') && (prev == s.doubleOpen.front()));
        // Apostroph nach einem Buchstaben (z.B. "geht's"), ausser ein einfaches Anführungszeichen ist offen
        const bool singleOpen = before.count(s.singleOpen) > before.count(s.singleClose);
        if (typed == QLatin1Char('\'') && !open && prev.isLetter() && !singleOpen) {
            edit.insert = QString(QChar(0x2019));
            return edit;
        }
        if (typed == QLatin1Char('"'))
            edit.insert = open ? s.doubleOpen : s.doubleClose;
        else
            edit.insert = open ? s.singleOpen : s.singleClose;
        return edit;
    }
    if (!endsWord(typed))
        return edit;

    // Eben beendetes Wort
    int start = int(before.size());
    while (start > 0 && isWordChar(before.at(start - 1)))
        --start;
    const QString word = before.mid(start);
    if (word.isEmpty())
        return edit;
    QString fixed = word;
    if (s.replaceText && s.replacements.contains(word)) {
        fixed = s.replacements.value(word);
    } else {
        // Zwei Grossbuchstaben am Wortanfang, danach klein
        if (s.fixTwoCaps && word.size() >= 3 && word.at(0).isUpper() && word.at(1).isUpper() && word.at(2).isLower()) {
            bool restLower = true;
            for (int i = 2; i < word.size(); ++i)
                restLower = restLower && !word.at(i).isUpper();
            if (restLower)
                fixed = word.left(1) + word.at(1).toLower() + word.mid(2);
        }
        if (s.capitalizeDays && dayNames(s.language).contains(word.toLower()) && word.at(0).isLower())
            fixed = capitalized(fixed);
        if (s.capitalizeSentences && fixed.at(0).isLower() && sentenceStart(before, start))
            fixed = capitalized(fixed);
    }
    if (fixed == word)
        return edit;
    edit.remove = int(word.size());
    edit.insert = fixed + typedText;
    return edit;
}

} // namespace vek::autocorrect

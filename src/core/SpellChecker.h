#pragma once

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <memory>

class Hunspell;

namespace vek {

// Rechtschreibprüfung über Hunspell mit den mitgelieferten Wörterbüchern (resources/dictionaries:
// de_CH, de_DE, en_US, fr_FR). Das Wörterbuch wird beim ersten Gebrauch geladen. Eigene Wörter
// landen in einer Benutzerdatei (ein Wort je Zeile), ignorierte gelten bis zum Programmende.
class SpellChecker {
public:
    static SpellChecker& instance();
    ~SpellChecker();

    // Ordner, in denen <sprache>.aff/.dic gesucht werden (in dieser Reihenfolge)
    void setSearchPaths(const QStringList& dirs);
    QStringList searchPaths() const { return m_dirs; }
    void setUserDictionary(const QString& path);
    QStringList availableLanguages() const;

    void setLanguage(const QString& code); // z.B. "de_CH"
    QString language() const { return m_language; }
    bool available() const; // Wörterbuch geladen

    // Zahlen, Wörter mit Ziffern, Adressen und einzelne Buchstaben gelten als richtig
    bool isCorrect(const QString& word) const;
    bool isIgnored(const QString& word) const { return m_ignored.contains(word); }
    QStringList suggestions(const QString& word, int max = 10) const;
    void addToUserDictionary(const QString& word);
    void ignore(const QString& word) { m_ignored.insert(word); }

    struct Range {
        int start = 0;
        int length = 0;
        bool ignored = false; // ignoriert, aber falsch geschrieben
    };
    // Falsch geschriebene Wörter im Text; mit includeIgnored auch die ignorierten
    QList<Range> check(const QString& text, bool includeIgnored = false) const;
    // Wortgrenzen um eine Position (für Kontextmenü und Dialog)
    static Range wordAt(const QString& text, int position);

private:
    SpellChecker();
    bool ensureLoaded() const;
    QByteArray encode(const QString& s) const;
    QString decode(const std::string& s) const;

    QStringList m_dirs;
    QString m_userPath;
    QString m_language = QStringLiteral("de_CH");
    mutable std::unique_ptr<Hunspell> m_hunspell;
    mutable QString m_loaded;   // geladene Sprache
    mutable bool m_failed = false;
    mutable bool m_latin1 = false;
    QSet<QString> m_ignored;
    QSet<QString> m_userWords;
};

} // namespace vek

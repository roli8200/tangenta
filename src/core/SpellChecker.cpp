#include "SpellChecker.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

#include <hunspell.hxx>

namespace vek {

namespace {

bool isWordChar(QChar c)
{
    return c.isLetterOrNumber() || c == QChar(0x2019) || c == QLatin1Char('\'') || c == QLatin1Char('-')
           || c.category() == QChar::Mark_NonSpacing;
}

} // namespace

SpellChecker& SpellChecker::instance()
{
    static SpellChecker checker;
    return checker;
}

SpellChecker::SpellChecker() = default;

SpellChecker::~SpellChecker() = default;

void SpellChecker::setSearchPaths(const QStringList& dirs)
{
    m_dirs = dirs;
    m_hunspell.reset();
    m_loaded.clear();
    m_failed = false;
}

void SpellChecker::setUserDictionary(const QString& path)
{
    m_userPath = path;
    m_userWords.clear();
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        in.setEncoding(QStringConverter::Utf8);
        while (!in.atEnd()) {
            const QString w = in.readLine().trimmed();
            if (!w.isEmpty())
                m_userWords.insert(w);
        }
    }
    if (m_hunspell)
        for (const QString& w : std::as_const(m_userWords))
            m_hunspell->add(encode(w).toStdString());
}

QStringList SpellChecker::availableLanguages() const
{
    QStringList result;
    for (const QString& dir : m_dirs)
        for (const QFileInfo& fi : QDir(dir).entryInfoList({QStringLiteral("*.dic")}, QDir::Files, QDir::Name))
            if (QFileInfo::exists(fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(".aff"))
                && !result.contains(fi.completeBaseName()))
                result << fi.completeBaseName();
    return result;
}

void SpellChecker::setLanguage(const QString& code)
{
    if (code == m_language)
        return;
    m_language = code;
    m_hunspell.reset();
    m_loaded.clear();
    m_failed = false;
}

bool SpellChecker::available() const
{
    return ensureLoaded();
}

bool SpellChecker::ensureLoaded() const
{
    if (m_hunspell && m_loaded == m_language)
        return true;
    if (m_failed)
        return false;
    for (const QString& dir : m_dirs) {
        const QString aff = QDir(dir).filePath(m_language + QStringLiteral(".aff"));
        const QString dic = QDir(dir).filePath(m_language + QStringLiteral(".dic"));
        if (!QFileInfo::exists(aff) || !QFileInfo::exists(dic))
            continue;
        // Hunspell nimmt UTF-8-Pfade (unter Windows mit eigener Umsetzung)
        m_hunspell = std::make_unique<Hunspell>(QDir::toNativeSeparators(aff).toUtf8().constData(),
                                                QDir::toNativeSeparators(dic).toUtf8().constData());
        const QString encoding = QString::fromLatin1(m_hunspell->get_dict_encoding().c_str()).toUpper();
        m_latin1 = encoding.startsWith(QLatin1String("ISO8859-1")) || encoding.startsWith(QLatin1String("ISO-8859-1"));
        m_loaded = m_language;
        for (const QString& w : m_userWords)
            m_hunspell->add(encode(w).toStdString());
        return true;
    }
    m_failed = true;
    return false;
}

QByteArray SpellChecker::encode(const QString& s) const
{
    // Typografischer Apostroph wie im Wörterbuch
    QString t = s;
    t.replace(QChar(0x2019), QLatin1Char('\''));
    return m_latin1 ? t.toLatin1() : t.toUtf8();
}

QString SpellChecker::decode(const std::string& s) const
{
    return m_latin1 ? QString::fromLatin1(s.c_str()) : QString::fromUtf8(s.c_str());
}

bool SpellChecker::isCorrect(const QString& word) const
{
    if (word.size() <= 1 || m_userWords.contains(word))
        return true;
    for (QChar c : word)
        if (c.isDigit())
            return true;
    if (word.contains(QLatin1Char('@')) || word.startsWith(QLatin1String("http")) || word.startsWith(QLatin1String("www.")))
        return true;
    if (!ensureLoaded())
        return true;
    if (m_latin1) {
        // Zeichen ausserhalb Latin-1 kennt das Wörterbuch nicht: nicht als Fehler melden
        for (QChar c : word)
            if (c.unicode() > 0xff && c != QChar(0x2019))
                return true;
    }
    if (m_hunspell->spell(encode(word).toStdString()))
        return true;
    // Bindestrich-Zusammensetzungen: jeder Teil für sich
    if (word.contains(QLatin1Char('-'))) {
        const QStringList parts = word.split(QLatin1Char('-'), Qt::SkipEmptyParts);
        bool all = !parts.isEmpty();
        for (const QString& part : parts)
            all = all && (part.size() <= 1 || m_hunspell->spell(encode(part).toStdString()));
        return all;
    }
    return false;
}

QStringList SpellChecker::suggestions(const QString& word, int max) const
{
    if (!ensureLoaded())
        return {};
    QStringList result;
    for (const std::string& s : m_hunspell->suggest(encode(word).toStdString())) {
        if (result.size() >= max)
            break;
        result << decode(s);
    }
    return result;
}

void SpellChecker::addToUserDictionary(const QString& word)
{
    if (word.isEmpty() || m_userWords.contains(word))
        return;
    m_userWords.insert(word);
    if (m_hunspell)
        m_hunspell->add(encode(word).toStdString());
    if (m_userPath.isEmpty())
        return;
    QDir().mkpath(QFileInfo(m_userPath).absolutePath());
    QFile f(m_userPath);
    if (f.open(QIODevice::Append | QIODevice::Text))
        f.write((word + QLatin1Char('\n')).toUtf8());
}

SpellChecker::Range SpellChecker::wordAt(const QString& text, int position)
{
    int start = std::clamp(position, 0, int(text.size()));
    int end = start;
    while (start > 0 && isWordChar(text.at(start - 1)))
        --start;
    while (end < text.size() && isWordChar(text.at(end)))
        ++end;
    // Bindestriche und Apostrophe am Rand gehören nicht zum Wort
    while (start < end && !text.at(start).isLetterOrNumber())
        ++start;
    while (end > start && !text.at(end - 1).isLetterOrNumber())
        --end;
    return {start, end - start, false};
}

QList<SpellChecker::Range> SpellChecker::check(const QString& text, bool includeIgnored) const
{
    QList<Range> result;
    int i = 0;
    while (i < text.size()) {
        if (!isWordChar(text.at(i))) {
            ++i;
            continue;
        }
        const Range w = wordAt(text, i);
        int next = i;
        while (next < text.size() && isWordChar(text.at(next)))
            ++next;
        if (w.length > 0) {
            const QString word = text.mid(w.start, w.length);
            if (!isCorrect(word)) {
                const bool ignored = m_ignored.contains(word);
                if (!ignored || includeIgnored)
                    result << Range{w.start, w.length, ignored};
            }
        }
        i = std::max(next, i + 1);
    }
    return result;
}

} // namespace vek

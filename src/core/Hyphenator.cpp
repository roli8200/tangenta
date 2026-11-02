#include "Hyphenator.h"

#include <QFile>

#include <algorithm>

namespace vek {

const Hyphenator& Hyphenator::german()
{
    static const Hyphenator instance = [] {
        QFile file(QStringLiteral(":/hyphenation/hyph-de-1996.pat"));
        QString text;
        if (file.open(QIODevice::ReadOnly))
            text = QString::fromUtf8(file.readAll());
        return Hyphenator(text);
    }();
    return instance;
}

Hyphenator::Hyphenator(const QString& patterns, int leftMin, int rightMin) : m_leftMin(leftMin), m_rightMin(rightMin)
{
    // Muster wie "1ba", ".ab3l", "n2st": Ziffern stehen zwischen den Buchstaben
    for (QStringView line : QStringView(patterns).split(QLatin1Char('\n'))) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('%')))
            continue;
        QString letters;
        QList<int> values{0};
        for (QChar c : line) {
            if (c.isDigit()) {
                values.last() = c.digitValue();
            } else {
                letters += c;
                values << 0;
            }
        }
        m_maxLength = std::max(m_maxLength, int(letters.size()));
        m_patterns.insert(letters, values);
    }
}

QList<int> Hyphenator::breakPoints(const QString& word) const
{
    QList<int> result;
    const int n = int(word.size());
    if (n < m_leftMin + m_rightMin || m_patterns.isEmpty())
        return result;
    const QString w = QLatin1Char('.') + word.toLower() + QLatin1Char('.');
    QList<int> points(w.size() + 1, 0);
    for (int i = 0; i < w.size(); ++i) {
        for (int len = 1; len <= m_maxLength && i + len <= w.size(); ++len) {
            const auto it = m_patterns.constFind(w.mid(i, len));
            if (it == m_patterns.constEnd())
                continue;
            const QList<int>& values = *it;
            for (int k = 0; k < values.size(); ++k)
                points[i + k] = std::max(points[i + k], values[k]);
        }
    }
    // points[i + 1] gehört zur Lücke vor word[i] (der Punkt am Anfang verschiebt um eins)
    for (int i = m_leftMin; i <= n - m_rightMin; ++i) {
        if (points[i + 1] % 2 == 1)
            result << i;
    }
    return result;
}

QString Hyphenator::hyphenated(const QString& word, QChar mark) const
{
    QString out = word;
    const QList<int> points = breakPoints(word);
    for (auto it = points.crbegin(); it != points.crend(); ++it)
        out.insert(*it, mark);
    return out;
}

QString Hyphenator::withSoftHyphens(const QString& text) const
{
    QString out;
    out.reserve(text.size() + text.size() / 4);
    int i = 0;
    while (i < text.size()) {
        if (!text[i].isLetter()) {
            out += text[i++];
            continue;
        }
        int j = i;
        while (j < text.size() && text[j].isLetter())
            ++j;
        out += hyphenated(text.mid(i, j - i), QChar(QChar::SoftHyphen));
        i = j;
    }
    return out;
}

} // namespace vek

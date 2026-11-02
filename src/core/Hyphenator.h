#pragma once

#include <QHash>
#include <QList>
#include <QString>

namespace vek {

// Silbentrennung nach Liang (TeX-Trennmuster), Deutsch nach reformierter Rechtschreibung
// (hyph-de-1996 aus hyph-utf8, MIT-Lizenz, eingebettet unter :/hyphenation).
class Hyphenator {
public:
    // Gemeinsame Instanz mit den deutschen Mustern
    static const Hyphenator& german();

    explicit Hyphenator(const QString& patterns, int leftMin = 2, int rightMin = 2);

    // Stellen, an denen getrennt werden darf: Index des Buchstabens, vor dem getrennt wird
    QList<int> breakPoints(const QString& word) const;
    // Wort mit Trennstrichen, z.B. "Sil-ben-tren-nung"
    QString hyphenated(const QString& word, QChar mark = QLatin1Char('-')) const;
    // Ganzen Text mit weichen Trennstrichen (U+00AD) versehen; nur Wörter aus Buchstaben
    QString withSoftHyphens(const QString& text) const;

private:
    QHash<QString, QList<int>> m_patterns; // Buchstabenfolge -> Werte zwischen den Zeichen
    int m_maxLength = 0;
    int m_leftMin;
    int m_rightMin;
};

} // namespace vek

#include "ExpressionSpinBox.h"

#include <QLocale>

#include <algorithm>

namespace vek {

namespace {

// Rekursiver Abstieg: Ausdruck = Term {(+|-) Term}, Term = Faktor {(*|/) Faktor},
// Faktor = [+|-] (Zahl | "(" Ausdruck ")")
class Parser {
public:
    explicit Parser(const QString& s) : m_s(s) {}

    std::optional<double> parse()
    {
        const std::optional<double> v = expression();
        skipSpace();
        if (!v || m_pos != m_s.size())
            return std::nullopt;
        return v;
    }

private:
    void skipSpace()
    {
        while (m_pos < m_s.size() && m_s[m_pos].isSpace())
            ++m_pos;
    }

    bool take(QChar c)
    {
        skipSpace();
        if (m_pos < m_s.size() && m_s[m_pos] == c) {
            ++m_pos;
            return true;
        }
        return false;
    }

    std::optional<double> expression()
    {
        std::optional<double> v = term();
        while (v) {
            if (take(QLatin1Char('+'))) {
                const std::optional<double> r = term();
                v = r ? std::optional(*v + *r) : std::nullopt;
            } else if (take(QLatin1Char('-'))) {
                const std::optional<double> r = term();
                v = r ? std::optional(*v - *r) : std::nullopt;
            } else {
                break;
            }
        }
        return v;
    }

    std::optional<double> term()
    {
        std::optional<double> v = factor();
        while (v) {
            if (take(QLatin1Char('*'))) {
                const std::optional<double> r = factor();
                v = r ? std::optional(*v * *r) : std::nullopt;
            } else if (take(QLatin1Char('/'))) {
                const std::optional<double> r = factor();
                v = r && *r != 0.0 ? std::optional(*v / *r) : std::nullopt;
            } else {
                break;
            }
        }
        return v;
    }

    std::optional<double> factor()
    {
        if (take(QLatin1Char('-'))) {
            const std::optional<double> v = factor();
            return v ? std::optional(-*v) : std::nullopt;
        }
        if (take(QLatin1Char('+')))
            return factor();
        if (take(QLatin1Char('('))) {
            const std::optional<double> v = expression();
            return v && take(QLatin1Char(')')) ? v : std::nullopt;
        }
        skipSpace();
        const qsizetype start = m_pos;
        while (m_pos < m_s.size() && (m_s[m_pos].isDigit() || m_s[m_pos] == QLatin1Char('.')))
            ++m_pos;
        bool ok = false;
        const double v = m_s.mid(start, m_pos - start).toDouble(&ok);
        return ok ? std::optional(v) : std::nullopt;
    }

    const QString& m_s;
    qsizetype m_pos = 0;
};

} // namespace

std::optional<double> evaluateExpression(QString text)
{
    text = text.trimmed();
    // Einheit am Ende übergehen; Dezimalkomma wie Punkt behandeln
    for (const char* unit : {"mm", "%", "°"}) {
        if (text.endsWith(QString::fromUtf8(unit)))
            text.chop(QString::fromUtf8(unit).size());
    }
    text.replace(QLatin1Char(','), QLatin1Char('.'));
    // Tausendertrennzeichen wie "1'000" weglassen
    text.remove(QLatin1Char('\''));
    if (text.trimmed().isEmpty())
        return std::nullopt;
    return Parser(text).parse();
}

QString ExpressionSpinBox::stripAffixes(QString text) const
{
    if (!suffix().isEmpty() && text.endsWith(suffix()))
        text.chop(suffix().size());
    if (!prefix().isEmpty() && text.startsWith(prefix()))
        text.remove(0, prefix().size());
    return text;
}

QValidator::State ExpressionSpinBox::validate(QString& input, int& pos) const
{
    Q_UNUSED(pos);
    const QString body = stripAffixes(input);
    for (const QChar c : body) {
        if (!(c.isDigit() || c.isSpace() || QStringLiteral("+-*/().,'mm%°").contains(c)))
            return QValidator::Invalid;
    }
    const std::optional<double> v = evaluateExpression(body);
    if (!v)
        return QValidator::Intermediate;
    return *v >= minimum() && *v <= maximum() ? QValidator::Acceptable : QValidator::Intermediate;
}

double ExpressionSpinBox::valueFromText(const QString& text) const
{
    return evaluateExpression(stripAffixes(text)).value_or(value());
}

void ExpressionSpinBox::fixup(QString& input) const
{
    // Unvollständig oder ausserhalb des Bereichs: auf den Bereich begrenzen bzw. alten Wert zeigen
    const std::optional<double> v = evaluateExpression(stripAffixes(input));
    const double fixed = v ? std::clamp(*v, minimum(), maximum()) : value();
    input = prefix() + locale().toString(fixed, 'f', decimals()) + suffix();
}

void ExpressionSpinBox::stepBy(int steps)
{
    QDoubleSpinBox::stepBy(steps);
    emit editingFinished();
}

} // namespace vek

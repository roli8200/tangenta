#include "ExpressionSpinBox.h"

#include "Options.h"

#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLocale>
#include <QSignalBlocker>

#include <algorithm>
#include <cmath>

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

std::optional<double> evaluateLength(QString text, int defaultUnit)
{
    text = text.trimmed();
    int unit = defaultUnit;
    // Einheit am Ende: Buchstaben bzw. " für Zoll
    qsizetype end = text.size();
    if (text.endsWith(QLatin1Char('"'))) {
        unit = 3;
        --end;
    } else {
        while (end > 0 && text[end - 1].isLetter())
            --end;
        const QString symbol = text.mid(end).toLower();
        if (!symbol.isEmpty()) {
            static const QList<std::pair<QString, int>> aliases = {
                {QStringLiteral("mm"), 0}, {QStringLiteral("cm"), 1}, {QStringLiteral("m"), 2},
                {QStringLiteral("in"), 3}, {QStringLiteral("inch"), 3}, {QStringLiteral("zoll"), 3},
                {QStringLiteral("pt"), 4}, {QStringLiteral("px"), 5}, {QStringLiteral("pc"), 6},
                {QStringLiteral("pica"), 6}, {QStringLiteral("km"), 7}, {QStringLiteral("ft"), 8}};
            unit = -2;
            for (const auto& [name, index] : aliases)
                if (name == symbol)
                    unit = index;
            if (unit == -2)
                return std::nullopt;
        }
    }
    const std::optional<double> v = evaluateExpression(text.left(end));
    if (!v)
        return std::nullopt;
    return *v * Options::unitInMm(unit < 0 ? 0 : unit);
}

QString ExpressionSpinBox::stripAffixes(QString text) const
{
    if (!suffix().isEmpty() && text.endsWith(suffix()))
        text.chop(suffix().size());
    if (!prefix().isEmpty() && text.startsWith(prefix()))
        text.remove(0, prefix().size());
    return text;
}

std::optional<double> ExpressionSpinBox::parse(const QString& text) const
{
    if (m_unit >= 0)
        return evaluateLength(text, m_unit);
    return evaluateExpression(stripAffixes(text));
}

QValidator::State ExpressionSpinBox::validate(QString& input, int& pos) const
{
    Q_UNUSED(pos);
    const QString body = m_unit >= 0 ? input : stripAffixes(input);
    for (const QChar c : body) {
        if (!(c.isDigit() || c.isSpace() || QStringLiteral("+-*/().,'mm%°").contains(c)
              || (m_unit >= 0 && (c.isLetter() || c == QLatin1Char('"')))))
            return QValidator::Invalid;
    }
    const std::optional<double> v = parse(input);
    if (!v)
        return QValidator::Intermediate;
    return *v >= minimum() && *v <= maximum() ? QValidator::Acceptable : QValidator::Intermediate;
}

double ExpressionSpinBox::valueFromText(const QString& text) const
{
    return parse(text).value_or(value());
}

void ExpressionSpinBox::fixup(QString& input) const
{
    // Unvollständig oder ausserhalb des Bereichs: auf den Bereich begrenzen bzw. alten Wert zeigen
    const std::optional<double> v = parse(input);
    const double fixed = v ? std::clamp(*v, minimum(), maximum()) : value();
    input = m_unit >= 0 ? textFromValue(fixed) : prefix() + locale().toString(fixed, 'f', decimals()) + suffix();
}

void ExpressionSpinBox::keyPressEvent(QKeyEvent* event)
{
    if (event->key() != Qt::Key_Return && event->key() != Qt::Key_Enter) {
        QDoubleSpinBox::keyPressEvent(event);
        return;
    }
    const QString shown = m_unit >= 0 ? textFromValue(value()) : prefix() + textFromValue(value()) + suffix();
    const bool pending = text() != shown;
    QDoubleSpinBox::keyPressEvent(event); // rechnet aus, meldet editingFinished, lässt die Taste weiter
    if (pending)
        event->accept();
}

QString ExpressionSpinBox::textFromValue(double value) const
{
    if (m_unit < 0)
        return QDoubleSpinBox::textFromValue(value);
    QString text = locale().toString(value / Options::unitInMm(m_unit), 'f', m_shownDecimals);
    if (!isGroupSeparatorShown())
        text.remove(locale().groupSeparator());
    return text + QLatin1Char(' ') + Options::unitSymbol(m_unit);
}

void ExpressionSpinBox::setLengthUnit(int unit)
{
    if (unit == m_unit)
        return;
    if (m_unit < 0) {
        // erstes Umschalten: mm-Einstellungen merken, Suffix übernimmt textFromValue
        m_mmDecimals = decimals();
        m_mmStep = singleStep();
        setSuffix(QString());
    } else if (m_unit == 0) {
        m_mmStep = singleStep(); // in mm gesetzte Schrittweite (Pfeilknöpfe) übernehmen
    }
    m_unit = unit;
    if (unit < 0) {
        setDecimals(m_mmDecimals);
        setSingleStep(m_mmStep);
        setSuffix(QStringLiteral(" mm"));
        setValue(value());
        return;
    }
    const double perMm = Options::unitInMm(unit);
    // gleiche Genauigkeit wie in mm, auf die Einheit umgerechnet
    m_shownDecimals = std::clamp(m_mmDecimals + int(std::lround(std::log10(perMm))), 0, 6);
    if (unit == 0)
        m_shownDecimals = m_mmDecimals;
    // intern genauer als angezeigt, damit "1 pt" nicht auf 0,35 mm gerundet wird
    const QSignalBlocker block(this);
    const double v = value();
    setDecimals(8);
    const double step = unit == 0 ? m_mmStep : std::pow(10.0, std::round(std::log10(m_mmStep / perMm))) * perMm;
    setSingleStep(step);
    setValue(v);
    lineEdit()->setText(textFromValue(value()));
}

void ExpressionSpinBox::followDocumentUnit()
{
    setLengthUnit(Options::instance().rulerUnit(true));
    connect(&Options::instance(), &Options::changed, this,
            [this] { setLengthUnit(Options::instance().rulerUnit(true)); });
}

void ExpressionSpinBox::stepBy(int steps)
{
    QDoubleSpinBox::stepBy(steps);
    emit editingFinished();
}

namespace {

class EnterAppliesFilter : public QObject {
public:
    using QObject::QObject;

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        const QEvent::Type type = event->type();
        if (type != QEvent::KeyPress && type != QEvent::FocusIn)
            return false;
        auto* spin = qobject_cast<QAbstractSpinBox*>(watched);
        if (!spin && qobject_cast<QLineEdit*>(watched))
            spin = qobject_cast<QAbstractSpinBox*>(watched->parent());
        // ExpressionSpinBox regelt das selbst
        if (!spin || qobject_cast<ExpressionSpinBox*>(spin))
            return false;
        static const char* const kCommitted = "vekCommittedText";
        if (type == QEvent::FocusIn) {
            spin->setProperty(kCommitted, spin->text());
            return false;
        }
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() != Qt::Key_Return && key->key() != Qt::Key_Enter)
            return false;
        const QVariant committed = spin->property(kCommitted);
        if (!committed.isValid() || committed.toString() == spin->text()) {
            spin->setProperty(kCommitted, spin->text());
            return false;
        }
        spin->interpretText();
        emit spin->editingFinished();
        spin->setProperty(kCommitted, spin->text());
        spin->selectAll();
        return true;
    }
};

} // namespace

void installEnterAppliesFilter()
{
    static QObject* filter = nullptr;
    if (filter || !qApp)
        return;
    filter = new EnterAppliesFilter(qApp);
    qApp->installEventFilter(filter);
}

} // namespace vek

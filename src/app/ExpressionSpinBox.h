#pragma once

#include <QDoubleSpinBox>

#include <optional>

namespace vek {

// Rechnet einen Ausdruck wie "297/2", "(210-20)*0,5" oder "-3+4 mm" aus: + - * /, Klammern,
// Vorzeichen, Dezimalpunkt oder -komma, eine angehängte Einheit wird übergangen.
std::optional<double> evaluateExpression(QString text);

// Zahlenfeld, in das man rechnen kann: Enter oder Verlassen rechnet aus
class ExpressionSpinBox : public QDoubleSpinBox {
    Q_OBJECT

public:
    using QDoubleSpinBox::QDoubleSpinBox;

    QValidator::State validate(QString& input, int& pos) const override;
    double valueFromText(const QString& text) const override;
    void fixup(QString& input) const override;
    // Pfeilknöpfe, Pfeiltasten und Mausrad: ein Schritt ist eine abgeschlossene Eingabe
    // und meldet editingFinished, damit er sofort wirkt
    void stepBy(int steps) override;

private:
    QString stripAffixes(QString text) const;
};

} // namespace vek

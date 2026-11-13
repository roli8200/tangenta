#pragma once

#include <QDoubleSpinBox>

#include <optional>

namespace vek {

// Rechnet einen Ausdruck wie "297/2", "(210-20)*0,5" oder "-3+4 mm" aus: + - * /, Klammern,
// Vorzeichen, Dezimalpunkt oder -komma, eine angehängte Einheit wird übergangen.
std::optional<double> evaluateExpression(QString text);
// Wie evaluateExpression, aber als Länge in mm: eine angehängte Einheit ("10 pt", "2in", "3 cm",
// auch 2" für Zoll) wird umgerechnet, ohne Einheit gilt defaultUnit (Index wie Options::unitNames)
std::optional<double> evaluateLength(QString text, int defaultUnit);

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
    QString textFromValue(double value) const override;
    // Eingabetaste mit geänderter Eingabe: Wert übernehmen, das Fenster bleibt offen (die Taste geht
    // nicht an den Standardknopf). Steht der Wert schon, wirkt sie wie gewohnt (z.B. OK im Dialog).
    void keyPressEvent(QKeyEvent* event) override;

    // Längenfeld: value() bleibt in mm, angezeigt und eingegeben wird in der Einheit unit
    // (Index wie Options::unitNames, -1 = aus). Eingaben mit Einheit ("10 pt") werden umgerechnet.
    void setLengthUnit(int unit);
    int lengthUnit() const { return m_unit; }
    // Längenfeld in der Dokumenteinheit (Optionen doc/rulers/units), folgt jeder Änderung
    void followDocumentUnit();

private:
    QString stripAffixes(QString text) const;
    std::optional<double> parse(const QString& text) const;

    int m_unit = -1;
    int m_mmDecimals = 2;   // Nachkommastellen in mm (vor dem Umschalten)
    double m_mmStep = 1.0;  // Schrittweite in mm (vor dem Umschalten)
    int m_shownDecimals = 2;
};

// Eingabetaste in allen übrigen Zahlenfeldern (QSpinBox usw.) wie im ExpressionSpinBox: Hat sich
// die Eingabe seit dem Betreten des Felds bzw. der letzten Übernahme geändert, übernimmt die Taste
// den Wert und das Fenster bleibt offen; erst die nächste wirkt wie gewohnt (OK im Dialog).
// Einmal für die ganze Anwendung.
void installEnterAppliesFilter();

} // namespace vek

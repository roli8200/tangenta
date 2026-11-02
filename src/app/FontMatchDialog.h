#pragma once

#include "FontMatch.h"

#include <QDialog>

#include <vector>

class QComboBox;
class QGridLayout;
class QLabel;
class QPushButton;

namespace vek {

// Schriftersetzung nach dem PDF-Import (wie "Schriftersetzung"): für jede nicht
// installierte Schrift die Vorschau aus den eingebetteten Umrissen, die ähnlichsten installierten
// Schriften mit Übereinstimmung in Prozent, "Als Kurven" (genau wie im PDF) und Links zu
// Google Fonts bzw. freien Gegenstücken. "Schriften neu einlesen" erkennt frisch installierte.
class FontMatchDialog : public QDialog {
    Q_OBJECT

public:
    explicit FontMatchDialog(std::vector<ImportedFont>& fonts, QWidget* parent = nullptr);

    // Gibt es überhaupt fehlende Schriften?
    static bool needed(const std::vector<ImportedFont>& fonts);
    // Gewählte Ersetzung anwenden (nach exec() == Accepted)
    void apply();
    fontmatch::Choice choice(int row) const;
    int rowCount() const { return int(m_rows.size()); }
    QComboBox* choiceBox(int row) const;

    // Vorschau eines Textes aus eingebetteten Umrissen bzw. einer installierten Schrift
    static QPixmap previewFromOutlines(const ImportedFont& font, const QString& text, int height);
    static QPixmap previewFromFamily(const QString& family, bool bold, bool italic, const QString& text, int height);

private:
    struct Row {
        ImportedFont* font = nullptr;
        QList<fontmatch::Candidate> candidates;
        QLabel* status = nullptr;
        QComboBox* choice = nullptr;
        QLabel* preview = nullptr;
    };
    void buildRow(int index, Row& row);
    void fillChoices(Row& row);
    void updatePreview(Row& row);
    void rescan();

    std::vector<ImportedFont>& m_fonts;
    std::vector<Row> m_rows;
    QGridLayout* m_grid = nullptr;
};

} // namespace vek

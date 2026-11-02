#pragma once

#include "Imposition.h"

#include <QDialog>
#include <QHash>
#include <QImage>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSpinBox;

namespace vek {

class Canvas;

// Vorschau eines ausgeschossenen Bogens: Seiten als Miniaturen mit Nummer (gedrehte Seiten mit
// Pfeil), Leerseiten schraffiert, Anschnitt, Schnitt- und Falzmarken
class ImpositionPreview : public QWidget {
    Q_OBJECT

public:
    explicit ImpositionPreview(Canvas& canvas, QWidget* parent = nullptr);
    void setResult(const impose::Result& result, const QList<int>& pages);
    void setSheet(int sheet, bool back);
    QSize sizeHint() const override { return {520, 520}; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    const QImage& thumbnail(int documentPage);

    Canvas& m_canvas;
    impose::Result m_result;
    QList<int> m_pages;
    int m_sheet = 0;
    bool m_back = false;
    QHash<int, QImage> m_thumbs;
};

// Datei > Ausschiessen: Anordnung, Papier, Wenden, Anschnitt, Marken; Vorschau je Bogen;
// Drucken (Duplex passend eingestellt) oder als PDF speichern
class ImpositionDialog : public QDialog {
    Q_OBJECT

public:
    explicit ImpositionDialog(Canvas& canvas, const QString& title, QWidget* parent = nullptr);

    impose::Settings settings() const;
    const impose::Result& result() const { return m_result; }
    const QList<int>& pages() const { return m_pages; }
    ImpositionPreview* preview() const { return m_preview; }
    QComboBox* layoutBox() const { return m_layout; }
    QComboBox* paperBox() const { return m_paper; }

    void update();

private:
    void print();
    void savePdf();
    void loadSettings();
    void storeSettings() const;

    Canvas& m_canvas;
    QString m_title;
    QComboBox* m_layout = nullptr;
    QComboBox* m_paper = nullptr;
    QDoubleSpinBox* m_paperW = nullptr;
    QDoubleSpinBox* m_paperH = nullptr;
    QSpinBox* m_from = nullptr;
    QSpinBox* m_to = nullptr;
    QComboBox* m_flip = nullptr;
    QSpinBox* m_columns = nullptr;
    QSpinBox* m_rows = nullptr;
    QDoubleSpinBox* m_gap = nullptr;
    QDoubleSpinBox* m_bleed = nullptr;
    QDoubleSpinBox* m_creep = nullptr;
    QComboBox* m_binding = nullptr;
    QCheckBox* m_cropMarks = nullptr;
    QCheckBox* m_foldMarks = nullptr;
    QCheckBox* m_info = nullptr;
    QLabel* m_summary = nullptr;
    QLabel* m_sheetLabel = nullptr;
    QPushButton* m_prev = nullptr;
    QPushButton* m_next = nullptr;
    QComboBox* m_side = nullptr;
    QPushButton* m_printButton = nullptr;
    QPushButton* m_pdfButton = nullptr;
    ImpositionPreview* m_preview = nullptr;
    impose::Result m_result;
    QList<int> m_pages;
    int m_sheet = 0;
    bool m_loading = false;
};

} // namespace vek

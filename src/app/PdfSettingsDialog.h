#pragma once

#include "PdfExport.h"
#include "PdfSettings.h"

#include <QDialog>
#include <QFileDialog>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QRadioButton;
class QSlider;
class QSpinBox;
class QTabWidget;
class QToolButton;

namespace vek {

class Canvas;

// "PDF-Einstellungen": Allgemein, Farbe, Dokument, Objekte, Druckvorstufe,
// Sicherheit und die Vorabkontrolle ("Keine Probleme" bzw. "n Probleme")
class PdfSettingsDialog : public QDialog {
    Q_OBJECT

public:
    PdfSettingsDialog(Canvas& canvas, const QString& path, const PdfSettings& settings, const QString& preset, QWidget* parent = nullptr);

    PdfSettings settings() const;
    QString presetName() const;
    QTabWidget* tabs() const { return m_tabs; }
    QComboBox* presetBox() const { return m_preset; }
    void setSettings(const PdfSettings& s);

private:
    void updatePreflight();
    void updateEnabled();
    QWidget* generalTab(const QString& path);
    QWidget* colorTab();
    QWidget* documentTab();
    QWidget* objectsTab();
    QWidget* prepressTab();
    QWidget* securityTab();
    QWidget* preflightTab();
    void accept() override;

    Canvas& m_canvas;
    QTabWidget* m_tabs = nullptr;
    bool m_loading = false;
    // Allgemein
    QRadioButton* m_rangeDocument = nullptr;
    QRadioButton* m_rangeSelection = nullptr;
    QRadioButton* m_rangeCurrent = nullptr;
    QRadioButton* m_rangePages = nullptr;
    QLineEdit* m_pages = nullptr;
    QRadioButton* m_sizeDocument = nullptr;
    QRadioButton* m_sizeSelection = nullptr;
    QComboBox* m_preset = nullptr;
    QToolButton* m_addPreset = nullptr;
    QToolButton* m_removePreset = nullptr;
    QComboBox* m_compatibility = nullptr;
    // Farbe
    QRadioButton* m_documentColors = nullptr;
    QRadioButton* m_proofColors = nullptr;
    QComboBox* m_colors = nullptr;
    QCheckBox* m_embedProfile = nullptr;
    QCheckBox* m_keepOverprint = nullptr;
    // Dokument
    QLineEdit* m_author = nullptr;
    QLineEdit* m_keywords = nullptr;
    QLineEdit* m_subject = nullptr;
    QCheckBox* m_fastWeb = nullptr;
    QCheckBox* m_bookmarks = nullptr;
    QCheckBox* m_thumbnails = nullptr;
    QButtonGroup* m_startView = nullptr;
    QRadioButton* m_ascii85 = nullptr;
    QRadioButton* m_binary = nullptr;
    // Objekte
    QComboBox* m_compression = nullptr;
    QSlider* m_quality = nullptr;
    QSpinBox* m_qualityValue = nullptr;
    QCheckBox* m_downColor = nullptr;
    QSpinBox* m_colorDpi = nullptr;
    QCheckBox* m_downGray = nullptr;
    QSpinBox* m_grayDpi = nullptr;
    QCheckBox* m_downMono = nullptr;
    QSpinBox* m_monoDpi = nullptr;
    QCheckBox* m_compressText = nullptr;
    QCheckBox* m_textAsCurves = nullptr;
    QCheckBox* m_embedFonts = nullptr;
    QCheckBox* m_embedBase14 = nullptr;
    QCheckBox* m_subset = nullptr;
    QSpinBox* m_subsetBelow = nullptr;
    QCheckBox* m_complexFills = nullptr;
    // Druckvorstufe
    QCheckBox* m_bleedLimit = nullptr;
    QDoubleSpinBox* m_bleed = nullptr;
    QCheckBox* m_cropMarks = nullptr;
    QCheckBox* m_registration = nullptr;
    QCheckBox* m_fileInfo = nullptr;
    QCheckBox* m_densitometer = nullptr;
    // Sicherheit
    QCheckBox* m_openPassword = nullptr;
    QLineEdit* m_openText = nullptr;
    QLineEdit* m_openRepeat = nullptr;
    QCheckBox* m_permPassword = nullptr;
    QLineEdit* m_permText = nullptr;
    QLineEdit* m_permRepeat = nullptr;
    QComboBox* m_printing = nullptr;
    QComboBox* m_changes = nullptr;
    QCheckBox* m_allowCopy = nullptr;
    // Vorabkontrolle
    QListWidget* m_issues = nullptr;
    QPlainTextEdit* m_issueText = nullptr;
    QCheckBox* m_ignorePreflight = nullptr;
    QList<pdfexport::Issue> m_found;
};

// "Als PDF freigeben": Dateiauswahl mit PDF-Voreinstellung und "Einstellungen …"
class PdfSaveDialog : public QFileDialog {
    Q_OBJECT

public:
    PdfSaveDialog(Canvas& canvas, const QString& suggested, QWidget* parent = nullptr);
    PdfSettings settings() const { return m_settings; }
    QString presetName() const;

private:
    Canvas& m_canvas;
    PdfSettings m_settings;
    QComboBox* m_preset = nullptr;
};

} // namespace vek

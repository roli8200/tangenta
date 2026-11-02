#include "PdfSettingsDialog.h"

#include "Canvas.h"
#include "Editor.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace vek {

namespace {

QGroupBox* group(const QString& title, QWidget* parent)
{
    return new QGroupBox(title, parent);
}

QSpinBox* dpiBox(QWidget* parent)
{
    auto* s = new QSpinBox(parent);
    s->setRange(9, 2400);
    s->setSuffix(QObject::tr(" dpi"));
    return s;
}

} // namespace

PdfSettingsDialog::PdfSettingsDialog(Canvas& canvas, const QString& path, const PdfSettings& settings, const QString& preset, QWidget* parent)
    : QDialog(parent), m_canvas(canvas)
{
    setWindowTitle(tr("PDF-Einstellungen"));
    auto* main = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(generalTab(path), tr("Allgemein"));
    m_tabs->addTab(colorTab(), tr("Farbe"));
    m_tabs->addTab(documentTab(), tr("Dokument"));
    m_tabs->addTab(objectsTab(), tr("Objekte"));
    m_tabs->addTab(prepressTab(), tr("Druckvorstufe"));
    m_tabs->addTab(securityTab(), tr("Sicherheit"));
    m_tabs->addTab(preflightTab(), tr("Keine Probleme"));
    main->addWidget(m_tabs);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Abbrechen"));
    connect(buttons, &QDialogButtonBox::accepted, this, &PdfSettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    main->addWidget(buttons);

    m_preset->addItems(PdfSettings::presets());
    m_preset->setCurrentText(preset);
    setSettings(settings);
    connect(m_preset, &QComboBox::currentTextChanged, this, [this](const QString& name) {
        if (!m_loading && PdfSettings::presets().contains(name))
            setSettings(PdfSettings::preset(name));
    });
    connect(m_addPreset, &QToolButton::clicked, this, [this] {
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("PDF-Voreinstellung speichern"), tr("Name:"), QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok || name.isEmpty())
            return;
        if (PdfSettings::isBuiltin(name)) {
            QMessageBox::information(this, tr("PDF-Voreinstellung"), tr("Die eingebauten Voreinstellungen lassen sich nicht überschreiben."));
            return;
        }
        PdfSettings::savePreset(name, this->settings());
        const QSignalBlocker block(m_preset);
        if (m_preset->findText(name) < 0)
            m_preset->addItem(name);
        m_preset->setCurrentText(name);
        updateEnabled();
    });
    connect(m_removePreset, &QToolButton::clicked, this, [this] {
        const QString name = m_preset->currentText();
        if (PdfSettings::isBuiltin(name))
            return;
        PdfSettings::removePreset(name);
        m_preset->removeItem(m_preset->currentIndex());
    });
    // Jede Änderung prüft neu
    for (QCheckBox* c : findChildren<QCheckBox*>())
        connect(c, &QCheckBox::toggled, this, [this] { updateEnabled(); updatePreflight(); });
    for (QRadioButton* r : findChildren<QRadioButton*>())
        connect(r, &QRadioButton::toggled, this, [this] { updateEnabled(); updatePreflight(); });
    for (QComboBox* c : findChildren<QComboBox*>())
        if (c != m_preset)
            connect(c, &QComboBox::currentIndexChanged, this, [this] { updateEnabled(); updatePreflight(); });
    for (QLineEdit* e : findChildren<QLineEdit*>())
        connect(e, &QLineEdit::textChanged, this, [this] { updatePreflight(); });
    updateEnabled();
    updatePreflight();
    resize(620, 480);
}

QWidget* PdfSettingsDialog::generalTab(const QString& path)
{
    auto* w = new QWidget(this);
    auto* v = new QVBoxLayout(w);
    auto* file = new QFormLayout;
    auto* name = new QLabel(QDir::toNativeSeparators(path), w);
    name->setTextInteractionFlags(Qt::TextSelectableByMouse);
    file->addRow(tr("Dateiname:"), name);
    v->addLayout(file);

    auto* range = group(tr("Exportbereich"), w);
    auto* g = new QGridLayout(range);
    m_rangeDocument = new QRadioButton(tr("Aktuelles Dokument"), range);
    m_rangeSelection = new QRadioButton(tr("Auswahl"), range);
    m_rangeCurrent = new QRadioButton(tr("Aktuelle Seite"), range);
    m_rangePages = new QRadioButton(tr("Seiten:"), range);
    m_pages = new QLineEdit(range);
    m_pages->setPlaceholderText(tr("z.B. 1-3, 5"));
    m_rangeSelection->setEnabled(!m_canvas.editor().selection().isEmpty());
    g->addWidget(m_rangeDocument, 0, 0);
    g->addWidget(m_rangeSelection, 1, 0);
    g->addWidget(m_rangeCurrent, 0, 1);
    auto* pagesRow = new QHBoxLayout;
    pagesRow->addWidget(m_rangePages);
    pagesRow->addWidget(m_pages);
    g->addLayout(pagesRow, 1, 1);
    v->addWidget(range);

    auto* size = group(tr("Seitengrösse"), w);
    auto* sv = new QVBoxLayout(size);
    m_sizeDocument = new QRadioButton(tr("Wie in TANGENTA definiert"), size);
    m_sizeSelection = new QRadioButton(tr("Durch die ausgewählten Objekte festgelegt"), size);
    sv->addWidget(m_sizeDocument);
    sv->addWidget(m_sizeSelection);
    v->addWidget(size);

    auto* form = new QFormLayout;
    auto* presetRow = new QHBoxLayout;
    m_preset = new QComboBox(w);
    m_preset->setMinimumWidth(220);
    m_addPreset = new QToolButton(w);
    m_addPreset->setText(QStringLiteral("+"));
    m_addPreset->setToolTip(tr("Einstellungen als eigene Voreinstellung speichern"));
    m_removePreset = new QToolButton(w);
    m_removePreset->setText(QStringLiteral("−"));
    m_removePreset->setToolTip(tr("Eigene Voreinstellung löschen"));
    presetRow->addWidget(m_preset, 1);
    presetRow->addWidget(m_addPreset);
    presetRow->addWidget(m_removePreset);
    form->addRow(tr("PDF-Voreinst.:"), presetRow);
    m_compatibility = new QComboBox(w);
    m_compatibility->addItems({tr("Acrobat 4.0 (PDF 1.3)"), tr("Acrobat 5.0 (PDF 1.4)"), tr("Acrobat 6.0 (PDF 1.5)"), tr("Acrobat 7.0 (PDF 1.6)"),
                               tr("Acrobat 8.0 (PDF 1.7)"), tr("Acrobat 9.0 (PDF 1.7)"), tr("PDF/X-1a"), tr("PDF/X-3"), tr("PDF/A-1b"), tr("PDF/A-2b")});
    form->addRow(tr("Kompatibilität:"), m_compatibility);
    v->addLayout(form);
    v->addStretch(1);
    return w;
}

QWidget* PdfSettingsDialog::colorTab()
{
    auto* w = new QWidget(this);
    auto* v = new QVBoxLayout(w);
    auto* manage = group(tr("Farbverwaltung"), w);
    auto* f = new QFormLayout(manage);
    m_documentColors = new QRadioButton(tr("Farbeinstellungen des Dokuments verwenden"), manage);
    m_proofColors = new QRadioButton(tr("Farbprüfeinstellungen verwenden"), manage);
    m_proofColors->setEnabled(false);
    m_proofColors->setToolTip(tr("TANGENTA hat noch keine Farbprüfung (Soft-Proof)."));
    f->addRow(m_documentColors);
    f->addRow(m_proofColors);
    m_colors = new QComboBox(manage);
    m_colors->addItems({tr("Programmeigen"), tr("RGB"), tr("CMYK"), tr("Graustufen")});
    m_colors->setToolTip(tr("Programmeigen: Farben bleiben, wie sie im Dokument stehen (RGB)"));
    f->addRow(tr("Farben ausgeben als:"), m_colors);
    m_embedProfile = new QCheckBox(tr("Farbprofil einbetten: sRGB IEC61966-2.1"), manage);
    f->addRow(m_embedProfile);
    v->addWidget(manage);
    auto* other = group(tr("Andere Farb-Steuerelemente"), w);
    auto* ov = new QVBoxLayout(other);
    m_keepOverprint = new QCheckBox(tr("Dokument-Überdruckung beibehalten"), other);
    auto* black = new QCheckBox(tr("Schwarz immer überdrucken"), other);
    black->setEnabled(false);
    black->setToolTip(tr("Noch nicht unterstützt: TANGENTA kennt Überdrucken erst mit der CMYK-Farbverwaltung."));
    ov->addWidget(m_keepOverprint);
    ov->addWidget(black);
    v->addWidget(other);
    v->addStretch(1);
    return w;
}

QWidget* PdfSettingsDialog::documentTab()
{
    auto* w = new QWidget(this);
    auto* v = new QVBoxLayout(w);
    auto* f = new QFormLayout;
    m_author = new QLineEdit(w);
    m_keywords = new QLineEdit(w);
    m_subject = new QLineEdit(w);
    f->addRow(tr("Autor:"), m_author);
    f->addRow(tr("Stichwörter:"), m_keywords);
    f->addRow(tr("Betreff:"), m_subject);
    v->addLayout(f);
    m_fastWeb = new QCheckBox(tr("Für schnelle Webanzeige optimieren"), w);
    v->addWidget(m_fastWeb);
    auto* row = new QHBoxLayout;
    auto* marks = group(tr("Lesezeichen"), w);
    auto* mv = new QVBoxLayout(marks);
    auto* links = new QCheckBox(tr("Verknüpfungen berücksichtigen"), marks);
    links->setEnabled(false);
    links->setToolTip(tr("TANGENTA-Dokumente enthalten noch keine Verknüpfungen (Hyperlinks)."));
    m_bookmarks = new QCheckBox(tr("Lesezeichen erstellen"), marks);
    m_bookmarks->setToolTip(tr("Ein Lesezeichen je Seite mit ihrem Namen"));
    m_thumbnails = new QCheckBox(tr("Skizzen erstellen"), marks);
    mv->addWidget(links);
    mv->addWidget(m_bookmarks);
    mv->addWidget(m_thumbnails);
    mv->addWidget(new QLabel(tr("Beim Start anzeigen:"), marks));
    m_startView = new QButtonGroup(this);
    int id = 0;
    for (const QString& text : {tr("Nur Seite"), tr("Vollbild"), tr("Lesezeichen"), tr("Skizzen")}) {
        auto* r = new QRadioButton(text, marks);
        m_startView->addButton(r, id++);
        mv->addWidget(r);
    }
    row->addWidget(marks);
    auto* coding = group(tr("Kodierung"), w);
    auto* cv = new QVBoxLayout(coding);
    m_ascii85 = new QRadioButton(tr("ASCII 85"), coding);
    m_binary = new QRadioButton(tr("Binär"), coding);
    cv->addWidget(m_ascii85);
    cv->addWidget(m_binary);
    cv->addStretch(1);
    row->addWidget(coding, 0, Qt::AlignTop);
    v->addLayout(row);
    v->addStretch(1);
    return w;
}

QWidget* PdfSettingsDialog::objectsTab()
{
    auto* w = new QWidget(this);
    auto* grid = new QGridLayout(w);
    auto* comp = group(tr("Bitmap-Komprimierung"), w);
    auto* cf = new QGridLayout(comp);
    m_compression = new QComboBox(comp);
    m_compression->addItems({tr("Keine"), tr("ZIP"), tr("JPEG")});
    cf->addWidget(new QLabel(tr("Komprimierungstyp:"), comp), 0, 0);
    cf->addWidget(m_compression, 0, 1, 1, 2);
    cf->addWidget(new QLabel(tr("JPEG-Qualität:"), comp), 1, 0);
    // Links hohe Qualität, rechts starke Komprimierung
    m_quality = new QSlider(Qt::Horizontal, comp);
    m_quality->setRange(0, 100);
    m_qualityValue = new QSpinBox(comp);
    m_qualityValue->setRange(0, 100);
    cf->addWidget(m_quality, 2, 0, 1, 2);
    cf->addWidget(m_qualityValue, 2, 2);
    cf->addWidget(new QLabel(tr("Hoch"), comp), 3, 0);
    cf->addWidget(new QLabel(tr("Niedrig"), comp), 3, 1, Qt::AlignRight);
    connect(m_quality, &QSlider::valueChanged, m_qualityValue, &QSpinBox::setValue);
    connect(m_qualityValue, &QSpinBox::valueChanged, m_quality, &QSlider::setValue);
    grid->addWidget(comp, 0, 0);

    auto* down = group(tr("Bitmap herunterskalieren"), w);
    auto* df = new QGridLayout(down);
    m_downColor = new QCheckBox(tr("Farbe"), down);
    m_downGray = new QCheckBox(tr("Graustufen"), down);
    m_downMono = new QCheckBox(tr("Monochrom"), down);
    m_colorDpi = dpiBox(down);
    m_grayDpi = dpiBox(down);
    m_monoDpi = dpiBox(down);
    df->addWidget(m_downColor, 0, 0);
    df->addWidget(m_colorDpi, 0, 1);
    df->addWidget(m_downGray, 1, 0);
    df->addWidget(m_grayDpi, 1, 1);
    df->addWidget(m_downMono, 2, 0);
    df->addWidget(m_monoDpi, 2, 1);
    grid->addWidget(down, 1, 0);

    auto* right = new QVBoxLayout;
    m_compressText = new QCheckBox(tr("Text und Strichgrafiken komprimieren"), w);
    m_textAsCurves = new QCheckBox(tr("Allen Text als Kurven exportieren"), w);
    right->addWidget(m_compressText);
    right->addWidget(m_textAsCurves);
    auto* fonts = group(tr("Text und Schriften"), w);
    auto* fv = new QVBoxLayout(fonts);
    m_embedFonts = new QCheckBox(tr("Schriften im Dokument einbetten"), fonts);
    m_embedBase14 = new QCheckBox(tr("Base 14-Schriften einbetten"), fonts);
    m_subset = new QCheckBox(tr("Schriftart-Teilmengen"), fonts);
    m_subsetBelow = new QSpinBox(fonts);
    m_subsetBelow->setRange(1, 100);
    m_subsetBelow->setSuffix(tr(" % des Zeichensatzes"));
    m_subsetBelow->setPrefix(tr("unter "));
    fv->addWidget(m_embedFonts);
    fv->addWidget(m_embedBase14);
    fv->addWidget(m_subset);
    fv->addWidget(m_subsetBelow);
    right->addWidget(fonts);
    right->addStretch(1);
    grid->addLayout(right, 0, 1, 2, 1);
    m_complexFills = new QCheckBox(tr("Komplexe Füllungen als Bitmaps wiedergeben"), w);
    m_complexFills->setToolTip(tr("Verläufe, Muster und Maschenfüllungen als Bild (300 dpi) – für ältere Belichter"));
    grid->addWidget(m_complexFills, 2, 0, 1, 2);
    grid->setRowStretch(3, 1);
    return w;
}

QWidget* PdfSettingsDialog::prepressTab()
{
    auto* w = new QWidget(this);
    auto* v = new QVBoxLayout(w);
    auto* row = new QHBoxLayout;
    m_bleedLimit = new QCheckBox(tr("Randanschnittgrenze:"), w);
    m_bleed = new QDoubleSpinBox(w);
    m_bleed->setRange(0, 50);
    m_bleed->setDecimals(3);
    m_bleed->setSuffix(tr(" mm"));
    row->addWidget(m_bleedLimit);
    row->addWidget(m_bleed);
    row->addStretch(1);
    v->addLayout(row);
    auto* marks = group(tr("Druckermarkierungen"), w);
    auto* g = new QGridLayout(marks);
    m_cropMarks = new QCheckBox(tr("Schneidemarken"), marks);
    m_registration = new QCheckBox(tr("Registriermarken"), marks);
    m_fileInfo = new QCheckBox(tr("Dateiinformationen"), marks);
    m_densitometer = new QCheckBox(tr("Densitometerskalen"), marks);
    g->addWidget(m_cropMarks, 0, 0);
    g->addWidget(m_registration, 0, 1);
    g->addWidget(m_fileInfo, 1, 0);
    g->addWidget(m_densitometer, 1, 1);
    v->addWidget(marks);
    auto* note = new QLabel(tr("Mit Anschnitt oder Marken wird die PDF-Seite um den Rand grösser; Endformat (TrimBox) und "
                               "Anschnitt (BleedBox) stehen im PDF, so dass die Druckerei richtig beschneidet."),
                            w);
    note->setWordWrap(true);
    v->addWidget(note);
    v->addStretch(1);
    return w;
}

QWidget* PdfSettingsDialog::securityTab()
{
    auto* w = new QWidget(this);
    auto* f = new QFormLayout(w);
    f->addRow(tr("Verschlüsselungsmethode:"), new QLabel(tr("Standard (RC4 128 Bit)"), w));
    m_openPassword = new QCheckBox(tr("Kennwort zum Öffnen des Dokuments"), w);
    f->addRow(m_openPassword);
    m_openText = new QLineEdit(w);
    m_openRepeat = new QLineEdit(w);
    m_permPassword = new QCheckBox(tr("Berechtigungskennwort"), w);
    m_permText = new QLineEdit(w);
    m_permRepeat = new QLineEdit(w);
    for (QLineEdit* e : {m_openText, m_openRepeat, m_permText, m_permRepeat})
        e->setEchoMode(QLineEdit::Password);
    f->addRow(tr("Kennwort:"), m_openText);
    f->addRow(tr("Kennwort zum Öffnen bestätigen:"), m_openRepeat);
    f->addRow(m_permPassword);
    f->addRow(tr("Kennwort:"), m_permText);
    f->addRow(tr("Berechtigungskennwort bestätigen:"), m_permRepeat);
    m_printing = new QComboBox(w);
    m_printing->addItems({tr("Keine"), tr("Niedrige Auflösung"), tr("Hohe Auflösung")});
    f->addRow(tr("Zulässiges Drucken:"), m_printing);
    m_changes = new QComboBox(w);
    m_changes->addItems({tr("Keine"), tr("Seiten einfügen, löschen und drehen"), tr("Formularfelder ausfüllen und unterschreiben"),
                         tr("Kommentieren, Formulare ausfüllen und unterschreiben"), tr("Alles ausser Seiten herausnehmen")});
    f->addRow(tr("Zulässige Änderungen:"), m_changes);
    m_allowCopy = new QCheckBox(tr("Kopieren von Text, Bildern und anderem Inhalt zulassen"), w);
    f->addRow(m_allowCopy);
    auto* note = new QLabel(tr("Die Berechtigungseinstellungen werden in Adobe Acrobat und den meisten PDF-Programmen beibehalten."), w);
    note->setWordWrap(true);
    f->addRow(note);
    return w;
}

QWidget* PdfSettingsDialog::preflightTab()
{
    auto* w = new QWidget(this);
    auto* v = new QVBoxLayout(w);
    m_issues = new QListWidget(w);
    m_issueText = new QPlainTextEdit(w);
    m_issueText->setReadOnly(true);
    v->addWidget(m_issues, 1);
    v->addWidget(m_issueText, 1);
    m_ignorePreflight = new QCheckBox(tr("Nicht mehr überprüfen"), w);
    v->addWidget(m_ignorePreflight);
    connect(m_issues, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < m_found.size())
            m_issueText->setPlainText(m_found[row].detail);
    });
    return w;
}

void PdfSettingsDialog::setSettings(const PdfSettings& s)
{
    m_loading = true;
    m_rangeDocument->setChecked(s.range == PdfSettings::Range::Document);
    m_rangeSelection->setChecked(s.range == PdfSettings::Range::Selection && m_rangeSelection->isEnabled());
    m_rangeCurrent->setChecked(s.range == PdfSettings::Range::CurrentPage);
    m_rangePages->setChecked(s.range == PdfSettings::Range::Pages);
    if (s.range == PdfSettings::Range::Selection && !m_rangeSelection->isEnabled())
        m_rangeDocument->setChecked(true);
    m_pages->setText(s.pages);
    m_sizeSelection->setChecked(s.sizeFromSelection);
    m_sizeDocument->setChecked(!s.sizeFromSelection);
    m_compatibility->setCurrentIndex(int(s.compatibility));
    m_documentColors->setChecked(true);
    m_colors->setCurrentIndex(int(s.colors));
    m_embedProfile->setChecked(s.embedProfile);
    m_keepOverprint->setChecked(s.keepOverprint);
    m_author->setText(s.author);
    m_keywords->setText(s.keywords);
    m_subject->setText(s.subject);
    m_fastWeb->setChecked(s.fastWebView);
    m_bookmarks->setChecked(s.bookmarks);
    m_thumbnails->setChecked(s.thumbnails);
    m_startView->button(int(s.startView))->setChecked(true);
    m_ascii85->setChecked(s.ascii85);
    m_binary->setChecked(!s.ascii85);
    m_compression->setCurrentIndex(int(s.compression));
    m_quality->setValue(100 - s.jpegQuality);
    m_downColor->setChecked(s.downsampleColor);
    m_colorDpi->setValue(s.colorDpi);
    m_downGray->setChecked(s.downsampleGray);
    m_grayDpi->setValue(s.grayDpi);
    m_downMono->setChecked(s.downsampleMono);
    m_monoDpi->setValue(s.monoDpi);
    m_compressText->setChecked(s.compressText);
    m_textAsCurves->setChecked(s.textAsCurves);
    m_embedFonts->setChecked(s.embedFonts);
    m_embedBase14->setChecked(s.embedBase14);
    m_subset->setChecked(s.subsetFonts);
    m_subsetBelow->setValue(s.subsetBelow);
    m_complexFills->setChecked(s.complexFillsAsBitmaps);
    m_bleedLimit->setChecked(s.bleedLimit);
    m_bleed->setValue(s.bleed);
    m_cropMarks->setChecked(s.cropMarks);
    m_registration->setChecked(s.registrationMarks);
    m_fileInfo->setChecked(s.fileInfo);
    m_densitometer->setChecked(s.densitometer);
    m_openPassword->setChecked(s.openPassword);
    m_openText->setText(s.openPasswordText);
    m_openRepeat->setText(s.openPasswordText);
    m_permPassword->setChecked(s.permissionPassword);
    m_permText->setText(s.permissionPasswordText);
    m_permRepeat->setText(s.permissionPasswordText);
    m_printing->setCurrentIndex(int(s.printing));
    m_changes->setCurrentIndex(int(s.changes));
    m_allowCopy->setChecked(s.allowCopy);
    m_ignorePreflight->setChecked(s.preflightIgnore);
    m_loading = false;
    updateEnabled();
    updatePreflight();
}

PdfSettings PdfSettingsDialog::settings() const
{
    PdfSettings s;
    s.range = m_rangeSelection->isChecked() ? PdfSettings::Range::Selection
              : m_rangeCurrent->isChecked() ? PdfSettings::Range::CurrentPage
              : m_rangePages->isChecked()   ? PdfSettings::Range::Pages
                                            : PdfSettings::Range::Document;
    s.pages = m_pages->text();
    s.sizeFromSelection = m_sizeSelection->isChecked();
    s.compatibility = PdfSettings::Compatibility(m_compatibility->currentIndex());
    s.colors = PdfSettings::Colors(m_colors->currentIndex());
    s.embedProfile = m_embedProfile->isChecked();
    s.keepOverprint = m_keepOverprint->isChecked();
    s.author = m_author->text();
    s.keywords = m_keywords->text();
    s.subject = m_subject->text();
    s.fastWebView = m_fastWeb->isChecked();
    s.bookmarks = m_bookmarks->isChecked();
    s.thumbnails = m_thumbnails->isChecked();
    s.startView = PdfSettings::StartView(std::max(0, m_startView->checkedId()));
    s.ascii85 = m_ascii85->isChecked();
    s.compression = PdfSettings::Compression(m_compression->currentIndex());
    s.jpegQuality = 100 - m_quality->value();
    s.downsampleColor = m_downColor->isChecked();
    s.colorDpi = m_colorDpi->value();
    s.downsampleGray = m_downGray->isChecked();
    s.grayDpi = m_grayDpi->value();
    s.downsampleMono = m_downMono->isChecked();
    s.monoDpi = m_monoDpi->value();
    s.compressText = m_compressText->isChecked();
    s.textAsCurves = m_textAsCurves->isChecked();
    s.embedFonts = m_embedFonts->isChecked();
    s.embedBase14 = m_embedBase14->isChecked();
    s.subsetFonts = m_subset->isChecked();
    s.subsetBelow = m_subsetBelow->value();
    s.complexFillsAsBitmaps = m_complexFills->isChecked();
    s.bleedLimit = m_bleedLimit->isChecked();
    s.bleed = m_bleed->value();
    s.cropMarks = m_cropMarks->isChecked();
    s.registrationMarks = m_registration->isChecked();
    s.fileInfo = m_fileInfo->isChecked();
    s.densitometer = m_densitometer->isChecked();
    s.openPassword = m_openPassword->isChecked();
    s.openPasswordText = m_openText->text();
    s.permissionPassword = m_permPassword->isChecked();
    s.permissionPasswordText = m_permText->text();
    s.printing = PdfSettings::Printing(m_printing->currentIndex());
    s.changes = PdfSettings::Changes(m_changes->currentIndex());
    s.allowCopy = m_allowCopy->isChecked();
    s.preflightIgnore = m_ignorePreflight->isChecked();
    return s;
}

QString PdfSettingsDialog::presetName() const
{
    return m_preset->currentText();
}

void PdfSettingsDialog::updateEnabled()
{
    m_pages->setEnabled(m_rangePages->isChecked());
    const bool selection = m_rangeSelection->isChecked();
    m_sizeDocument->setEnabled(selection);
    m_sizeSelection->setEnabled(selection);
    m_removePreset->setEnabled(!PdfSettings::isBuiltin(m_preset->currentText()));
    const bool jpeg = m_compression->currentIndex() == int(PdfSettings::Compression::Jpeg);
    m_quality->setEnabled(jpeg);
    m_qualityValue->setEnabled(jpeg);
    m_colorDpi->setEnabled(m_downColor->isChecked());
    m_grayDpi->setEnabled(m_downGray->isChecked());
    m_monoDpi->setEnabled(m_downMono->isChecked());
    const bool fonts = !m_textAsCurves->isChecked();
    m_embedFonts->setEnabled(fonts);
    m_embedBase14->setEnabled(fonts && m_embedFonts->isChecked());
    m_subset->setEnabled(fonts && m_embedFonts->isChecked());
    m_subsetBelow->setEnabled(fonts && m_embedFonts->isChecked() && m_subset->isChecked());
    m_bleed->setEnabled(m_bleedLimit->isChecked());
    const bool standard = m_compatibility->currentIndex() >= int(PdfSettings::Compatibility::PdfX1a);
    for (QWidget* x : std::initializer_list<QWidget*>{m_openPassword, m_permPassword})
        x->setEnabled(!standard);
    m_openText->setEnabled(!standard && m_openPassword->isChecked());
    m_openRepeat->setEnabled(m_openText->isEnabled());
    const bool perm = !standard && m_permPassword->isChecked();
    for (QWidget* x : std::initializer_list<QWidget*>{m_permText, m_permRepeat, m_printing, m_changes, m_allowCopy})
        x->setEnabled(perm);
}

void PdfSettingsDialog::updatePreflight()
{
    if (m_loading || !m_issues)
        return;
    m_found = pdfexport::preflight(m_canvas, settings());
    m_issues->clear();
    for (const pdfexport::Issue& i : std::as_const(m_found)) {
        const QStyle::StandardPixmap icon = i.severity == pdfexport::Issue::Severity::Error     ? QStyle::SP_MessageBoxCritical
                                            : i.severity == pdfexport::Issue::Severity::Warning ? QStyle::SP_MessageBoxWarning
                                                                                                : QStyle::SP_MessageBoxInformation;
        m_issues->addItem(new QListWidgetItem(style()->standardIcon(icon), i.title));
    }
    const int problems = pdfexport::problemCount(m_found);
    const int tab = m_tabs->count() - 1;
    if (problems == 0) {
        m_tabs->setTabText(tab, tr("Keine Probleme"));
        m_tabs->setTabIcon(tab, style()->standardIcon(QStyle::SP_DialogApplyButton));
        if (m_found.isEmpty()) {
            m_issues->addItem(tr("Derzeit liegen keine Probleme vor"));
            m_issueText->setPlainText(tr("Es gibt keine Probleme mit dem aktuellen Dokument oder den aktuellen Einstellungen. "
                                         "Überprüfen Sie dies vor dem Druck, der Ausgabe oder dem Senden des Dokuments an eine "
                                         "Drittperson erneut."));
        }
    } else {
        m_tabs->setTabText(tab, tr("%n Problem(e)", nullptr, problems));
        m_tabs->setTabIcon(tab, style()->standardIcon(QStyle::SP_MessageBoxWarning));
    }
    if (!m_found.isEmpty())
        m_issues->setCurrentRow(0);
}

void PdfSettingsDialog::accept()
{
    const PdfSettings s = settings();
    if (s.openPassword && m_openText->text() != m_openRepeat->text()) {
        m_tabs->setCurrentIndex(5);
        QMessageBox::warning(this, windowTitle(), tr("Die beiden Kennwörter zum Öffnen stimmen nicht überein."));
        return;
    }
    if (s.permissionPassword && m_permText->text() != m_permRepeat->text()) {
        m_tabs->setCurrentIndex(5);
        QMessageBox::warning(this, windowTitle(), tr("Die beiden Berechtigungskennwörter stimmen nicht überein."));
        return;
    }
    const bool errors = std::any_of(m_found.begin(), m_found.end(), [](const pdfexport::Issue& i) { return i.severity == pdfexport::Issue::Severity::Error; });
    if (errors) {
        m_tabs->setCurrentIndex(m_tabs->count() - 1);
        if (QMessageBox::question(this, windowTitle(), tr("Die Vorabkontrolle meldet Fehler. Trotzdem fortfahren?")) != QMessageBox::Yes)
            return;
    }
    PdfSettings::remember(s, presetName());
    QDialog::accept();
}

// --- Als PDF freigeben ------------------------------------------------------------------------

PdfSaveDialog::PdfSaveDialog(Canvas& canvas, const QString& suggested, QWidget* parent) : QFileDialog(parent), m_canvas(canvas)
{
    setWindowTitle(tr("Als PDF freigeben"));
    setOption(QFileDialog::DontUseNativeDialog, true); // nur so lassen sich Voreinstellung und Knopf einfügen
    setAcceptMode(QFileDialog::AcceptSave);
    setFileMode(QFileDialog::AnyFile);
    setNameFilter(tr("PDF - Portable Document Format (*.pdf)"));
    setDefaultSuffix(QStringLiteral("pdf"));
    selectFile(suggested);
    QString preset;
    m_settings = PdfSettings::last(&preset);
    m_preset = new QComboBox(this);
    m_preset->addItems(PdfSettings::presets());
    m_preset->setCurrentText(preset);
    auto* settingsButton = new QPushButton(tr("Einstellungen …"), this);
    if (auto* grid = qobject_cast<QGridLayout*>(layout())) {
        const int row = grid->rowCount();
        grid->addWidget(new QLabel(tr("PDF-Voreinst.:"), this), row, 0);
        grid->addWidget(m_preset, row, 1);
        grid->addWidget(settingsButton, row, 2);
    }
    connect(m_preset, &QComboBox::currentTextChanged, this, [this](const QString& name) { m_settings = PdfSettings::preset(name); });
    connect(settingsButton, &QPushButton::clicked, this, [this] {
        const QString file = selectedFiles().value(0);
        PdfSettingsDialog dialog(m_canvas, file, m_settings, m_preset->currentText(), this);
        if (dialog.exec() == QDialog::Accepted) {
            m_settings = dialog.settings();
            const QSignalBlocker block(m_preset);
            if (m_preset->findText(dialog.presetName()) < 0)
                m_preset->addItem(dialog.presetName());
            m_preset->setCurrentText(dialog.presetName());
        }
    });
}

QString PdfSaveDialog::presetName() const
{
    return m_preset->currentText();
}

} // namespace vek

#include "ImpositionDialog.h"

#include "Canvas.h"
#include "Document.h"
#include "Editor.h"
#include "Output.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

namespace vek {

namespace {

struct Paper {
    const char* name;
    double w, h;
};
// Gängige Bogenformate (Hochformat-Masse); SRA = mit Platz für Anschnitt und Marken
const Paper kPapers[] = {{"A4", 210, 297},    {"A3", 297, 420},      {"SRA3", 320, 450},  {"A5", 148, 210},
                         {"SRA4", 225, 320},  {"A2", 420, 594},      {"Letter", 215.9, 279.4}, {"Tabloid", 279.4, 431.8}};

} // namespace

// --- Vorschau ---------------------------------------------------------------------------------

ImpositionPreview::ImpositionPreview(Canvas& canvas, QWidget* parent) : QWidget(parent), m_canvas(canvas)
{
    setMinimumSize(360, 360);
}

void ImpositionPreview::setResult(const impose::Result& result, const QList<int>& pages)
{
    m_result = result;
    m_pages = pages;
    m_sheet = std::clamp(m_sheet, 0, std::max(0, int(result.sheets.size()) - 1));
    update();
}

void ImpositionPreview::setSheet(int sheet, bool back)
{
    m_sheet = sheet;
    m_back = back;
    update();
}

const QImage& ImpositionPreview::thumbnail(int documentPage)
{
    auto it = m_thumbs.find(documentPage);
    if (it == m_thumbs.end()) {
        // Miniatur mit 3 mm Rand für den Anschnitt
        const QSizeF size = m_canvas.editor().document().page(documentPage).size();
        const double pxPerMm = std::min(3.0, 600.0 / std::max(size.width(), size.height()));
        const double margin = 3.0;
        QImage img(QSize(int(std::ceil((size.width() + 2 * margin) * pxPerMm)), int(std::ceil((size.height() + 2 * margin) * pxPerMm))),
                   QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.translate(margin * pxPerMm, margin * pxPerMm);
        m_canvas.renderPage(p, documentPage, pxPerMm, QPointF(0, 0), true);
        p.end();
        img.setText(QStringLiteral("pxPerMm"), QString::number(pxPerMm));
        it = m_thumbs.insert(documentPage, img);
    }
    return *it;
}

void ImpositionPreview::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), palette().window());
    if (!m_result.ok() || m_sheet >= m_result.sheets.size()) {
        p.setPen(palette().text().color());
        p.drawText(rect().adjusted(20, 20, -20, -20), Qt::AlignCenter | Qt::TextWordWrap, m_result.error);
        return;
    }
    const impose::Sheet& sheet = m_result.sheets[m_sheet];
    const impose::Side& side = m_back && sheet.hasBack ? sheet.back : sheet.front;
    const QSizeF paper = m_result.sheet;
    const double scale = std::min((width() - 24) / paper.width(), (height() - 24) / paper.height());
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.translate((width() - paper.width() * scale) / 2, (height() - paper.height() * scale) / 2);
    p.fillRect(QRectF(QPointF(4, 4), paper * scale), QColor(0, 0, 0, 60));
    p.fillRect(QRectF(QPointF(0, 0), paper * scale), Qt::white);
    for (const impose::Placement& pl : side.pages) {
        const QRectF trim(pl.trim.topLeft() * scale, pl.trim.size() * scale);
        if (pl.page < 0 || pl.page >= m_pages.size()) {
            p.fillRect(trim, QBrush(QColor(200, 200, 200), Qt::BDiagPattern));
            p.setPen(QColor(120, 120, 120));
            p.drawText(trim, Qt::AlignCenter, tr("leer"));
            continue;
        }
        const int docPage = m_pages[pl.page];
        const QImage& img = thumbnail(docPage);
        const double pxPerMm = img.text(QStringLiteral("pxPerMm")).toDouble();
        p.save();
        const QRectF clip = QRectF(pl.trim.marginsAdded(pl.bleed).topLeft() * scale, pl.trim.marginsAdded(pl.bleed).size() * scale);
        p.setClipRect(clip);
        p.translate((pl.trim.center() + QPointF(pl.shift, 0)) * scale);
        p.rotate(pl.rotation);
        const double k = scale / pxPerMm;
        p.drawImage(QRectF(-img.width() * k / 2, -img.height() * k / 2, img.width() * k, img.height() * k), img);
        p.restore();
        // Seitennummer (Kopf oben bzw. unten)
        p.save();
        QFont f = font();
        f.setPixelSize(std::max(10, int(trim.height() * 0.12)));
        f.setBold(true);
        p.setFont(f);
        const QString label = QString::number(pl.page + 1) + (pl.rotation == 180 ? QStringLiteral(" ↓") : QString());
        const QRectF badge = QFontMetricsF(f).boundingRect(label).adjusted(-6, -2, 6, 2);
        const QRectF at(trim.center() - badge.center(), badge.size());
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 160, 0, 210));
        p.drawRoundedRect(at, 4, 4);
        p.setPen(Qt::black);
        p.drawText(at, Qt::AlignCenter, label);
        p.restore();
    }
    // Endformat dünn, Marken
    p.setPen(QPen(QColor(0, 120, 215, 120), 1, Qt::DotLine));
    p.setBrush(Qt::NoBrush);
    for (const impose::Placement& pl : side.pages)
        p.drawRect(QRectF(pl.trim.topLeft() * scale, pl.trim.size() * scale));
    p.setPen(QPen(Qt::black, 1));
    for (const QLineF& l : side.cropMarks)
        p.drawLine(QLineF(l.p1() * scale, l.p2() * scale));
    p.setPen(QPen(Qt::black, 1, Qt::DashLine));
    for (const QLineF& l : side.foldMarks)
        p.drawLine(QLineF(l.p1() * scale, l.p2() * scale));
}

// --- Dialog -----------------------------------------------------------------------------------

ImpositionDialog::ImpositionDialog(Canvas& canvas, const QString& title, QWidget* parent) : QDialog(parent), m_canvas(canvas), m_title(title)
{
    setWindowTitle(tr("Ausschiessen"));
    auto* main = new QHBoxLayout(this);
    auto* left = new QVBoxLayout;
    main->addLayout(left);
    auto* form = new QFormLayout;
    left->addLayout(form);

    m_layout = new QComboBox(this);
    for (impose::Layout l : {impose::Layout::Booklet, impose::Layout::Signature8, impose::Layout::NUp, impose::Layout::StepRepeat, impose::Layout::Single})
        m_layout->addItem(impose::layoutName(l), int(l));
    form->addRow(tr("Anordnung:"), m_layout);

    m_paper = new QComboBox(this);
    for (const Paper& paper : kPapers)
        m_paper->addItem(QStringLiteral("%1 (%2 x %3 mm)").arg(QLatin1String(paper.name)).arg(paper.w).arg(paper.h), QSizeF(paper.w, paper.h));
    m_paper->addItem(tr("Benutzerdefiniert"), QSizeF());
    form->addRow(tr("Papier:"), m_paper);
    auto* custom = new QHBoxLayout;
    m_paperW = new QDoubleSpinBox(this);
    m_paperH = new QDoubleSpinBox(this);
    for (QDoubleSpinBox* s : {m_paperW, m_paperH}) {
        s->setRange(20, 2000);
        s->setDecimals(1);
        s->setSuffix(tr(" mm"));
        custom->addWidget(s);
    }
    form->addRow(QString(), custom);

    const int pageCount = canvas.editor().document().pageCount();
    auto* range = new QHBoxLayout;
    m_from = new QSpinBox(this);
    m_to = new QSpinBox(this);
    for (QSpinBox* s : {m_from, m_to})
        s->setRange(1, pageCount);
    m_from->setValue(1);
    m_to->setValue(pageCount);
    range->addWidget(m_from);
    range->addWidget(new QLabel(tr("bis"), this));
    range->addWidget(m_to);
    form->addRow(tr("Seiten:"), range);

    m_flip = new QComboBox(this);
    m_flip->addItem(tr("an der kurzen Kante"), int(impose::Flip::ShortEdge));
    m_flip->addItem(tr("an der langen Kante"), int(impose::Flip::LongEdge));
    m_flip->setToolTip(tr("So wendet der Drucker das Blatt beim beidseitigen Druck. Beim Heft auf A4 quer: kurze Kante."));
    form->addRow(tr("Beidseitig wenden:"), m_flip);

    m_binding = new QComboBox(this);
    m_binding->addItem(tr("ineinander (Rückenheftung)"), true);
    m_binding->addItem(tr("hintereinander (Klebebindung)"), false);
    form->addRow(tr("Falzbogen:"), m_binding);

    auto* grid = new QHBoxLayout;
    m_columns = new QSpinBox(this);
    m_rows = new QSpinBox(this);
    for (QSpinBox* s : {m_columns, m_rows}) {
        s->setRange(0, 20);
        s->setSpecialValueText(tr("auto"));
        grid->addWidget(s);
    }
    form->addRow(tr("Spalten / Zeilen:"), grid);

    auto mm = [this](double max) {
        auto* s = new QDoubleSpinBox(this);
        s->setRange(0, max);
        s->setDecimals(1);
        s->setSingleStep(0.5);
        s->setSuffix(tr(" mm"));
        return s;
    };
    m_gap = mm(50);
    m_gap->setToolTip(tr("Abstand zwischen den Nutzen; beim Falzbogen der Kopfbeschnitt"));
    form->addRow(tr("Abstand:"), m_gap);
    m_bleed = mm(20);
    form->addRow(tr("Anschnitt:"), m_bleed);
    m_creep = mm(5);
    m_creep->setToolTip(tr("Bundzuwachs: Die Seiten des innersten Bogens rücken so viel zum Bund, damit sie nach dem Falzen "
                           "nicht über die äusseren hinausstehen"));
    form->addRow(tr("Bundzuwachs:"), m_creep);

    m_cropMarks = new QCheckBox(tr("Schnittmarken"), this);
    m_foldMarks = new QCheckBox(tr("Falzmarken"), this);
    m_info = new QCheckBox(tr("Dateiinformation"), this);
    auto* marks = new QVBoxLayout;
    for (QCheckBox* c : {m_cropMarks, m_foldMarks, m_info})
        marks->addWidget(c);
    form->addRow(tr("Marken:"), marks);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_summary->setMinimumWidth(320);
    left->addWidget(m_summary);
    left->addStretch(1);

    auto* buttons = new QDialogButtonBox(this);
    m_printButton = buttons->addButton(tr("Drucken …"), QDialogButtonBox::AcceptRole);
    m_pdfButton = buttons->addButton(tr("Als PDF speichern …"), QDialogButtonBox::ActionRole);
    buttons->addButton(tr("Schliessen"), QDialogButtonBox::RejectRole);
    left->addWidget(buttons);
    connect(m_printButton, &QPushButton::clicked, this, &ImpositionDialog::print);
    connect(m_pdfButton, &QPushButton::clicked, this, &ImpositionDialog::savePdf);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Vorschau mit Bogenwahl
    auto* right = new QVBoxLayout;
    main->addLayout(right, 1);
    auto* nav = new QHBoxLayout;
    m_prev = new QPushButton(QStringLiteral("◀"), this);
    m_next = new QPushButton(QStringLiteral("▶"), this);
    m_sheetLabel = new QLabel(this);
    m_side = new QComboBox(this);
    m_side->addItems({tr("Vorderseite"), tr("Rückseite")});
    nav->addWidget(m_prev);
    nav->addWidget(m_sheetLabel, 1, Qt::AlignCenter);
    nav->addWidget(m_next);
    nav->addWidget(m_side);
    right->addLayout(nav);
    m_preview = new ImpositionPreview(canvas, this);
    right->addWidget(m_preview, 1);
    connect(m_prev, &QPushButton::clicked, this, [this] {
        m_sheet = std::max(0, m_sheet - 1);
        update();
    });
    connect(m_next, &QPushButton::clicked, this, [this] {
        m_sheet = std::min(int(m_result.sheets.size()) - 1, m_sheet + 1);
        update();
    });
    connect(m_side, &QComboBox::currentIndexChanged, this, [this] { update(); });

    loadSettings();
    for (QComboBox* c : {m_layout, m_paper, m_flip, m_binding})
        connect(c, &QComboBox::currentIndexChanged, this, [this] { update(); });
    for (QSpinBox* s : {m_from, m_to, m_columns, m_rows})
        connect(s, &QSpinBox::valueChanged, this, [this] { update(); });
    for (QDoubleSpinBox* s : {m_paperW, m_paperH, m_gap, m_bleed, m_creep})
        connect(s, &QDoubleSpinBox::valueChanged, this, [this] { update(); });
    for (QCheckBox* c : {m_cropMarks, m_foldMarks, m_info})
        connect(c, &QCheckBox::toggled, this, [this] { update(); });
    update();
    resize(1000, 640);
}

impose::Settings ImpositionDialog::settings() const
{
    impose::Settings s;
    s.layout = impose::Layout(m_layout->currentData().toInt());
    const QSizeF paper = m_paper->currentData().toSizeF();
    s.paper = paper.isEmpty() ? QSizeF(m_paperW->value(), m_paperH->value()) : paper;
    s.flip = impose::Flip(m_flip->currentData().toInt());
    s.nested = m_binding->currentData().toBool();
    s.columns = m_columns->value();
    s.rows = m_rows->value();
    s.gap = m_gap->value();
    s.bleed = m_bleed->value();
    s.creep = m_creep->value();
    s.cropMarks = m_cropMarks->isChecked();
    s.foldMarks = m_foldMarks->isChecked();
    s.info = m_info->isChecked();
    return s;
}

void ImpositionDialog::update()
{
    if (m_loading)
        return;
    const impose::Settings s = settings();
    const bool custom = m_paper->currentData().toSizeF().isEmpty();
    m_paperW->setEnabled(custom);
    m_paperH->setEnabled(custom);
    const bool duplexLayout = s.layout == impose::Layout::Booklet || s.layout == impose::Layout::Signature8;
    m_flip->setEnabled(duplexLayout);
    m_creep->setEnabled(duplexLayout);
    m_binding->setEnabled(s.layout == impose::Layout::Signature8);
    m_columns->setEnabled(s.layout == impose::Layout::NUp || s.layout == impose::Layout::StepRepeat);
    m_rows->setEnabled(m_columns->isEnabled());
    m_pages.clear();
    for (int i = std::min(m_from->value(), m_to->value()); i <= std::max(m_from->value(), m_to->value()); ++i)
        m_pages << i - 1;
    const QSizeF page = m_canvas.editor().document().page(m_pages.value(0)).size();
    m_result = impose::arrange(s, int(m_pages.size()), page);
    m_sheet = std::clamp(m_sheet, 0, std::max(0, int(m_result.sheets.size()) - 1));
    QString text = impose::describe(m_result);
    if (m_result.ok() && s.layout == impose::Layout::Booklet)
        text += tr(".<br>Nach dem Drucken die Bogen in dieser Reihenfolge ineinanderlegen, in der Mitte falzen und heften.");
    if (m_result.ok() && s.layout == impose::Layout::Signature8)
        text += s.nested ? tr(".<br>Jeden Bogen zuerst quer (Köpfe aufeinander), dann längs falzen; Falzbogen ineinanderstecken, "
                              "heften und an Kopf und Vorderkante beschneiden.")
                         : tr(".<br>Jeden Bogen zuerst quer, dann längs falzen; Falzbogen der Reihe nach stapeln, binden und beschneiden.");
    if (!m_result.warning.isEmpty())
        text += QStringLiteral("<br><span style='color:#b35c00'>%1</span>").arg(m_result.warning);
    if (!m_result.ok())
        text = QStringLiteral("<span style='color:#b00020'>%1</span>").arg(m_result.error);
    m_summary->setText(text);
    const bool back = m_side->currentIndex() == 1;
    m_side->setEnabled(m_result.ok() && m_result.sheets.value(m_sheet).hasBack);
    m_sheetLabel->setText(m_result.ok() ? tr("Bogen %1 von %2").arg(m_sheet + 1).arg(m_result.sheets.size()) : QString());
    m_prev->setEnabled(m_sheet > 0);
    m_next->setEnabled(m_sheet + 1 < m_result.sheets.size());
    m_printButton->setEnabled(m_result.ok());
    m_pdfButton->setEnabled(m_result.ok());
    m_preview->setResult(m_result, m_pages);
    m_preview->setSheet(m_sheet, back);
    storeSettings();
}

void ImpositionDialog::print()
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(m_title);
    output::setupImposedPrinter(printer, settings(), m_result);
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(tr("Ausgeschossen drucken"));
    if (dialog.exec() != QDialog::Accepted)
        return;
    // Der Druckerdialog darf Papier und Duplex nicht verstellen: Bogen wie berechnet
    output::setupImposedPrinter(printer, settings(), m_result);
    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = output::printImposed(m_canvas, printer, m_result, m_pages, m_info->isChecked() ? m_title : QString(), &error);
    QApplication::restoreOverrideCursor();
    if (!ok)
        QMessageBox::warning(this, tr("Drucken"), error);
    else
        accept();
}

void ImpositionDialog::savePdf()
{
    QSettings settings;
    const QString dir = settings.value(QStringLiteral("files/exportDir"), QDir::homePath()).toString();
    const QString path = QFileDialog::getSaveFileName(this, tr("Ausgeschossen als PDF speichern"),
                                                      QDir(dir).filePath(m_title + tr(" – ausgeschossen.pdf")), tr("PDF (*.pdf)"));
    if (path.isEmpty())
        return;
    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = output::exportImposedPdf(m_canvas, path, m_result, m_pages, m_info->isChecked() ? m_title : QString(), &error);
    QApplication::restoreOverrideCursor();
    if (!ok)
        QMessageBox::warning(this, tr("Als PDF speichern"), error);
}

void ImpositionDialog::loadSettings()
{
    m_loading = true;
    QSettings st;
    st.beginGroup(QStringLiteral("impose"));
    m_layout->setCurrentIndex(std::max(0, m_layout->findData(st.value(QStringLiteral("layout"), int(impose::Layout::Booklet)).toInt())));
    m_paper->setCurrentIndex(std::clamp(st.value(QStringLiteral("paper"), 0).toInt(), 0, m_paper->count() - 1));
    m_paperW->setValue(st.value(QStringLiteral("paperW"), 320).toDouble());
    m_paperH->setValue(st.value(QStringLiteral("paperH"), 450).toDouble());
    m_flip->setCurrentIndex(st.value(QStringLiteral("flip"), 0).toInt());
    m_binding->setCurrentIndex(st.value(QStringLiteral("binding"), 0).toInt());
    m_columns->setValue(st.value(QStringLiteral("columns"), 0).toInt());
    m_rows->setValue(st.value(QStringLiteral("rows"), 0).toInt());
    m_gap->setValue(st.value(QStringLiteral("gap"), 0.0).toDouble());
    m_bleed->setValue(st.value(QStringLiteral("bleed"), 0.0).toDouble());
    m_creep->setValue(st.value(QStringLiteral("creep"), 0.0).toDouble());
    m_cropMarks->setChecked(st.value(QStringLiteral("cropMarks"), true).toBool());
    m_foldMarks->setChecked(st.value(QStringLiteral("foldMarks"), true).toBool());
    m_info->setChecked(st.value(QStringLiteral("info"), true).toBool());
    m_loading = false;
}

void ImpositionDialog::storeSettings() const
{
    QSettings st;
    st.beginGroup(QStringLiteral("impose"));
    st.setValue(QStringLiteral("layout"), m_layout->currentData().toInt());
    st.setValue(QStringLiteral("paper"), m_paper->currentIndex());
    st.setValue(QStringLiteral("paperW"), m_paperW->value());
    st.setValue(QStringLiteral("paperH"), m_paperH->value());
    st.setValue(QStringLiteral("flip"), m_flip->currentIndex());
    st.setValue(QStringLiteral("binding"), m_binding->currentIndex());
    st.setValue(QStringLiteral("columns"), m_columns->value());
    st.setValue(QStringLiteral("rows"), m_rows->value());
    st.setValue(QStringLiteral("gap"), m_gap->value());
    st.setValue(QStringLiteral("bleed"), m_bleed->value());
    st.setValue(QStringLiteral("creep"), m_creep->value());
    st.setValue(QStringLiteral("cropMarks"), m_cropMarks->isChecked());
    st.setValue(QStringLiteral("foldMarks"), m_foldMarks->isChecked());
    st.setValue(QStringLiteral("info"), m_info->isChecked());
}

} // namespace vek

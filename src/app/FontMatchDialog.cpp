#include "FontMatchDialog.h"

#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFontDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace vek {

namespace {

constexpr int kPreviewHeight = 34;
constexpr int kPreviewWidth = 260;
// Eintrag "Als Kurven" bzw. "Andere Schrift" im Auswahlfeld (sonst steht dort die Familie)
const QString kCurves = QStringLiteral("\x01curves");
const QString kOther = QStringLiteral("\x01other");

QString sampleText(const ImportedFont& font)
{
    QString s = font.sample.simplified();
    if (s.size() > 40)
        s = s.left(40) + QStringLiteral("…");
    return s.isEmpty() ? QStringLiteral("Hamburgefonstiv 123") : s;
}

} // namespace

bool FontMatchDialog::needed(const std::vector<ImportedFont>& fonts)
{
    return std::any_of(fonts.begin(), fonts.end(), [](const ImportedFont& f) { return f.installed.isEmpty() && !f.texts.isEmpty(); });
}

FontMatchDialog::FontMatchDialog(std::vector<ImportedFont>& fonts, QWidget* parent) : QDialog(parent), m_fonts(fonts)
{
    setWindowTitle(tr("Schriftersetzung"));
    auto* main = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("Diese Schriften aus dem PDF sind auf diesem Rechner nicht installiert. Wählen Sie je Schrift "
                                "eine ähnliche installierte Schrift (der Text bleibt bearbeitbar) oder <b>Als Kurven</b> – "
                                "dann sieht alles genau wie im PDF aus. Nach dem Installieren einer Schrift: "
                                "<b>Schriften neu einlesen</b>."),
                             this);
    intro->setWordWrap(true);
    main->addWidget(intro);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto* inner = new QWidget(scroll);
    m_grid = new QGridLayout(inner);
    m_grid->setColumnStretch(1, 1);
    m_grid->setColumnStretch(3, 1);
    int col = 0;
    for (const QString& head : {tr("Schrift im PDF"), tr("So im PDF"), tr("Ersetzen durch"), tr("So nach dem Import"), QString()}) {
        auto* l = new QLabel(QStringLiteral("<b>%1</b>").arg(head), inner);
        m_grid->addWidget(l, 0, col++);
    }
    scroll->setWidget(inner);
    main->addWidget(scroll, 1);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    for (ImportedFont& f : m_fonts)
        if (f.installed.isEmpty() && !f.texts.isEmpty()) {
            Row row;
            row.font = &f;
            row.candidates = fontmatch::suggest(f, 8);
            m_rows.push_back(row);
        }
    for (int i = 0; i < int(m_rows.size()); ++i)
        buildRow(i, m_rows[std::size_t(i)]);
    m_grid->setRowStretch(int(m_rows.size()) + 1, 1); // Zeilen oben zusammenhalten
    m_grid->setVerticalSpacing(10);
    QApplication::restoreOverrideCursor();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Importieren"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Abbrechen"));
    auto* rescanButton = buttons->addButton(tr("Schriften neu einlesen"), QDialogButtonBox::ActionRole);
    rescanButton->setToolTip(tr("Seit dem Start von TANGENTA installierte Schriften laden und erneut vergleichen"));
    connect(rescanButton, &QPushButton::clicked, this, &FontMatchDialog::rescan);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    main->addWidget(buttons);
    resize(1240, std::min(720, 200 + 56 * int(m_rows.size())));
}

void FontMatchDialog::buildRow(int index, Row& row)
{
    const int r = index + 1;
    QWidget* inner = m_grid->parentWidget();
    const ImportedFont& f = *row.font;
    QStringList style;
    if (f.bold)
        style << tr("fett");
    if (f.italic)
        style << tr("kursiv");
    row.status = new QLabel(inner);
    row.status->setText(QStringLiteral("<b>%1</b> %2<br><small>%3 · %4</small>")
                            .arg(f.family.toHtmlEscaped(), style.join(QLatin1Char(' ')), f.baseName.toHtmlEscaped(),
                                 f.embedded ? tr("eingebettet") : tr("nicht eingebettet")));
    m_grid->addWidget(row.status, r, 0);

    auto* original = new QLabel(inner);
    if (f.embedded)
        original->setPixmap(previewFromOutlines(f, sampleText(f), kPreviewHeight));
    else
        original->setText(tr("(Umrisse nicht im PDF)"));
    m_grid->addWidget(original, r, 1);

    row.choice = new QComboBox(inner);
    row.choice->setMinimumWidth(220);
    fillChoices(row);
    m_grid->addWidget(row.choice, r, 2);

    row.preview = new QLabel(inner);
    m_grid->addWidget(row.preview, r, 3);
    updatePreview(row);

    // Bezugsquellen: Google Fonts, freie Gegenstücke, Websuche
    auto* links = new QToolButton(inner);
    links->setText(tr("Schrift beschaffen"));
    links->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(links);
    if (const auto g = fontmatch::googleFont(f.family))
        menu->addAction(tr("%1 bei Google Fonts öffnen").arg(g->family), this, [url = g->url] { QDesktopServices::openUrl(url); });
    for (const fontmatch::WebFont& alt : fontmatch::freeAlternatives(f.family))
        menu->addAction(tr("Frei: %1 (%2)").arg(alt.family, alt.note), this, [url = alt.url] { QDesktopServices::openUrl(url); });
    if (!menu->isEmpty())
        menu->addSeparator();
    menu->addAction(tr("»%1« im Web suchen").arg(f.family), this, [url = fontmatch::searchUrl(f.family)] { QDesktopServices::openUrl(url); });
    links->setMenu(menu);
    if (fontmatch::googleFont(f.family))
        links->setToolTip(tr("Diese Schrift gibt es kostenlos bei Google Fonts"));
    m_grid->addWidget(links, r, 4);

    connect(row.choice, &QComboBox::activated, this, [this, index] {
        Row& row = m_rows[std::size_t(index)];
        if (row.choice->currentData().toString() == kOther) {
            bool ok = false;
            const QFont picked = QFontDialog::getFont(&ok, QFont(row.candidates.isEmpty() ? QStringLiteral("Arial") : row.candidates.first().family), this,
                                                      tr("Ersatzschrift für %1").arg(row.font->family));
            if (ok) {
                const int at = row.choice->count() - 1;
                row.choice->insertItem(at, picked.family(), picked.family());
                row.choice->setCurrentIndex(at);
            } else {
                row.choice->setCurrentIndex(0);
            }
        }
        updatePreview(row);
    });
}

void FontMatchDialog::fillChoices(Row& row)
{
    const ImportedFont& f = *row.font;
    row.choice->clear();
    if (!f.installed.isEmpty())
        row.choice->addItem(tr("%1 (installiert)").arg(f.installed), f.installed);
    if (f.embedded)
        row.choice->addItem(tr("Als Kurven – genau wie im PDF"), kCurves);
    for (const fontmatch::Candidate& c : std::as_const(row.candidates))
        row.choice->addItem(QStringLiteral("%1 – %2 %").arg(c.family).arg(int(std::lround(c.score * 100))), c.family);
    row.choice->addItem(tr("Andere Schrift …"), kOther);
    const fontmatch::Choice def = fontmatch::defaultChoice(f, row.candidates);
    const int at = row.choice->findData(def.kind == fontmatch::Choice::Kind::Curves ? kCurves : def.family);
    row.choice->setCurrentIndex(std::max(0, at));
}

void FontMatchDialog::updatePreview(Row& row)
{
    const ImportedFont& f = *row.font;
    const QString data = row.choice->currentData().toString();
    if (data == kCurves)
        row.preview->setPixmap(previewFromOutlines(f, sampleText(f), kPreviewHeight));
    else if (data != kOther)
        row.preview->setPixmap(previewFromFamily(data, f.bold, f.italic, sampleText(f), kPreviewHeight));
}

void FontMatchDialog::rescan()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    fontmatch::reloadInstalledFonts();
    for (Row& row : m_rows) {
        row.font->installed = fontmatch::installedFamily(row.font->family);
        row.candidates = fontmatch::suggest(*row.font, 8);
        fillChoices(row);
        updatePreview(row);
        if (!row.font->installed.isEmpty())
            row.status->setText(row.status->text() + tr("<br><span style='color:#2a7a2a'>jetzt installiert</span>"));
    }
    QApplication::restoreOverrideCursor();
}

fontmatch::Choice FontMatchDialog::choice(int row) const
{
    const QString data = m_rows[std::size_t(row)].choice->currentData().toString();
    if (data == kCurves)
        return {fontmatch::Choice::Kind::Curves, {}};
    return {fontmatch::Choice::Kind::Family, data == kOther ? QStringLiteral("Arial") : data};
}

QComboBox* FontMatchDialog::choiceBox(int row) const
{
    return m_rows[std::size_t(row)].choice;
}

void FontMatchDialog::apply()
{
    for (int i = 0; i < int(m_rows.size()); ++i)
        fontmatch::apply(*m_rows[std::size_t(i)].font, choice(i));
}

QPixmap FontMatchDialog::previewFromOutlines(const ImportedFont& font, const QString& text, int height)
{
    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    QPixmap pm(QSize(kPreviewWidth, height) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const double size = height * 0.72; // Geviert in Pixeln
    double x = 2;
    const double baseline = height * 0.78;
    for (char32_t ch : text.toUcs4()) {
        const QPainterPath g = font.glyphs.value(ch);
        p.fillPath(QTransform(size / 1000, 0, 0, -size / 1000, x, baseline).map(g), QApplication::palette().text());
        x += font.widths.value(ch, ch == U' ' ? 280 : 500) / 1000.0 * size;
        if (x > kPreviewWidth)
            break;
    }
    return pm;
}

QPixmap FontMatchDialog::previewFromFamily(const QString& family, bool bold, bool italic, const QString& text, int height)
{
    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    QPixmap pm(QSize(kPreviewWidth, height) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QFont font(family);
    font.setPixelSize(int(std::lround(height * 0.72)));
    font.setBold(bold);
    font.setItalic(italic);
    p.setFont(font);
    p.setPen(QApplication::palette().text().color());
    p.drawText(QPointF(2, height * 0.78), text);
    return pm;
}

} // namespace vek

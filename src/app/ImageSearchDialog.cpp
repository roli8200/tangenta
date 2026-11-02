#include "ImageSearchDialog.h"

#include "Options.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPointer>
#include <QDesktopServices>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QShortcut>
#include <QStyledItemDelegate>
#include <QToolTip>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTextLayout>
#include <QThread>
#include <QThreadPool>
#include <QVBoxLayout>

namespace vek {

using namespace imagesearch;

namespace {

constexpr int kTile = 140;

QIcon placeholder(const QString& text)
{
    QPixmap pm(kTile, kTile);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setPen(QColor(0x90, 0x90, 0x90));
    p.drawText(pm.rect(), Qt::AlignCenter | Qt::TextWordWrap, text);
    return QIcon(pm);
}

bool licensed(const QString& source)
{
    return source == QLatin1String("adobe") || source == QLatin1String("getty") || source == QLatin1String("istock");
}

constexpr int kPageRole = Qt::UserRole + 1;    // Seite beim Anbieter
constexpr int kTitleRole = Qt::UserRole + 2;   // Titel über dem Bild
constexpr int kLicenseRole = Qt::UserRole + 3; // Lizenz unter dem Bild
constexpr int kSourceRole = Qt::UserRole + 4;  // Archiv
constexpr int kStateRole = Qt::UserRole + 5;   // 0 frei, 1 Premium mit Abo, 2 lizenzpflichtig

// Kachel: Karte mit Titel oben, Bild, Lizenz und Archiv unten
constexpr int kPad = 8;
constexpr int kCardWidth = kTile + 2 * kPad + 4;
constexpr int kCardHeight = kTile + 112;

// Titel ohne Dateiendung (Commons-Titel sind Dateinamen)
QString displayTitle(const QString& title)
{
    static const QRegularExpression extension(QStringLiteral("\\.(jpe?g|png|tiff?|gif|webp|svg)$"),
                                              QRegularExpression::CaseInsensitiveOption);
    return title.simplified().remove(extension);
}

// Titel im Eintragstext (Tastatursuche, Barrierefreiheit): am Wortende gekürzt
QString shortTitle(const QString& title)
{
    QString t = displayTitle(title);
    constexpr int kMax = 46;
    if (t.length() <= kMax)
        return t;
    t.truncate(kMax);
    const int space = t.lastIndexOf(QLatin1Char(' '));
    if (space > kMax / 2)
        t.truncate(space);
    return t + QStringLiteral(" …");
}

// Globus: blaue Kugel mit hellem Gradnetz und weissem Rand (wie ein "Im Web öffnen"-Symbol)
void paintGlobe(QPainter& p, const QRectF& r, bool hover)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    // weicher Schatten und weisser Rand, damit er auf jedem Bild lesbar bleibt
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 50));
    p.drawEllipse(r.translated(0, 0.8));
    p.setBrush(Qt::white);
    p.drawEllipse(r);
    const QRectF g = r.adjusted(1.6, 1.6, -1.6, -1.6);
    QRadialGradient fill(g.topLeft() + QPointF(g.width() * 0.35, g.height() * 0.3), g.width() * 0.75);
    fill.setColorAt(0, hover ? QColor(0x6c, 0xb4, 0xff) : QColor(0x5a, 0xa6, 0xf0));
    fill.setColorAt(1, hover ? QColor(0x1d, 0x5f, 0xc4) : QColor(0x1a, 0x56, 0xa8));
    p.setBrush(fill);
    p.drawEllipse(g);
    p.setClipRegion(QRegion(g.toAlignedRect(), QRegion::Ellipse));
    p.setPen(QPen(QColor(255, 255, 255, 215), 0.9));
    p.setBrush(Qt::NoBrush);
    const QPointF c = g.center();
    p.drawEllipse(QRectF(c.x() - g.width() * 0.22, g.top(), g.width() * 0.44, g.height()));
    p.drawLine(QPointF(c.x(), g.top()), QPointF(c.x(), g.bottom()));
    p.drawLine(QPointF(g.left(), c.y()), QPointF(g.right(), c.y()));
    p.drawLine(QPointF(g.left(), g.top() + g.height() * 0.27), QPointF(g.right(), g.top() + g.height() * 0.27));
    p.drawLine(QPointF(g.left(), g.bottom() - g.height() * 0.27), QPointF(g.right(), g.bottom() - g.height() * 0.27));
    p.restore();
}

// Text auf höchstens zwei Zeilen umbrechen, die zweite bei Bedarf mit "…" gekürzt
QStringList twoLines(const QString& text, const QFont& font, int width)
{
    QStringList lines;
    const QFontMetrics fm(font);
    QTextLayout layout(text, font);
    layout.beginLayout();
    while (lines.size() < 2) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(width);
        QString part = text.mid(line.textStart(), line.textLength());
        if (lines.size() == 1 && line.textStart() + line.textLength() < text.length())
            part = fm.elidedText(text.mid(line.textStart()), Qt::ElideRight, width);
        else if (fm.horizontalAdvance(part.trimmed()) > width) // ein einzelnes überlanges Wort
            part = fm.elidedText(part, Qt::ElideRight, width);
        lines << part.trimmed();
    }
    layout.endLayout();
    return lines;
}

// Einfacher Bildbetrachter: Bild passt sich dem Fenster an
class ImageViewer : public QDialog {
public:
    explicit ImageViewer(QWidget* parent)
        : QDialog(parent)
    {
        auto* layout = new QVBoxLayout(this);
        picture = new QLabel(this);
        picture->setAlignment(Qt::AlignCenter);
        picture->setMinimumSize(200, 150);
        picture->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        picture->setStyleSheet(QStringLiteral("background: #202020;"));
        layout->addWidget(picture, 1);
        info = new QLabel(this);
        info->setWordWrap(true);
        info->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(info);
        auto* row = new QHBoxLayout;
        previous = new QPushButton(QStringLiteral("◀"), this);
        previous->setToolTip(QCoreApplication::translate("vek::ImageSearchDialog", "Voriges Bild (Pfeil links)"));
        next = new QPushButton(QStringLiteral("▶"), this);
        next->setToolTip(QCoreApplication::translate("vek::ImageSearchDialog", "Nächstes Bild (Pfeil rechts)"));
        row->addWidget(previous);
        row->addWidget(next);
        row->addStretch(1);
        browser = new QPushButton(QCoreApplication::translate("vek::ImageSearchDialog", "Im Browser öffnen"), this);
        insert = new QPushButton(QCoreApplication::translate("vek::ImageSearchDialog", "Einfügen"), this);
        auto* close = new QPushButton(QCoreApplication::translate("vek::ImageSearchDialog", "Schliessen"), this);
        row->addWidget(browser);
        row->addWidget(insert);
        row->addWidget(close);
        layout->addLayout(row);
        connect(close, &QPushButton::clicked, this, &QDialog::hide);
        resize(1000, 760);
    }

    void setImage(const QImage& image)
    {
        m_image = image;
        rescale();
    }
    const QImage& image() const { return m_image; }

    QLabel* picture = nullptr;
    QLabel* info = nullptr;
    QPushButton* previous = nullptr;
    QPushButton* next = nullptr;
    QPushButton* browser = nullptr;
    QPushButton* insert = nullptr;

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Left)
            previous->click();
        else if (event->key() == Qt::Key_Right)
            next->click();
        else
            QDialog::keyPressEvent(event);
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QDialog::resizeEvent(event);
        rescale();
    }

private:
    void rescale()
    {
        if (m_image.isNull()) {
            picture->setPixmap(QPixmap());
            return;
        }
        const qreal dpr = devicePixelRatioF();
        // Kleine Bilder (Symbole) nicht über das Doppelte aufblasen
        const QSize room = picture->size().boundedTo(m_image.size() * 2.0 / dpr) * dpr;
        QPixmap pm = QPixmap::fromImage(m_image.scaled(room, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        pm.setDevicePixelRatio(dpr);
        picture->setPixmap(pm);
    }

    QImage m_image;
};

} // namespace

// Kachel mit Globus oben rechts: Klick öffnet die Seite des Bildes beim Anbieter
class TileDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    std::function<void(const QUrl&)> open;
    mutable QHash<int, QRect> painted; // zuletzt gezeichnete Globen je Zeile (für Tests)

    QRect card(const QStyleOptionViewItem& option) const { return option.rect.adjusted(2, 2, -2, -2); }

    QRect image(const QStyleOptionViewItem& option) const
    {
        const QRect c = card(option);
        const int titleHeight = 2 * QFontMetrics(titleFont(option)).lineSpacing();
        return QRect(c.center().x() - kTile / 2 + 1, c.top() + kPad + titleHeight + 6, kTile, kTile);
    }

    QRect globe(const QStyleOptionViewItem& option, const QModelIndex&) const
    {
        const QRect img = image(option);
        return QRect(img.right() - 21, img.top() + 5, 16, 16);
    }

    static QFont titleFont(const QStyleOptionViewItem& option)
    {
        QFont f = option.font;
        f.setBold(true);
        return f;
    }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return {kCardWidth, kCardHeight}; }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const QPalette& pal = option.palette;
        const bool selected = option.state & QStyle::State_Selected;
        const bool hover = option.state & QStyle::State_MouseOver;
        const QRect c = card(option);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        // Karte
        QColor background = pal.color(QPalette::Base);
        if (selected)
            background = QColor::fromRgbF(0.5 * background.redF() + 0.5 * pal.color(QPalette::Highlight).lighter(170).redF(),
                                          0.5 * background.greenF() + 0.5 * pal.color(QPalette::Highlight).lighter(170).greenF(),
                                          0.5 * background.blueF() + 0.5 * pal.color(QPalette::Highlight).lighter(170).blueF());
        const QColor border = selected ? pal.color(QPalette::Highlight) : hover ? pal.color(QPalette::Mid) : pal.color(QPalette::Midlight).darker(112);
        painter->setPen(QPen(border, selected ? 2.0 : 1.0));
        painter->setBrush(background);
        painter->drawRoundedRect(QRectF(c).adjusted(0.5, 0.5, -0.5, -0.5), 7, 7);
        // Titel oben, zweizeilig
        const QFont bold = titleFont(option);
        const int textWidth = c.width() - 2 * kPad;
        painter->setFont(bold);
        painter->setPen(pal.color(QPalette::Text));
        const QStringList title = twoLines(index.data(kTitleRole).toString(), bold, textWidth);
        const int lineHeight = QFontMetrics(bold).lineSpacing();
        for (int i = 0; i < title.size(); ++i)
            painter->drawText(QRect(c.left() + kPad, c.top() + kPad + i * lineHeight, textWidth, lineHeight),
                              Qt::AlignHCenter | Qt::AlignVCenter, title.at(i));
        // Bild auf ruhigem Grund
        const QRect img = image(option);
        QPainterPath frame;
        frame.addRoundedRect(QRectF(img), 4, 4);
        painter->fillPath(frame, pal.color(QPalette::AlternateBase).darker(103));
        painter->save();
        painter->setClipPath(frame);
        const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
        icon.paint(painter, img, Qt::AlignCenter);
        painter->restore();
        // Lizenz und Archiv darunter
        QFont small = option.font;
        small.setPointSizeF(std::max(7.0, small.pointSizeF() - 1.0));
        const QFontMetrics fm(small);
        painter->setFont(small);
        const int state = index.data(kStateRole).toInt();
        const QColor licenseColor = state == 2 ? QColor(0xb3, 0x26, 0x1e) : state == 1 ? QColor(0xa8, 0x62, 0x00) : QColor(0x2e, 0x7d, 0x32);
        int y = img.bottom() + 7;
        painter->setPen(licenseColor);
        painter->drawText(QRect(c.left() + kPad, y, textWidth, fm.lineSpacing()), Qt::AlignHCenter,
                          fm.elidedText(index.data(kLicenseRole).toString(), Qt::ElideRight, textWidth));
        y += fm.lineSpacing();
        painter->setPen(pal.color(QPalette::PlaceholderText));
        painter->drawText(QRect(c.left() + kPad, y, textWidth, fm.lineSpacing()), Qt::AlignHCenter,
                          fm.elidedText(index.data(kSourceRole).toString(), Qt::ElideRight, textWidth));
        painter->restore();
        if (index.data(kPageRole).toUrl().isValid()) {
            const QRect r = globe(option, index);
            painted.insert(index.row(), r);
            const QPoint mouse = option.widget ? option.widget->mapFromGlobal(QCursor::pos()) : QPoint(-1, -1);
            paintGlobe(*painter, r, hover && r.adjusted(-2, -2, 2, 2).contains(mouse));
        }
    }

    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override
    {
        const QUrl page = index.data(kPageRole).toUrl();
        if (page.isValid() && (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::MouseButtonDblClick ||
                               event->type() == QEvent::MouseButtonPress)) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            if (globe(option, index).adjusted(-2, -2, 2, 2).contains(mouse->position().toPoint())) {
                if (event->type() == QEvent::MouseButtonRelease && mouse->button() == Qt::LeftButton && open)
                    open(page);
                return true; // weder auswählen noch Betrachter öffnen
            }
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

    bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option, const QModelIndex& index) override
    {
        if (index.data(kPageRole).toUrl().isValid() && globe(option, index).adjusted(-2, -2, 2, 2).contains(event->pos())) {
            QToolTip::showText(event->globalPos(), QCoreApplication::translate("vek::ImageSearchDialog", "Seite des Bildes im Browser öffnen"), view);
            return true;
        }
        return QStyledItemDelegate::helpEvent(event, view, option, index);
    }
};

ImageSearchDialog::ImageSearchDialog(QWidget* parent)
    : QDialog(parent)
    , m_search(new ImageSearch(this))
    , m_decoder(new QThreadPool(this))
{
    m_decoder->setMaxThreadCount(std::max(2, QThread::idealThreadCount() - 1));
    setWindowTitle(tr("Bildersuche"));
    m_search->setKeys(keysFromOptions());

    auto* layout = new QVBoxLayout(this);

    auto* top = new QHBoxLayout;
    m_term = new QLineEdit(this);
    m_term->setPlaceholderText(tr("Motiv beschreiben, z.B. „Serverschrank“ oder „monitor“"));
    m_term->setClearButtonEnabled(true);
    top->addWidget(m_term, 1);
    m_kind = new QComboBox(this);
    m_kind->addItems({tr("Fotos und Symbole"), tr("Nur Fotos"), tr("Nur Symbole")});
    m_kind->setCurrentIndex(std::clamp(Options::instance().integer(QStringLiteral("global/imageSearch/kind")), 0, 2));
    top->addWidget(m_kind);
    auto* go = new QPushButton(tr("Suchen"), this);
    go->setDefault(true);
    top->addWidget(go);
    layout->addLayout(top);

    // Quellen: ohne Schlüssel ausgegraut, der Hinweis sagt, woher er kommt
    auto* sourcesRow = new QGridLayout;
    int column = 0, line = 0;
    const QStringList ready = available(m_search->keys());
    for (const Source& s : sources()) {
        auto* box = new QCheckBox(s.name, this);
        box->setChecked(ready.contains(s.key) && !licensed(s.key));
        box->setProperty("ready", ready.contains(s.key));
        box->setToolTip(s.hint + (ready.contains(s.key) ? QString() : QStringLiteral("\n") + tr("Kein Schlüssel hinterlegt: Optionen > Global > Bildersuche.")));
        m_sourceBoxes.insert(s.key, box);
        sourcesRow->addWidget(box, line, column);
        if (++column == 6) {
            column = 0;
            ++line;
        }
    }
    layout->addLayout(sourcesRow);

    auto* options = new QHBoxLayout;
    m_onlyFree = new QCheckBox(tr("nur frei verwendbare"), this);
    m_onlyFree->setChecked(Options::instance().flag(QStringLiteral("global/imageSearch/onlyFree")));
    m_onlyFree->setToolTip(tr("Ohne Häkchen erscheinen auch Vorschauen von Getty, iStock, Adobe Stock und Premium-Treffer: "
                              "zum Finden eines Motivs, einfügen lassen sie sich nicht."));
    options->addWidget(m_onlyFree);
    options->addStretch(1);
    auto* keys = new QPushButton(tr("Schlüssel …"), this);
    keys->setToolTip(tr("API-Schlüssel der Archive (Optionen > Global > Bildersuche)"));
    options->addWidget(keys);
    layout->addLayout(options);

    m_complaints = new QLabel(this);
    m_complaints->setWordWrap(true);
    m_complaints->setStyleSheet(QStringLiteral("color: #a05a00;"));
    m_complaints->hide();
    layout->addWidget(m_complaints);

    // Anzeige eingrenzen: nur Treffer bestimmter Archive zeigen (ändert die Suche nicht)
    m_viewRow = new QWidget(this);
    auto* viewLayout = new QHBoxLayout(m_viewRow);
    viewLayout->setContentsMargins(0, 0, 0, 0);
    viewLayout->addWidget(new QLabel(tr("Anzeigen:"), m_viewRow));
    m_viewAll = new QCheckBox(tr("Alle"), m_viewRow);
    m_viewAll->setChecked(true);
    viewLayout->addWidget(m_viewAll);
    viewLayout->addStretch(1);
    m_viewRow->hide();
    layout->addWidget(m_viewRow);
    connect(m_viewAll, &QCheckBox::clicked, this, [this](bool on) {
        // Teilweise gewählt + Klick = alle
        if (m_viewAll->checkState() == Qt::PartiallyChecked)
            on = true;
        for (QCheckBox* box : std::as_const(m_viewBoxes)) {
            const QSignalBlocker block(box);
            box->setChecked(on);
        }
        updateViewAll();
        filter();
    });

    m_list = new QListWidget(this);
    m_list->setViewMode(QListView::IconMode);
    m_list->setIconSize(QSize(kTile, kTile));
    m_list->setGridSize(QSize(kCardWidth + 8, kCardHeight + 8));
    m_list->setSpacing(4);
    m_list->viewport()->setAttribute(Qt::WA_Hover);
    m_list->setMouseTracking(true);
    m_delegate = new TileDelegate(m_list);
    m_delegate->open = [this](const QUrl& url) { m_openUrl(url); };
    m_list->setItemDelegate(m_delegate);
    m_openUrl = [](const QUrl& url) { QDesktopServices::openUrl(url); };
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setWordWrap(true);
    m_list->setUniformItemSizes(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list, 1);

    m_details = new QLabel(this);
    m_details->setWordWrap(true);
    m_details->setTextFormat(Qt::RichText);
    m_details->setOpenExternalLinks(true);
    m_details->setMinimumHeight(48);
    layout->addWidget(m_details);

    auto* bottom = new QHBoxLayout;
    m_more = new QPushButton(tr("Mehr laden"), this);
    m_more->setEnabled(false);
    bottom->addWidget(m_more);
    bottom->addStretch(1);
    // Welche Fassung eingefügt wird: Original, kleinere Stufen, SVG/PNG bei Symbolen
    m_sizeLabel = new QLabel(tr("Auflösung:"), this);
    bottom->addWidget(m_sizeLabel);
    m_size = new QComboBox(this);
    m_size->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_size->setToolTip(tr("Welche Fassung eingefügt wird. Das Original ist die beste, aber auch die grösste Datei."));
    m_sizeLabel->setBuddy(m_size);
    bottom->addWidget(m_size);
    auto* buttons = new QDialogButtonBox(this);
    m_insert = buttons->addButton(tr("Einfügen"), QDialogButtonBox::AcceptRole);
    m_insert->setEnabled(false);
    buttons->addButton(QDialogButtonBox::Cancel);
    bottom->addWidget(buttons);
    layout->addLayout(bottom);

    connect(go, &QPushButton::clicked, this, [this] { search(false); });
    connect(m_term, &QLineEdit::returnPressed, this, [this] { search(false); });
    connect(m_more, &QPushButton::clicked, this, [this] { search(true); });
    connect(m_kind, &QComboBox::currentIndexChanged, this, [this](int kind) {
        Options::instance().set(QStringLiteral("global/imageSearch/kind"), kind);
        updateSources();
        filter();
    });
    connect(m_onlyFree, &QCheckBox::toggled, this, [this](bool on) {
        Options::instance().set(QStringLiteral("global/imageSearch/onlyFree"), on);
        updateSources();
        filter();
    });
    connect(keys, &QPushButton::clicked, this, [this] {
        emit optionsRequested();
        // Neue Schlüssel gleich benutzen
        m_search->setKeys(keysFromOptions());
        const QStringList now = available(m_search->keys());
        for (auto it = m_sourceBoxes.begin(); it != m_sourceBoxes.end(); ++it) {
            const bool ready = now.contains(it.key());
            if (ready && !it.value()->property("ready").toBool())
                it.value()->setChecked(!licensed(it.key()));
            it.value()->setProperty("ready", ready);
        }
        updateSources();
    });
    connect(m_list, &QListWidget::currentRowChanged, this, &ImageSearchDialog::updateDetails);
    // Unten angekommen: nächste Seite aller Quellen nachladen (wie auf den Plattformen)
    connect(m_list->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        if (value > 0 && value >= m_list->verticalScrollBar()->maximum() && m_more->isEnabled() && !m_busy)
            search(true);
    });
    // Doppelklick oder Leertaste: Bild gross ansehen (Einfügen dort oder mit dem Knopf)
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) { showViewer(m_list->row(item)); });
    auto* space = new QShortcut(QKeySequence(Qt::Key_Space), m_list, [this] { showViewer(m_list->currentRow()); });
    space->setContext(Qt::WidgetShortcut);
    connect(buttons, &QDialogButtonBox::accepted, this, &ImageSearchDialog::insertCurrent);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_search, &ImageSearch::arrived, this, [this](const QList<Hit>& hits) {
        showResults(hits, {}, !m_fresh);
        m_fresh = false;
    });
    connect(m_search, &ImageSearch::finished, this, [this](const QList<Hit>& hits, const QStringList& complaints) {
        m_list->viewport()->unsetCursor();
        m_busy = false;
        if (m_fresh) // keine Quelle hat geantwortet
            showResults({}, complaints);
        m_fresh = false;
        m_complaints->setText(complaints.join(QLatin1Char('\n')));
        m_complaints->setVisible(!complaints.isEmpty());
        m_more->setEnabled(!hits.isEmpty());
        if (visibleCount() == 0)
            showNoHits();
    });

    updateSources();
    updateDetails();
    resize(1000, 720);
}

ImageSearchDialog::~ImageSearchDialog()
{
    // Sauber beenden: keine Abrufe und keine Rückmeldungen mehr, Hintergrundarbeit abwarten
    ++m_generation;
    m_search->cancel();
    m_decoder->clear();
    m_decoder->waitForDone();
}

void ImageSearchDialog::updateSources()
{
    const int kind = m_kind->currentIndex();
    for (auto it = m_sourceBoxes.begin(); it != m_sourceBoxes.end(); ++it) {
        const Source* s = source(it.key());
        const bool kindOk = kind == 0 || (kind == 1) == (s->kind == Kind::Photo);
        const bool freeOk = !m_onlyFree->isChecked() || !licensed(it.key());
        it.value()->setEnabled(it.value()->property("ready").toBool() && kindOk && freeOk);
    }
}

QStringList ImageSearchDialog::checkedSources() const
{
    QStringList out;
    for (const Source& s : sources()) {
        const QCheckBox* box = m_sourceBoxes.value(s.key);
        if (box && box->isEnabled() && box->isChecked())
            out << s.key;
    }
    return out;
}

void ImageSearchDialog::setTerm(const QString& term)
{
    m_term->setText(term);
}

void ImageSearchDialog::search(bool more)
{
    const QString term = more ? m_lastTerm : m_term->text().trimmed();
    if (term.isEmpty())
        return;
    const QStringList wanted = checkedSources();
    if (wanted.isEmpty()) {
        showResults({}, {tr("Keine Quelle gewählt.")});
        return;
    }
    m_lastTerm = term;
    m_page = more ? m_page + 1 : 1;
    m_fresh = !more;
    // Nur im Dialog: eine abgelöste Suche oder ein geschlossener Dialog kann sie nicht stehen lassen
    m_list->viewport()->setCursor(Qt::BusyCursor);
    m_more->setEnabled(false);
    m_busy = true;
    m_search->search(term, wanted, m_page);
}

void ImageSearchDialog::showResults(const QList<Hit>& hits, const QStringList& complaints, bool append)
{
    if (!append) {
        m_hits.clear();
        m_list->clear();
        ++m_generation; // Vorschaubilder der alten Suche verwerfen
        m_delegate->painted.clear();
        qDeleteAll(m_viewBoxes);
        m_viewBoxes.clear();
        m_viewAll->setChecked(true);
    }
    m_complaints->setText(complaints.join(QLatin1Char('\n')));
    m_complaints->setVisible(!complaints.isEmpty());
    for (const Hit& h : hits) {
        const int index = int(m_hits.size());
        m_hits << h;
        const Source* s = source(h.source);
        const QString title = shortTitle(h.title);
        const QString label = (title.isEmpty() ? QString() : title + QLatin1Char('\n')) + (s ? s->name : h.source) + QLatin1Char('\n') +
                              (h.freeToUse ? h.license : h.subscription ? tr("Premium (Abo)") : tr("kostenpflichtig"));
        auto* item = new QListWidgetItem(placeholder(tr("lädt …")), label, m_list);
        item->setData(Qt::UserRole, index);
        item->setData(kTitleRole, displayTitle(h.title));
        item->setData(kLicenseRole, h.freeToUse ? h.license : h.subscription ? tr("Premium (Abo)") : tr("kostenpflichtig"));
        item->setData(kSourceRole, s ? s->name : h.source);
        item->setData(kStateRole, h.freeToUse ? 0 : h.subscription ? 1 : 2);
        if (h.page.isValid() && !h.page.isEmpty())
            item->setData(kPageRole, h.page);
        item->setToolTip(QStringLiteral("%1\n%2: %3\n%4: %5\n%6").arg(h.title, tr("Urheber"), h.author, tr("Lizenz"), h.license, h.condition));
        loadPreview(index);
    }
    updateViewBoxes();
    m_more->setEnabled(!hits.isEmpty() && !m_busy);
    filter();
    if (visibleCount() == 0 && !m_lastTerm.isEmpty())
        showNoHits();
}

void ImageSearchDialog::showNoHits()
{
    m_details->setText(tr("Keine Treffer.") +
                           (m_onlyFree->isChecked() ? QLatin1Char(' ') + tr("Vielleicht hilft es, „nur frei verwendbare“ abzuschalten.") : QString()) +
                           QLatin1Char(' ') + tr("Englische Begriffe bringen oft mehr Treffer."));
}

void ImageSearchDialog::loadPreview(int index, bool fallback)
{
    const QUrl url = fallback ? m_hits.at(index).fallbackPreview : m_hits.at(index).preview;
    if (!url.isValid() || url.isEmpty())
        return;
    const int generation = m_generation;
    m_search->fetch(url, [this, index, generation, fallback](const QByteArray& data) {
        if (generation != m_generation)
            return;
        const QUrl second = m_hits.at(index).fallbackPreview;
        if (data.isEmpty() && !fallback && second.isValid() && !second.isEmpty()) {
            loadPreview(index, true);
            return;
        }
        // Dekodieren und Verkleinern (Fotos bis 1600 Pixel) in einem Hintergrund-Thread
        QPointer<ImageSearchDialog> self(this);
        m_decoder->start([self, index, generation, data] {
            const QImage image = QImage::fromData(data);
            QImage tile;
            if (!image.isNull()) {
                tile = QImage(kTile, kTile, QImage::Format_ARGB32_Premultiplied);
                tile.fill(Qt::transparent);
                QPainter p(&tile);
                const QImage scaled = image.scaled(kTile, kTile, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                p.drawImage((kTile - scaled.width()) / 2, (kTile - scaled.height()) / 2, scaled);
            }
            if (!self)
                return;
            QMetaObject::invokeMethod(self.data(), [self, index, generation, tile] {
                if (self && generation == self->m_generation)
                    self->setPreview(index, tile);
            }, Qt::QueuedConnection);
        });
    });
}

void ImageSearchDialog::setPreview(int index, const QImage& tile)
{
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem* item = m_list->item(row);
        if (item->data(Qt::UserRole).toInt() == index)
            item->setIcon(tile.isNull() ? placeholder(tr("keine Vorschau")) : QIcon(QPixmap::fromImage(tile)));
    }
}

void ImageSearchDialog::updateViewBoxes()
{
    QHash<QString, int> counts;
    for (const Hit& h : std::as_const(m_hits))
        ++counts[h.source];
    auto* viewLayout = static_cast<QHBoxLayout*>(m_viewRow->layout());
    for (const Source& s : sources()) {
        if (!counts.contains(s.key))
            continue;
        QCheckBox* box = m_viewBoxes.value(s.key);
        if (!box) {
            box = new QCheckBox(m_viewRow);
            box->setChecked(m_viewAll->checkState() != Qt::Unchecked);
            connect(box, &QCheckBox::toggled, this, [this] {
                updateViewAll();
                filter();
            });
            // In der Reihenfolge der Quellen, nach "Anzeigen:" und "Alle"
            int position = 2;
            for (const Source& o : sources()) {
                if (o.key == s.key)
                    break;
                if (m_viewBoxes.contains(o.key))
                    ++position;
            }
            m_viewBoxes.insert(s.key, box);
            viewLayout->insertWidget(position, box);
        }
        box->setText(QStringLiteral("%1 (%2)").arg(s.name).arg(counts.value(s.key)));
    }
    m_viewRow->setVisible(!m_viewBoxes.isEmpty());
    updateViewAll();
}

void ImageSearchDialog::updateViewAll()
{
    int on = 0;
    for (const QCheckBox* box : std::as_const(m_viewBoxes))
        on += box->isChecked() ? 1 : 0;
    const int all = int(m_viewBoxes.size());
    const QSignalBlocker block(m_viewAll);
    m_viewAll->setTristate(on > 0 && on < all);
    m_viewAll->setCheckState(on == all ? Qt::Checked : on == 0 ? Qt::Unchecked : Qt::PartiallyChecked);
}

void ImageSearchDialog::showSources(const QStringList& shown)
{
    for (auto it = m_viewBoxes.begin(); it != m_viewBoxes.end(); ++it) {
        const QSignalBlocker block(it.value());
        it.value()->setChecked(shown.contains(it.key()));
    }
    updateViewAll();
    filter();
}

void ImageSearchDialog::filter()
{
    const int kind = m_kind->currentIndex();
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem* item = m_list->item(row);
        const Hit& h = m_hits.at(item->data(Qt::UserRole).toInt());
        const Source* s = source(h.source);
        const bool kindOk = kind == 0 || (kind == 1) == (s && s->kind == Kind::Photo);
        const QCheckBox* view = m_viewBoxes.value(h.source);
        const bool shown = !view || view->isChecked();
        item->setHidden(!kindOk || !shown || (m_onlyFree->isChecked() && !h.freeToUse));
    }
    updateDetails();
}

int ImageSearchDialog::visibleCount() const
{
    int n = 0;
    for (int row = 0; row < m_list->count(); ++row)
        n += m_list->item(row)->isHidden() ? 0 : 1;
    return n;
}

bool ImageSearchDialog::canInsert(int row) const
{
    if (row < 0 || row >= m_list->count() || m_list->item(row)->isHidden())
        return false;
    return insertable(m_hits.at(m_list->item(row)->data(Qt::UserRole).toInt()));
}

void ImageSearchDialog::updateDetails()
{
    const int row = m_list->currentRow();
    m_insert->setEnabled(canInsert(row));
    m_size->clear();
    m_size->setEnabled(canInsert(row));
    m_sizeLabel->setEnabled(canInsert(row));
    if (canInsert(row))
        for (const Size& s : choices(m_hits.at(m_list->item(row)->data(Qt::UserRole).toInt())))
            m_size->addItem(sizeLabel(s));
    if (row < 0 || m_list->item(row)->isHidden()) {
        m_details->setText(m_hits.isEmpty() ? tr("Beschreiben Sie das Motiv, nicht den Zweck. Zwei bis drei Wörter genügen.") : QString());
        return;
    }
    const Hit& h = m_hits.at(m_list->item(row)->data(Qt::UserRole).toInt());
    const Source* s = source(h.source);
    QString text = QStringLiteral("<b>%1</b><br>").arg(h.title.toHtmlEscaped());
    text += tr("%1 · Urheber: %2 · Lizenz: %3").arg((s ? s->name : h.source).toHtmlEscaped(), h.author.toHtmlEscaped(), h.license.toHtmlEscaped());
    if (h.width > 0)
        text += QStringLiteral(" · %1 × %2").arg(h.width).arg(h.height);
    if (h.vector)
        text += QStringLiteral(" · ") + tr("als Vektor (SVG)");
    text += QStringLiteral("<br>") + h.condition.toHtmlEscaped();
    if (!h.sizeNote.isEmpty())
        text += QLatin1Char(' ') + h.sizeNote.toHtmlEscaped();
    if (h.page.isValid() && !h.page.isEmpty())
        text += QStringLiteral(" <a href=\"%1\">%2</a>").arg(h.page.toString(QUrl::FullyEncoded).toHtmlEscaped(), tr("Seite beim Anbieter"));
    m_details->setText(text);
}

void ImageSearchDialog::setUrlOpener(std::function<void(const QUrl&)> open)
{
    m_openUrl = std::move(open);
}

QRect ImageSearchDialog::globeRect(int row) const
{
    return m_delegate->painted.value(row);
}

QDialog* ImageSearchDialog::showViewer(int row)
{
    if (row < 0 || row >= m_list->count() || m_list->item(row)->isHidden())
        return nullptr;
    auto* viewer = static_cast<ImageViewer*>(m_viewer);
    if (!viewer) {
        viewer = new ImageViewer(this);
        m_viewer = viewer;
        // Nächstes/voriges sichtbares Bild
        auto step = [this](int direction) {
            for (int r = m_viewerRow + direction; r >= 0 && r < m_list->count(); r += direction)
                if (!m_list->item(r)->isHidden()) {
                    m_list->setCurrentRow(r);
                    showViewer(r);
                    return;
                }
        };
        connect(viewer->previous, &QPushButton::clicked, this, [step] { step(-1); });
        connect(viewer->next, &QPushButton::clicked, this, [step] { step(1); });
        connect(viewer->browser, &QPushButton::clicked, this, [this] {
            const QUrl page = m_hits.at(m_list->item(m_viewerRow)->data(Qt::UserRole).toInt()).page;
            if (page.isValid())
                m_openUrl(page);
        });
        connect(viewer->insert, &QPushButton::clicked, this, [this, viewer] {
            viewer->hide();
            m_list->setCurrentRow(m_viewerRow);
            insertCurrent();
        });
    }
    m_viewerRow = row;
    QListWidgetItem* item = m_list->item(row);
    const Hit& h = m_hits.at(item->data(Qt::UserRole).toInt());
    const Source* s = source(h.source);
    viewer->setWindowTitle(h.title.isEmpty() ? tr("Bild") : h.title.left(120));
    QString info = QStringLiteral("<b>%1</b><br>").arg(h.title.toHtmlEscaped());
    info += tr("%1 · Urheber: %2 · Lizenz: %3").arg((s ? s->name : h.source).toHtmlEscaped(), h.author.toHtmlEscaped(), h.license.toHtmlEscaped());
    if (h.width > 0)
        info += QStringLiteral(" · %1 × %2").arg(h.width).arg(h.height);
    viewer->info->setText(info);
    viewer->browser->setEnabled(h.page.isValid() && !h.page.isEmpty());
    viewer->insert->setEnabled(canInsert(row));
    // Sofort die Kachel zeigen, dann das grössere Bild nachladen
    viewer->setImage(item->icon().pixmap(kTile, kTile).toImage());
    const int token = ++m_viewerToken;
    QPointer<ImageViewer> target(viewer);
    m_search->fetch(viewUrl(h), [this, target, token](const QByteArray& data) {
        if (!target || token != m_viewerToken)
            return;
        const QImage image = QImage::fromData(data);
        if (!image.isNull())
            target->setImage(image);
    });
    viewer->show();
    viewer->raise();
    return viewer;
}

void ImageSearchDialog::insertCurrent()
{
    const int row = m_list->currentRow();
    if (!canInsert(row))
        return;
    const Hit hit = m_hits.at(m_list->item(row)->data(Qt::UserRole).toInt());
    setCursor(Qt::WaitCursor);
    m_insert->setEnabled(false);
    const int size = std::max(0, m_size->currentIndex());
    m_search->download(hit, [this, hit](const QByteArray& data, const QString& error) {
        unsetCursor();
        m_insert->setEnabled(true);
        if (data.isEmpty()) {
            m_complaints->setText(error.isEmpty() ? tr("Das Bild liess sich nicht herunterladen.") : error);
            m_complaints->show();
            return;
        }
        m_hit = hit;
        m_data = data;
        accept();
    }, size);
}

} // namespace vek

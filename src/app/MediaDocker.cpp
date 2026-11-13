#include "MediaDocker.h"

#include "ArtisticMediaTool.h"
#include "Canvas.h"
#include "Editor.h"
#include "PathOps.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace vek {

QIcon mediaStrokeIcon(const ArtisticMedia& media, const QSize& size, qreal dpr)
{
    QPixmap pix(size * dpr);
    pix.setDevicePixelRatio(dpr);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    const double w = size.width(), h = size.height();
    QPainterPath spine(QPointF(w * 0.06, h * 0.55));
    spine.cubicTo(QPointF(w * 0.35, h * 0.05), QPointF(w * 0.65, h * 0.95), QPointF(w * 0.94, h * 0.45));
    ArtisticMedia sample = media;
    sample.pressure.clear();
    sample.seed = 7;
    if (sample.kind == ArtisticMedia::Kind::Sprayer) {
        sample.width = h * 0.42;
        sample.spacing = sample.width * 0.9;
        sample.dabCount = std::min(sample.dabCount, 3);
    } else {
        sample.width = h * 0.5;
    }
    p.setPen(Qt::NoPen);
    if (sample.hasPieces()) {
        for (const ArtPiece& piece : pathops::mediaPieces(spine, sample))
            p.fillPath(piece.path, piece.color.toQColor());
    } else {
        if (sample.kind == ArtisticMedia::Kind::Pressure)
            sample.pressure = {0.3, 1.0, 0.6, 0.2};
        p.setBrush(QColor(0x30, 0x30, 0x30));
        p.drawPath(pathops::mediaOutline(spine, sample));
    }
    p.end();
    return QIcon(pix);
}

MediaDocker::MediaDocker(Editor& editor, Canvas& canvas, QWidget* parent)
    : QWidget(parent), m_editor(editor), m_canvas(canvas)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    m_category = new QComboBox(this);
    m_category->setObjectName(QStringLiteral("mediaCategory"));
    m_category->addItems({tr("Zuletzt verwendet"), tr("Voreinstellungen"), tr("Pinsel"), tr("Sprühlisten"), tr("Eigene Striche")});
    m_category->setCurrentIndex(int(Category::Presets));
    layout->addWidget(m_category);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("mediaList"));
    m_list->setIconSize(QSize(150, 34));
    m_list->setSpacing(2);
    m_list->setUniformItemSizes(true);
    m_list->setToolTip(tr("Klick wendet den Strich auf die ausgewählten Objekte an"));
    layout->addWidget(m_list, 1);

    auto* row = new QHBoxLayout;
    m_apply = new QPushButton(tr("Anwenden"), this);
    m_apply->setObjectName(QStringLiteral("mediaApply"));
    m_delete = new QPushButton(tr("Löschen"), this);
    m_delete->setObjectName(QStringLiteral("mediaDelete"));
    m_delete->setToolTip(tr("Eigenen Strich aus der Bibliothek löschen"));
    row->addWidget(m_delete);
    row->addStretch(1);
    row->addWidget(m_apply);
    layout->addLayout(row);

    // Gewählte Objekte als eigenen Strich speichern
    auto* saveTitle = new QLabel(tr("Gewählte Objekte speichern als:"), this);
    layout->addWidget(saveTitle);
    auto* saveRow = new QHBoxLayout;
    m_saveType = new QComboBox(this);
    m_saveType->setObjectName(QStringLiteral("mediaSaveType"));
    m_saveType->addItems({tr("Pinselstrich"), tr("Sprühliste")});
    m_saveName = new QLineEdit(this);
    m_saveName->setObjectName(QStringLiteral("mediaSaveName"));
    m_saveName->setPlaceholderText(tr("Name"));
    m_save = new QPushButton(tr("Speichern"), this);
    m_save->setObjectName(QStringLiteral("mediaSave"));
    m_save->setToolTip(tr("Ausgewählte Objekte als Pinselstrich (entlang des Pfads gestreckt) oder als Sprühliste "
                          "(je Objekt ein Eintrag) in der Bibliothek ablegen"));
    saveRow->addWidget(m_saveType);
    saveRow->addWidget(m_saveName, 1);
    saveRow->addWidget(m_save);
    layout->addLayout(saveRow);
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    connect(m_category, &QComboBox::currentIndexChanged, this, [this] { reload(); });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) { apply(m_list->row(item)); });
    connect(m_list, &QListWidget::currentRowChanged, this, [this] { updateButtons(); });
    connect(m_apply, &QPushButton::clicked, this, [this] { apply(m_list->currentRow()); });
    connect(m_delete, &QPushButton::clicked, this, [this] {
        const int row = m_list->currentRow();
        if (row >= 0 && row < m_entries.size() && !m_entries[row].builtIn)
            MediaLibrary::instance().removeUser(m_entries[row].name);
    });
    connect(m_save, &QPushButton::clicked, this, [this] {
        saveSelection(m_saveType->currentIndex() == 1 ? SaveAs::Sprayer : SaveAs::Brush, m_saveName->text().trimmed());
    });
    // Erst nach dem auslösenden Klick neu aufbauen: die Liste darf im eigenen Signal nicht verschwinden
    connect(&MediaLibrary::instance(), &MediaLibrary::changed, this, [this] { reload(); }, Qt::QueuedConnection);
    connect(&m_editor, &Editor::selectionChanged, this, [this] { updateButtons(); });
    reload();
}

void MediaDocker::setCategory(Category c)
{
    m_category->setCurrentIndex(int(c));
    reload();
}

MediaDocker::Category MediaDocker::category() const
{
    return Category(m_category->currentIndex());
}

void MediaDocker::reload()
{
    const MediaLibrary& lib = MediaLibrary::instance();
    const QString current = m_list->currentItem() ? m_list->currentItem()->text() : QString();
    switch (category()) {
    case Category::Recent: m_entries = lib.recent(); break;
    case Category::Presets: m_entries = lib.presets(); break;
    case Category::Brushes: m_entries = lib.brushes(); break;
    case Category::Sprayers: m_entries = lib.sprayers(); break;
    case Category::User: m_entries = lib.user(); break;
    }
    const QSignalBlocker block(m_list);
    m_list->clear();
    const qreal dpr = devicePixelRatioF();
    for (const MediaStroke& s : std::as_const(m_entries)) {
        auto* item = new QListWidgetItem(mediaStrokeIcon(s.media, m_list->iconSize(), dpr), s.name, m_list);
        item->setToolTip(s.name);
        if (s.name == current)
            m_list->setCurrentItem(item);
    }
    updateButtons();
}

void MediaDocker::updateButtons()
{
    const int row = m_list->currentRow();
    const bool valid = row >= 0 && row < m_entries.size();
    m_apply->setEnabled(valid);
    m_delete->setEnabled(valid && !m_entries[row].builtIn && category() == Category::User);
    m_save->setEnabled(!m_editor.selectedObjects().isEmpty());
}

bool MediaDocker::apply(int index)
{
    if (index < 0 || index >= m_entries.size())
        return false;
    const MediaStroke stroke = m_entries[index];
    if (!m_editor.selectedObjects().isEmpty())
        m_editor.applyMediaToSelection(stroke.media, tr("Künstlerische Medien"));
    m_canvas.mediaTool().setMedia(stroke.media);
    m_canvas.notifyToolState();
    MediaLibrary::instance().noteUsed(stroke);
    m_status->setText(tr("Strich: %1").arg(stroke.name));
    return true;
}

QString MediaDocker::saveSelection(SaveAs as, const QString& name)
{
    const QList<const DrawObject*> objects = m_editor.selectedObjects();
    QList<ArtItem> items;
    for (const DrawObject* o : objects) {
        ArtItem pieces = media::piecesOf(*o);
        if (!pieces.isEmpty())
            items << pieces;
    }
    if (items.isEmpty()) {
        m_status->setText(tr("Zuerst Objekte mit Füllung oder Kontur auswählen."));
        return {};
    }
    MediaLibrary& lib = MediaLibrary::instance();
    MediaStroke saved;
    if (as == SaveAs::Brush) {
        ArtItem all;
        for (const ArtItem& item : std::as_const(items))
            all += item;
        saved = lib.addUserBrush(name, all);
    } else {
        saved = lib.addUserSprayer(name, items);
    }
    m_saveName->clear();
    setCategory(Category::User);
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].name == saved.name)
            m_list->setCurrentRow(i);
    m_status->setText(as == SaveAs::Brush ? tr("Pinselstrich «%1» gespeichert.").arg(saved.name)
                                          : tr("Sprühliste «%1» gespeichert.").arg(saved.name));
    return saved.name;
}

} // namespace vek

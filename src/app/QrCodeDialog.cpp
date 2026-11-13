#include "QrCodeDialog.h"

#include "ColorDialog.h"
#include "ExpressionSpinBox.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace vek {

namespace {
constexpr int kPreview = 240;
}

QrCodeDialog::QrCodeDialog(const QrCodeObject& initial, bool inserting, QWidget* parent)
    : QDialog(parent), m_inserting(inserting)
{
    setWindowTitle(inserting ? tr("QR-Code einfügen") : tr("QR-Code bearbeiten"));
    auto* main = new QVBoxLayout(this);
    auto* row = new QHBoxLayout;
    main->addLayout(row);

    auto* left = new QVBoxLayout;
    row->addLayout(left, 1);
    auto* kindForm = new QFormLayout;
    left->addLayout(kindForm);
    m_kind = new QComboBox(this);
    for (int k = 0; k < qr::kKindCount; ++k)
        m_kind->addItem(qr::kindLabel(qr::Kind(k)));
    kindForm->addRow(tr("Inhalt:"), m_kind);

    // Je Art eine Seite mit ihren Feldern
    m_pages = new QStackedWidget(this);
    left->addWidget(m_pages);
    const QMap<QString, QString> start = initial.values();
    for (int k = 0; k < qr::kKindCount; ++k) {
        auto* page = new QWidget(m_pages);
        auto* form = new QFormLayout(page);
        form->setContentsMargins(0, 0, 0, 0);
        QMap<QString, QWidget*> widgets;
        const bool current = qr::Kind(k) == initial.kind();
        for (const qr::Field& f : qr::fields(qr::Kind(k))) {
            const QString value = current ? start.value(f.id) : QString();
            QWidget* w = nullptr;
            if (!f.choices.isEmpty()) {
                auto* combo = new QComboBox(page);
                combo->addItems(f.choices);
                combo->setCurrentIndex(std::max(0, int(f.choiceValues.indexOf(value))));
                connect(combo, &QComboBox::currentIndexChanged, this, &QrCodeDialog::refresh);
                w = combo;
            } else if (f.multiline) {
                auto* edit = new QPlainTextEdit(value, page);
                edit->setTabChangesFocus(true);
                edit->setFixedHeight(70);
                connect(edit, &QPlainTextEdit::textChanged, this, &QrCodeDialog::refresh);
                w = edit;
            } else {
                auto* edit = new QLineEdit(value, page);
                if (f.id == QLatin1String("password"))
                    edit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
                connect(edit, &QLineEdit::textChanged, this, &QrCodeDialog::refresh);
                w = edit;
            }
            w->setObjectName(QStringLiteral("qr-") + f.id);
            form->addRow(f.label, w);
            widgets.insert(f.id, w);
        }
        m_fields << widgets;
        m_pages->addWidget(page);
    }

    left->addStretch();
    auto* right = new QVBoxLayout;
    row->addLayout(right);
    m_preview = new QLabel(this);
    m_preview->setFixedSize(kPreview, kPreview);
    m_preview->setFrameShape(QFrame::Box);
    m_preview->setAlignment(Qt::AlignCenter);
    right->addWidget(m_preview);
    m_info = new QLabel(this);
    m_info->setWordWrap(true);
    m_info->setFixedWidth(kPreview);
    right->addWidget(m_info);

    auto* options = new QGroupBox(tr("Darstellung"), this);
    right->addWidget(options);
    auto* form = new QFormLayout(options);
    m_ecc = new QComboBox(options);
    m_ecc->addItems(qr::errorCorrectionLabels());
    m_ecc->setToolTip(tr("Höhere Fehlerkorrektur liest sich auch bei Schmutz oder einem Logo in der Mitte, "
                         "braucht aber mehr Module"));
    form->addRow(tr("Fehlerkorrektur:"), m_ecc);
    m_shape = new QComboBox(options);
    m_shape->addItems({tr("Quadrate"), tr("Punkte")});
    form->addRow(tr("Module:"), m_shape);
    m_quiet = new QSpinBox(options);
    m_quiet->setRange(0, 10);
    m_quiet->setSuffix(tr(" Module"));
    m_quiet->setToolTip(tr("Freier Rand um den Code; die Norm verlangt 4 Module"));
    form->addRow(tr("Ruhezone:"), m_quiet);
    m_color = new ColorButton(tr("Farbe der Module"), options);
    form->addRow(tr("Farbe:"), m_color);
    auto* bgRow = new QHBoxLayout;
    m_hasBackground = new QCheckBox(options);
    m_background = new ColorButton(tr("Hintergrund"), options);
    bgRow->addWidget(m_hasBackground);
    bgRow->addWidget(m_background);
    bgRow->addStretch();
    form->addRow(tr("Hintergrund:"), bgRow);
    if (inserting) {
        m_size = new ExpressionSpinBox(options);
        m_size->setRange(5, 1000);
        m_size->setDecimals(1);
        m_size->setSuffix(QStringLiteral(" mm"));
        m_size->setValue(initial.size().width());
        form->addRow(tr("Grösse:"), m_size);
    }
    right->addStretch();

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    main->addWidget(m_buttons);

    m_kind->setCurrentIndex(static_cast<int>(initial.kind()));
    showPage(static_cast<int>(initial.kind()));
    m_ecc->setCurrentIndex(initial.errorCorrection());
    m_shape->setCurrentIndex(initial.shape() == QrCodeObject::Shape::Dots ? 1 : 0);
    m_quiet->setValue(initial.quietZone());
    m_color->setColor(initial.style().fill.representativeColor().value_or(Color(0, 0, 0)));
    m_hasBackground->setChecked(initial.background().has_value());
    m_background->setColor(initial.background().value_or(Color(255, 255, 255)));
    m_background->setEnabled(m_hasBackground->isChecked());

    connect(m_kind, &QComboBox::currentIndexChanged, this, [this](int index) {
        showPage(index);
        refresh();
    });
    connect(m_ecc, &QComboBox::currentIndexChanged, this, &QrCodeDialog::refresh);
    connect(m_shape, &QComboBox::currentIndexChanged, this, &QrCodeDialog::refresh);
    connect(m_quiet, &QSpinBox::valueChanged, this, &QrCodeDialog::refresh);
    connect(m_color, &ColorButton::colorChanged, this, &QrCodeDialog::refresh);
    connect(m_background, &ColorButton::colorChanged, this, &QrCodeDialog::refresh);
    connect(m_hasBackground, &QCheckBox::toggled, this, [this](bool on) {
        m_background->setEnabled(on);
        refresh();
    });
    refresh();
}

void QrCodeDialog::showPage(int index)
{
    // Nur die sichtbare Seite bestimmt die Höhe; sonst bliebe bei kurzen Arten der Platz der
    // Visitenkarte leer
    for (int i = 0; i < m_pages->count(); ++i)
        m_pages->widget(i)->setSizePolicy(QSizePolicy::Preferred, i == index ? QSizePolicy::Preferred : QSizePolicy::Ignored);
    m_pages->setCurrentIndex(index);
    m_pages->adjustSize();
    if (layout())
        layout()->activate();
}

QWidget* QrCodeDialog::field(const QString& id) const
{
    return m_fields.value(m_kind->currentIndex()).value(id);
}

QMap<QString, QString> QrCodeDialog::values() const
{
    QMap<QString, QString> out;
    const qr::Kind kind = qr::Kind(m_kind->currentIndex());
    for (const qr::Field& f : qr::fields(kind)) {
        QWidget* w = m_fields.value(int(kind)).value(f.id);
        if (auto* combo = qobject_cast<QComboBox*>(w))
            out.insert(f.id, f.choiceValues.value(combo->currentIndex()));
        else if (auto* text = qobject_cast<QPlainTextEdit*>(w))
            out.insert(f.id, text->toPlainText());
        else if (auto* line = qobject_cast<QLineEdit*>(w))
            out.insert(f.id, line->text());
    }
    return out;
}

void QrCodeDialog::applyTo(QrCodeObject& object) const
{
    object.setContent(qr::Kind(m_kind->currentIndex()), values());
    object.setErrorCorrection(m_ecc->currentIndex());
    object.setShape(m_shape->currentIndex() == 1 ? QrCodeObject::Shape::Dots : QrCodeObject::Shape::Squares);
    object.setQuietZone(m_quiet->value());
    if (m_hasBackground->isChecked())
        object.setBackground(m_background->color());
    else
        object.setBackground(std::nullopt);
    object.editStyle([this](Style& s) { s.fill = Fill::solid(m_color->color()); });
    if (m_size)
        object.setSize(QSizeF(m_size->value(), m_size->value()));
}

QString QrCodeDialog::info() const
{
    return m_info->text();
}

bool QrCodeDialog::isValid() const
{
    return m_buttons->button(QDialogButtonBox::Ok)->isEnabled();
}

void QrCodeDialog::refresh()
{
    QrCodeObject sample;
    applyTo(sample);
    sample.setSize(QSizeF(kPreview - 20, kPreview - 20));
    QString error;
    const qr::Matrix m = qr::encode(sample.payload(), sample.errorCorrection(), &error);
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(m.size > 0);

    QImage image(kPreview - 2, kPreview - 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(0xee, 0xee, 0xee));
    if (m.size > 0) {
        QPainter p(&image);
        p.setRenderHint(QPainter::Antialiasing);
        p.translate(9, 9);
        if (sample.background())
            p.fillPath(sample.backgroundPath(), sample.background()->toQColor());
        p.fillPath(sample.localPath(), m_color->color().toQColor());
        const int chars = int(sample.payload().toUtf8().size());
        m_info->setText(tr("Version %1 · %2 × %2 Module · %3 Byte").arg(m.version).arg(m.size).arg(chars));
        m_info->setStyleSheet(QString());
        if (m_size) {
            // Kleinste Modulgrösse für sicheres Lesen mit dem Telefon: etwa 0,4 mm
            const double module = m_size->value() / (m.size + 2 * m_quiet->value());
            if (module < 0.4)
                m_info->setText(m_info->text() + QStringLiteral("\n")
                                + tr("Module nur %1 mm gross – für den Druck grösser wählen").arg(module, 0, 'f', 2));
        }
    } else {
        m_info->setText(error);
        m_info->setStyleSheet(QStringLiteral("color: #c0392b"));
    }
    m_preview->setPixmap(QPixmap::fromImage(image));
}

} // namespace vek

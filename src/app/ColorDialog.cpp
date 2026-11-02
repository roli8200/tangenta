#include "ColorDialog.h"

#include "Icons.h"
#include "Palette.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpressionValidator>
#include <QScreen>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QToolTip>
#include <QVBoxLayout>

#include <cmath>
#include <functional>

namespace vek {

// --- Bausteine -----------------------------------------------------------------------------

// Sättigung (waagrecht) und Helligkeit (senkrecht) bei festem Farbton
class SaturationValueField : public QWidget {
public:
    std::function<void(double s, double v)> changed;

    explicit SaturationValueField(QWidget* parent) : QWidget(parent) { setFixedSize(210, 210); }
    void set(double hue, double s, double v)
    {
        m_hue = hue;
        m_s = s;
        m_v = v;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const QRect r = rect().adjusted(0, 0, -1, -1);
        QLinearGradient horizontal(r.topLeft(), r.topRight());
        horizontal.setColorAt(0, Qt::white);
        horizontal.setColorAt(1, QColor::fromHsvF(static_cast<float>(m_hue / 360.0), 1.0f, 1.0f));
        p.fillRect(r, horizontal);
        QLinearGradient vertical(r.topLeft(), r.bottomLeft());
        vertical.setColorAt(0, QColor(0, 0, 0, 0));
        vertical.setColorAt(1, Qt::black);
        p.fillRect(r, vertical);
        p.setPen(palette().color(QPalette::Mid));
        p.drawRect(r);
        const QPointF m(m_s * r.width(), (1.0 - m_v) * r.height());
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(m_v > 0.5 ? Qt::black : Qt::white, 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(m.x() - 3.5, m.y() - 3.5, 7, 7));
    }
    void mousePressEvent(QMouseEvent* e) override { pick(e->position()); }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (e->buttons() & Qt::LeftButton)
            pick(e->position());
    }

private:
    void pick(const QPointF& pos)
    {
        m_s = std::clamp(pos.x() / (width() - 1), 0.0, 1.0);
        m_v = 1.0 - std::clamp(pos.y() / (height() - 1), 0.0, 1.0);
        update();
        if (changed)
            changed(m_s, m_v);
    }

    double m_hue = 0.0;
    double m_s = 0.0;
    double m_v = 0.0;
};

// Senkrechte Farbtonleiste
class HueStrip : public QWidget {
public:
    std::function<void(double hue)> changed;

    explicit HueStrip(QWidget* parent) : QWidget(parent) { setFixedSize(18, 210); }
    void set(double hue)
    {
        m_hue = hue;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const QRect r = rect().adjusted(3, 0, -4, -1);
        QLinearGradient g(r.topLeft(), r.bottomLeft());
        for (int i = 0; i <= 6; ++i)
            g.setColorAt(i / 6.0, QColor::fromHsvF(static_cast<float>(1.0 - i / 6.0) * 0.9999f, 1.0f, 1.0f));
        p.fillRect(r, g);
        p.setPen(palette().color(QPalette::Mid));
        p.drawRect(r);
        const double y = (1.0 - m_hue / 360.0) * r.height();
        p.setPen(QPen(palette().color(QPalette::WindowText), 1));
        p.setBrush(Qt::white);
        p.drawRect(QRectF(0.5, y - 2.5, width() - 1.0, 5));
    }
    void mousePressEvent(QMouseEvent* e) override { pick(e->position()); }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (e->buttons() & Qt::LeftButton)
            pick(e->position());
    }

private:
    void pick(const QPointF& pos)
    {
        m_hue = std::clamp(1.0 - pos.y() / (height() - 1), 0.0, 1.0) * 359.999;
        update();
        if (changed)
            changed(m_hue);
    }

    double m_hue = 0.0;
};

// Raster der Standardpalette
class PaletteGrid : public QWidget {
public:
    std::function<void(int index)> chosen;
    static constexpr int Columns = 12;
    static constexpr int Cell = 17;

    explicit PaletteGrid(QWidget* parent) : QWidget(parent)
    {
        const int rows = (static_cast<int>(defaultPalette().size()) + Columns - 1) / Columns;
        setFixedSize(Columns * Cell + 1, rows * Cell + 1);
        setMouseTracking(true);
    }
    void setCurrent(int index)
    {
        m_current = index;
        update();
    }
    QRect cell(int i) const { return QRect((i % Columns) * Cell, (i / Columns) * Cell, Cell, Cell); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const auto& colors = defaultPalette();
        for (int i = 0; i < colors.size(); ++i) {
            const QRect r = cell(i);
            p.fillRect(r.adjusted(1, 1, 0, 0), colors[i].color.toQColor());
            p.setPen(QColor(96, 96, 96));
            p.drawRect(r);
        }
        if (m_current >= 0) {
            p.setPen(QPen(palette().color(QPalette::Highlight), 2));
            p.drawRect(cell(m_current).adjusted(1, 1, 0, 0));
        }
    }
    void mousePressEvent(QMouseEvent* e) override
    {
        for (int i = 0; i < defaultPalette().size(); ++i) {
            if (cell(i).contains(e->position().toPoint())) {
                setCurrent(i);
                if (chosen)
                    chosen(i);
                return;
            }
        }
    }
    bool event(QEvent* e) override
    {
        if (e->type() == QEvent::ToolTip) {
            auto* help = static_cast<QHelpEvent*>(e);
            for (int i = 0; i < defaultPalette().size(); ++i) {
                if (cell(i).contains(help->pos())) {
                    QToolTip::showText(help->globalPos(), defaultPalette()[i].name, this);
                    return true;
                }
            }
            QToolTip::hideText();
            return true;
        }
        return QWidget::event(e);
    }

private:
    int m_current = -1;
};

// Vorschau: oben die bisherige, unten die neue Farbe
class ColorPreview : public QWidget {
public:
    explicit ColorPreview(QWidget* parent) : QWidget(parent) { setFixedSize(130, 64); }
    void set(const Color& oldColor, const Color& newColor)
    {
        m_old = oldColor;
        m_new = newColor;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const QRect r = rect().adjusted(0, 0, -1, -1);
        p.fillRect(QRect(r.left(), r.top(), r.width(), r.height() / 2), m_old.toQColor());
        p.fillRect(QRect(r.left(), r.top() + r.height() / 2, r.width(), r.height() - r.height() / 2), m_new.toQColor());
        p.setPen(palette().color(QPalette::Mid));
        p.drawRect(r);
    }

private:
    Color m_old;
    Color m_new;
};

// --- Dialog --------------------------------------------------------------------------------

ColorDialog::ColorDialog(const QString& title, const Color& initial, QWidget* parent)
    : QDialog(parent), m_initial(initial), m_color(initial)
{
    setWindowTitle(title);
    m_hue = std::max(0.0, initial.toQColor().hsvHueF() * 360.0);

    auto* tabs = new QTabWidget(this);

    // Modelle
    auto* models = new QWidget(tabs);
    auto* modelsLayout = new QVBoxLayout(models);
    auto* modelRow = new QHBoxLayout;
    modelRow->addWidget(new QLabel(tr("Modell:"), models));
    m_model = new QComboBox(models);
    m_model->addItems({QStringLiteral("RGB"), QStringLiteral("HSB")});
    modelRow->addWidget(m_model, 1);
    modelsLayout->addLayout(modelRow);
    auto* pickers = new QHBoxLayout;
    m_field = new SaturationValueField(models);
    m_hueStrip = new HueStrip(models);
    pickers->addWidget(m_field);
    pickers->addWidget(m_hueStrip);
    pickers->addStretch(1);
    modelsLayout->addLayout(pickers);
    modelsLayout->addStretch(1);
    tabs->addTab(models, icon(QStringLiteral("color-models")), tr("Modelle"));

    // Paletten
    auto* palettes = new QWidget(tabs);
    auto* palettesLayout = new QVBoxLayout(palettes);
    auto* paletteName = new QComboBox(palettes);
    paletteName->addItem(tr("Standard-RGB-Palette"));
    palettesLayout->addWidget(paletteName);
    m_grid = new PaletteGrid(palettes);
    palettesLayout->addWidget(m_grid);
    auto* tintRow = new QHBoxLayout;
    tintRow->addWidget(new QLabel(tr("Tönung:"), palettes));
    m_tint = new QSlider(Qt::Horizontal, palettes);
    m_tint->setRange(0, 100);
    m_tint->setValue(100);
    auto* tintValue = new QLabel(QStringLiteral("100"), palettes);
    tintValue->setMinimumWidth(28);
    tintRow->addWidget(m_tint, 1);
    tintRow->addWidget(tintValue);
    palettesLayout->addLayout(tintRow);
    palettesLayout->addStretch(1);
    tabs->addTab(palettes, icon(QStringLiteral("color-palettes")), tr("Paletten"));

    // Rechte Seite
    auto* side = new QGridLayout;
    m_eyedropper = new QToolButton(this);
    m_eyedropper->setIcon(icon(QStringLiteral("eyedropper")));
    m_eyedropper->setIconSize(QSize(20, 20));
    m_eyedropper->setToolTip(tr("Pipette: Farbe vom Bildschirm aufnehmen"));
    m_preview = new ColorPreview(this);
    side->addWidget(m_eyedropper, 0, 0, Qt::AlignTop);
    side->addWidget(m_preview, 0, 1, 1, 2);
    side->addWidget(new QLabel(tr("Hex"), this), 1, 1);
    m_hex = new QLineEdit(this);
    m_hex->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("#?[0-9A-Fa-f]{0,6}")), m_hex));
    m_hex->setMaximumWidth(80);
    side->addWidget(m_hex, 1, 2);
    for (int i = 0; i < 3; ++i) {
        m_labels[i] = new QLabel(this);
        m_spins[i] = new QSpinBox(this);
        m_spins[i]->setKeyboardTracking(false);
        side->addWidget(m_labels[i], 2 + i, 1);
        side->addWidget(m_spins[i], 2 + i, 2);
    }
    side->addWidget(new QLabel(tr("Name:"), this), 5, 1, 1, 2);
    m_name = new QComboBox(this);
    m_name->addItem(QString());
    for (const NamedColor& n : defaultPalette())
        m_name->addItem(n.name);
    side->addWidget(m_name, 6, 1, 1, 2);
    side->setRowStretch(7, 1);

    auto* row = new QHBoxLayout;
    row->addWidget(tabs, 1);
    row->addLayout(side);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(row);
    layout->addWidget(buttons);

    // Verbindungen
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    m_field->changed = [this](double s, double v) { setFromHsv(m_hue, s, v); };
    m_hueStrip->changed = [this](double h) {
        const QColor c = m_color.toQColor();
        setFromHsv(h, c.hsvSaturationF(), c.valueF());
    };
    auto applyPalette = [this, tintValue] {
        tintValue->setText(QString::number(m_tint->value()));
        const int index = m_name->currentIndex() - 1;
        if (index < 0 || index >= defaultPalette().size())
            return;
        // Tönung: Anteil der Palettenfarbe, Rest Weiss
        const Color base = defaultPalette()[index].color;
        const double t = m_tint->value() / 100.0;
        auto mix = [t](std::uint8_t c) { return static_cast<std::uint8_t>(std::lround(255.0 - (255.0 - c) * t)); };
        setColor(Color(mix(base.r), mix(base.g), mix(base.b), m_color.a));
    };
    m_grid->chosen = [this, applyPalette](int index) {
        const QSignalBlocker block(m_name);
        m_name->setCurrentIndex(index + 1);
        applyPalette();
    };
    connect(m_tint, &QSlider::valueChanged, this, applyPalette);
    connect(m_name, &QComboBox::activated, this, [this](int index) {
        if (index > 0)
            setColor(defaultPalette()[index - 1].color);
    });
    connect(m_model, &QComboBox::currentIndexChanged, this, [this] { updateWidgets(); });
    connect(m_hex, &QLineEdit::editingFinished, this, [this] {
        QString t = m_hex->text();
        if (!t.startsWith(QLatin1Char('#')))
            t.prepend(QLatin1Char('#'));
        if (const std::optional<Color> c = Color::fromHex(t); c && t.size() == 7)
            setColor(Color(c->r, c->g, c->b, m_color.a));
        else
            updateWidgets();
    });
    for (int i = 0; i < 3; ++i) {
        connect(m_spins[i], &QSpinBox::valueChanged, this, [this] {
            if (m_updating)
                return;
            if (m_model->currentIndex() == 0) {
                setColor(Color(static_cast<std::uint8_t>(m_spins[0]->value()),
                               static_cast<std::uint8_t>(m_spins[1]->value()),
                               static_cast<std::uint8_t>(m_spins[2]->value()), m_color.a));
            } else {
                setFromHsv(m_spins[0]->value(), m_spins[1]->value() / 100.0, m_spins[2]->value() / 100.0);
            }
        });
    }
    connect(m_eyedropper, &QToolButton::clicked, this, &ColorDialog::startEyedropper);

    updateWidgets();
}

void ColorDialog::setColor(const Color& c)
{
    m_color = c;
    const QColor q = c.toQColor();
    if (q.hsvHueF() >= 0) // Grautöne haben keinen Farbton; den bisherigen behalten
        m_hue = q.hsvHueF() * 360.0;
    updateWidgets();
}

void ColorDialog::setFromHsv(double h, double s, double v)
{
    m_hue = h;
    const QColor q = QColor::fromHsvF(static_cast<float>(h / 360.0), static_cast<float>(s), static_cast<float>(v));
    m_color = Color(static_cast<std::uint8_t>(q.red()), static_cast<std::uint8_t>(q.green()),
                    static_cast<std::uint8_t>(q.blue()), m_color.a);
    updateWidgets();
}

void ColorDialog::updateWidgets()
{
    m_updating = true;
    const QColor q = m_color.toQColor();
    m_field->set(m_hue, q.hsvSaturationF(), q.valueF());
    m_hueStrip->set(m_hue);
    m_preview->set(m_initial, m_color);
    m_hex->setText(m_color.toHex().left(7).toUpper());

    const bool rgb = m_model->currentIndex() == 0;
    const QStringList labels = rgb ? QStringList{QStringLiteral("R"), QStringLiteral("G"), QStringLiteral("B")}
                                   : QStringList{QStringLiteral("H"), QStringLiteral("S"), QStringLiteral("B")};
    const int values[3] = {rgb ? m_color.r : static_cast<int>(std::lround(m_hue)) % 360,
                           rgb ? m_color.g : static_cast<int>(std::lround(q.hsvSaturationF() * 100)),
                           rgb ? m_color.b : static_cast<int>(std::lround(q.valueF() * 100))};
    for (int i = 0; i < 3; ++i) {
        const QSignalBlocker block(m_spins[i]);
        m_labels[i]->setText(labels[i]);
        m_spins[i]->setRange(0, rgb ? 255 : (i == 0 ? 359 : 100));
        m_spins[i]->setSuffix(rgb ? QString() : (i == 0 ? QStringLiteral(" °") : QStringLiteral(" %")));
        m_spins[i]->setValue(values[i]);
    }

    const QString name = colorName(m_color);
    {
        const QSignalBlocker block(m_name);
        m_name->setCurrentIndex(name.isEmpty() ? 0 : m_name->findText(name));
    }
    m_grid->setCurrent(name.isEmpty() ? -1 : m_name->currentIndex() - 1);
    m_updating = false;
}

// --- Pipette --------------------------------------------------------------------------------

std::optional<Color> ColorDialog::screenColorAt(const QPoint& globalPos)
{
    QScreen* screen = QGuiApplication::screenAt(globalPos);
    if (!screen)
        return std::nullopt;
    const QPoint local = globalPos - screen->geometry().topLeft();
    const QImage img = screen->grabWindow(0, local.x(), local.y(), 1, 1).toImage();
    if (img.isNull())
        return std::nullopt; // z.B. Wayland ohne Bildschirmzugriff
    const QColor c = img.pixelColor(0, 0);
    return Color(static_cast<std::uint8_t>(c.red()), static_cast<std::uint8_t>(c.green()),
                 static_cast<std::uint8_t>(c.blue()));
}

void ColorDialog::startEyedropper()
{
    m_picking = true;
    m_beforePicking = m_color;
    m_eyedropper->setDown(true);
    setMouseTracking(true);
    grabMouse(QCursor(icon(QStringLiteral("eyedropper")).pixmap(24), 2, 21));
    grabKeyboard();
}

void ColorDialog::stopEyedropper(bool accept)
{
    m_picking = false;
    m_eyedropper->setDown(false);
    releaseMouse();
    releaseKeyboard();
    QToolTip::hideText();
    if (!accept)
        setColor(m_beforePicking);
}

void ColorDialog::mouseMoveEvent(QMouseEvent* e)
{
    if (!m_picking)
        return QDialog::mouseMoveEvent(e);
    const QPoint gp = e->globalPosition().toPoint();
    if (const std::optional<Color> c = screenColorAt(gp)) {
        setColor(*c);
        QToolTip::showText(gp + QPoint(14, 14),
                           QStringLiteral("R: %1\nG: %2\nB: %3\n%4").arg(c->r).arg(c->g).arg(c->b).arg(c->toHex().toUpper()));
    }
}

void ColorDialog::mousePressEvent(QMouseEvent* e)
{
    if (!m_picking)
        return QDialog::mousePressEvent(e);
    if (e->button() == Qt::LeftButton) {
        if (const std::optional<Color> c = screenColorAt(e->globalPosition().toPoint()))
            setColor(*c);
        stopEyedropper(true);
    } else {
        stopEyedropper(false);
    }
}

void ColorDialog::keyPressEvent(QKeyEvent* e)
{
    if (m_picking && e->key() == Qt::Key_Escape) {
        stopEyedropper(false);
        return;
    }
    QDialog::keyPressEvent(e);
}

std::optional<Color> ColorDialog::getColor(QWidget* parent, const QString& title, const Color& initial)
{
    ColorDialog dlg(title, initial, parent);
    if (dlg.exec() != QDialog::Accepted)
        return std::nullopt;
    return dlg.color();
}

// --- Farbknopf -----------------------------------------------------------------------------

ColorButton::ColorButton(const QString& dialogTitle, QWidget* parent)
    : QToolButton(parent), m_title(dialogTitle)
{
    setIconSize(QSize(36, 16));
    setColor(Color());
    connect(this, &QToolButton::clicked, this, [this] {
        if (const std::optional<Color> c = ColorDialog::getColor(this, m_title, m_color)) {
            setColor(*c);
            emit colorChanged(*c);
        }
    });
}

void ColorButton::setColor(const Color& c)
{
    m_color = c;
    QPixmap pm(iconSize());
    pm.fill(c.toQColor());
    QPainter p(&pm);
    p.setPen(QColor(96, 96, 96));
    p.drawRect(pm.rect().adjusted(0, 0, -1, -1));
    setIcon(QIcon(pm));
    setToolTip(c.toHex().toUpper());
}

} // namespace vek

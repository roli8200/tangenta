#include "GradientDialog.h"

#include "ColorDialog.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QRadioButton>
#include <QSlider>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <cmath>

namespace vek {

namespace {

constexpr int kMarker = 6;

Color mix(const Color& a, const Color& b, double t)
{
    auto m = [t](std::uint8_t x, std::uint8_t y) { return static_cast<std::uint8_t>(std::lround(x + (y - x) * t)); };
    return Color(m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a));
}

// Vorschau im Seitenverhältnis des Objekts
class GradientPreview : public QWidget {
public:
    GradientPreview(const Fill* fill, const QSizeF& size, QWidget* parent) : QWidget(parent), m_fill(fill), m_size(size)
    {
        setFixedSize(150, 150);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), palette().color(QPalette::Base));
        const double w = std::max(m_size.width(), 1e-6);
        const double h = std::max(m_size.height(), 1e-6);
        const double scale = std::min((width() - 10) / w, (height() - 10) / h);
        const QRectF r((width() - w * scale) / 2, (height() - h * scale) / 2, w * scale, h * scale);
        p.fillRect(r, m_fill->brush(r));
        p.setPen(palette().color(QPalette::Mid));
        p.drawRect(r);
    }

private:
    const Fill* m_fill;
    QSizeF m_size;
};

} // namespace

// --- Farbstopps ---------------------------------------------------------------------------

GradientStopBar::GradientStopBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(34);
    setMinimumWidth(240);
    setFocusPolicy(Qt::StrongFocus);
    setToolTip(tr("Klick wählt einen Farbstopp, Ziehen verschiebt ihn,\nDoppelklick fügt einen hinzu, Entf löscht ihn"));
}

void GradientStopBar::setStops(const QList<GradientStop>& stops)
{
    m_stops = stops;
    m_current = std::clamp(m_current, 0, static_cast<int>(m_stops.size()) - 1);
    update();
}

QRect GradientStopBar::barRect() const
{
    return QRect(kMarker, 2, width() - 2 * kMarker, height() - 2 - 2 * kMarker - 2);
}

double GradientStopBar::positionAt(double x) const
{
    const QRect r = barRect();
    return std::clamp((x - r.left()) / std::max(1, r.width()), 0.0, 1.0);
}

int GradientStopBar::stopAt(const QPoint& pos) const
{
    const QRect r = barRect();
    for (int i = static_cast<int>(m_stops.size()) - 1; i >= 0; --i) {
        const double x = r.left() + m_stops[i].position * r.width();
        if (std::abs(pos.x() - x) <= kMarker && pos.y() >= r.bottom() - 4)
            return i;
    }
    return -1;
}

Color GradientStopBar::colorAt(double position) const
{
    if (m_stops.isEmpty())
        return Color();
    if (position <= m_stops.first().position)
        return m_stops.first().color;
    for (int i = 1; i < m_stops.size(); ++i) {
        if (position <= m_stops[i].position) {
            const double span = m_stops[i].position - m_stops[i - 1].position;
            return mix(m_stops[i - 1].color, m_stops[i].color, span > 0 ? (position - m_stops[i - 1].position) / span : 0);
        }
    }
    return m_stops.last().color;
}

void GradientStopBar::setCurrentColor(const Color& c)
{
    if (m_current < 0 || m_current >= m_stops.size())
        return;
    m_stops[m_current].color = c;
    update();
    emit stopsChanged();
}

void GradientStopBar::setCurrentPosition(double position)
{
    // Anfang und Ende bleiben fest
    if (m_current <= 0 || m_current >= m_stops.size() - 1)
        return;
    m_stops[m_current].position = std::clamp(position, 0.001, 0.999);
    const GradientStop moved = m_stops[m_current];
    std::stable_sort(m_stops.begin(), m_stops.end(),
                     [](const GradientStop& a, const GradientStop& b) { return a.position < b.position; });
    m_current = static_cast<int>(m_stops.indexOf(moved));
    update();
    emit stopsChanged();
    emit currentChanged(m_current);
}

void GradientStopBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    const QRect r = barRect();
    QLinearGradient g(r.topLeft(), r.topRight());
    for (const GradientStop& s : m_stops)
        g.setColorAt(s.position, s.color.toQColor());
    p.fillRect(r, g);
    p.setPen(palette().color(QPalette::Mid));
    p.drawRect(r.adjusted(0, 0, -1, -1));
    p.setRenderHint(QPainter::Antialiasing);
    for (int i = 0; i < m_stops.size(); ++i) {
        const double x = r.left() + m_stops[i].position * r.width();
        const QPolygonF marker{QPointF(x, r.bottom() - 3), QPointF(x - kMarker, r.bottom() + 2 * kMarker),
                               QPointF(x + kMarker, r.bottom() + 2 * kMarker)};
        p.setPen(QPen(i == m_current ? palette().color(QPalette::Highlight) : QColor(60, 60, 60), i == m_current ? 2 : 1));
        p.setBrush(m_stops[i].color.toQColor());
        p.drawPolygon(marker);
    }
}

void GradientStopBar::mousePressEvent(QMouseEvent* e)
{
    const int i = stopAt(e->position().toPoint());
    if (i < 0)
        return;
    m_current = i;
    m_dragging = i > 0 && i < m_stops.size() - 1;
    update();
    emit currentChanged(i);
}

void GradientStopBar::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragging && (e->buttons() & Qt::LeftButton))
        setCurrentPosition(positionAt(e->position().x()));
}

void GradientStopBar::mouseDoubleClickEvent(QMouseEvent* e)
{
    const double pos = positionAt(e->position().x());
    if (pos <= 0.0 || pos >= 1.0)
        return;
    m_stops.append({pos, colorAt(pos)});
    std::stable_sort(m_stops.begin(), m_stops.end(),
                     [](const GradientStop& a, const GradientStop& b) { return a.position < b.position; });
    for (int i = 0; i < m_stops.size(); ++i) {
        if (m_stops[i].position == pos)
            m_current = i;
    }
    update();
    emit stopsChanged();
    emit currentChanged(m_current);
}

void GradientStopBar::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Delete && m_current > 0 && m_current < m_stops.size() - 1) {
        m_stops.removeAt(m_current);
        m_current = std::min(m_current, static_cast<int>(m_stops.size()) - 1);
        update();
        emit stopsChanged();
        emit currentChanged(m_current);
        return;
    }
    QWidget::keyPressEvent(e);
}

// --- Dialog --------------------------------------------------------------------------------

GradientDialog::GradientDialog(const Fill& initial, const QSizeF& objectSize, QWidget* parent)
    : QDialog(parent), m_fill(initial), m_size(objectSize)
{
    setWindowTitle(tr("Farbverlauf"));
    if (!m_fill.isGradient()) {
        const Color from = m_fill.representativeColor().value_or(Color(0, 0, 0));
        m_fill = Fill::gradient(Fill::Type::Linear, from, Color(255, 255, 255));
    }
    if (m_fill.stops.size() < 2)
        m_fill.stops = {GradientStop{0.0, Color(0, 0, 0)}, GradientStop{1.0, Color(255, 255, 255)}};

    auto spin = [this](double min, double max, const QString& suffix) {
        auto* s = new QDoubleSpinBox(this);
        s->setRange(min, max);
        s->setDecimals(1);
        s->setSuffix(suffix);
        s->setKeyboardTracking(false);
        return s;
    };

    // Typ und Mittelpunktverschiebung
    m_type = new QComboBox(this);
    m_type->addItems({tr("Linear"), tr("Radial"), tr("Konisch")});
    m_offsetX = spin(-100, 100, QStringLiteral(" %"));
    m_offsetY = spin(-100, 100, QStringLiteral(" %"));
    auto* typeForm = new QFormLayout;
    typeForm->addRow(tr("Typ:"), m_type);
    auto* centerBox = new QGroupBox(tr("Mittelpunktverschiebung"), this);
    auto* centerForm = new QFormLayout(centerBox);
    centerForm->addRow(tr("Horizontal:"), m_offsetX);
    centerForm->addRow(tr("Vertikal:"), m_offsetY);

    // Optionen
    m_angle = spin(-360, 360, QStringLiteral(" °"));
    m_pad = spin(0, 49, QStringLiteral(" %"));
    auto* optionsBox = new QGroupBox(tr("Optionen"), this);
    auto* optionsForm = new QFormLayout(optionsBox);
    optionsForm->addRow(tr("Winkel:"), m_angle);
    optionsForm->addRow(tr("Randauffüllung:"), m_pad);

    m_preview = new GradientPreview(&m_fill, m_size, this);

    // Farbübergang
    auto* blendBox = new QGroupBox(tr("Farbübergang"), this);
    auto* blendLayout = new QVBoxLayout(blendBox);
    auto* modeRow = new QHBoxLayout;
    m_twoColors = new QRadioButton(tr("Zwei Farben"), blendBox);
    m_custom = new QRadioButton(tr("Benutzerdefiniert"), blendBox);
    modeRow->addWidget(m_twoColors);
    modeRow->addWidget(m_custom);
    modeRow->addStretch(1);
    blendLayout->addLayout(modeRow);
    m_blendPages = new QStackedWidget(blendBox);

    auto* twoPage = new QWidget(m_blendPages);
    auto* twoForm = new QFormLayout(twoPage);
    m_from = new ColorButton(tr("Farbe für Verlaufsanfang"), twoPage);
    m_to = new ColorButton(tr("Farbe für Verlaufsende"), twoPage);
    m_mid = new QSlider(Qt::Horizontal, twoPage);
    m_mid->setRange(1, 99);
    twoForm->addRow(tr("Von:"), m_from);
    twoForm->addRow(tr("Bis:"), m_to);
    twoForm->addRow(tr("Mittelpunkt:"), m_mid);
    m_blendPages->addWidget(twoPage);

    auto* customPage = new QWidget(m_blendPages);
    auto* customLayout = new QVBoxLayout(customPage);
    m_bar = new GradientStopBar(customPage);
    customLayout->addWidget(m_bar);
    auto* stopRow = new QHBoxLayout;
    stopRow->addWidget(new QLabel(tr("Position:"), customPage));
    m_stopPosition = spin(0, 100, QStringLiteral(" %"));
    stopRow->addWidget(m_stopPosition);
    stopRow->addWidget(new QLabel(tr("Farbe:"), customPage));
    m_stopColor = new ColorButton(tr("Farbe des Farbstopps"), customPage);
    stopRow->addWidget(m_stopColor);
    stopRow->addStretch(1);
    customLayout->addLayout(stopRow);
    m_blendPages->addWidget(customPage);
    blendLayout->addWidget(m_blendPages);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* top = new QHBoxLayout;
    auto* left = new QVBoxLayout;
    left->addLayout(typeForm);
    left->addWidget(centerBox);
    left->addWidget(optionsBox);
    left->addStretch(1);
    top->addLayout(left);
    top->addWidget(m_preview, 0, Qt::AlignTop);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(blendBox);
    layout->addWidget(buttons);

    // Zwei Farben, wenn der Verlauf nur aus Anfang, Ende und höchstens einer Mischfarbe besteht
    const bool simple = m_fill.stops.size() == 2
                        || (m_fill.stops.size() == 3
                            && m_fill.stops[1].color == mix(m_fill.stops.first().color, m_fill.stops.last().color, 0.5));
    (simple ? m_twoColors : m_custom)->setChecked(true);
    writeWidgets();

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_type, &QComboBox::currentIndexChanged, this, &GradientDialog::readWidgets);
    for (QDoubleSpinBox* s : {m_offsetX, m_offsetY, m_angle, m_pad})
        connect(s, &QDoubleSpinBox::valueChanged, this, &GradientDialog::readWidgets);
    connect(m_twoColors, &QRadioButton::toggled, this, &GradientDialog::readWidgets);
    connect(m_from, &ColorButton::colorChanged, this, &GradientDialog::readWidgets);
    connect(m_to, &ColorButton::colorChanged, this, &GradientDialog::readWidgets);
    connect(m_mid, &QSlider::valueChanged, this, &GradientDialog::readWidgets);
    connect(m_bar, &GradientStopBar::stopsChanged, this, &GradientDialog::readWidgets);
    connect(m_bar, &GradientStopBar::currentChanged, this, &GradientDialog::writeWidgets);
    connect(m_stopColor, &ColorButton::colorChanged, m_bar, &GradientStopBar::setCurrentColor);
    connect(m_stopPosition, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (!m_updating)
            m_bar->setCurrentPosition(v / 100.0);
    });
}

void GradientDialog::writeWidgets()
{
    m_updating = true;
    const int typeIndex = m_fill.type == Fill::Type::Radial ? 1 : m_fill.type == Fill::Type::Conical ? 2 : 0;
    m_type->setCurrentIndex(typeIndex);
    const GradientGeometry g = m_fill.geometry(m_size);
    m_angle->setValue(g.angle);
    m_pad->setValue(g.edgePad);
    m_offsetX->setValue(g.offsetX);
    m_offsetY->setValue(g.offsetY);
    m_offsetX->setEnabled(m_fill.type != Fill::Type::Linear);
    m_offsetY->setEnabled(m_fill.type != Fill::Type::Linear);
    m_angle->setEnabled(m_fill.type != Fill::Type::Radial);

    m_blendPages->setCurrentIndex(m_twoColors->isChecked() ? 0 : 1);
    m_from->setColor(m_fill.stops.first().color);
    m_to->setColor(m_fill.stops.last().color);
    m_mid->setValue(m_fill.stops.size() == 3 ? static_cast<int>(std::lround(m_fill.stops[1].position * 100)) : 50);
    if (m_bar->stops() != m_fill.stops)
        m_bar->setStops(m_fill.stops);
    const int cur = m_bar->current();
    if (cur >= 0 && cur < m_fill.stops.size()) {
        m_stopColor->setColor(m_fill.stops[cur].color);
        m_stopPosition->setValue(m_fill.stops[cur].position * 100.0);
        m_stopPosition->setEnabled(cur > 0 && cur < m_fill.stops.size() - 1);
    }
    m_preview->update();
    m_updating = false;
}

void GradientDialog::readWidgets()
{
    if (m_updating)
        return;
    const Fill::Type types[] = {Fill::Type::Linear, Fill::Type::Radial, Fill::Type::Conical};
    const Fill::Type newType = types[std::clamp(m_type->currentIndex(), 0, 2)];
    GradientGeometry g;
    g.angle = m_angle->value();
    g.edgePad = m_pad->value();
    g.offsetX = newType == Fill::Type::Linear ? 0.0 : m_offsetX->value();
    g.offsetY = newType == Fill::Type::Linear ? 0.0 : m_offsetY->value();
    m_fill.type = newType;
    m_fill.setGeometry(g, m_size);

    if (m_twoColors->isChecked()) {
        const Color from = m_from->color();
        const Color to = m_to->color();
        m_fill.stops = {GradientStop{0.0, from}, GradientStop{1.0, to}};
        if (m_mid->value() != 50)
            m_fill.stops.insert(1, GradientStop{m_mid->value() / 100.0, mix(from, to, 0.5)});
    } else {
        m_fill.stops = m_bar->stops();
    }
    writeWidgets();
}

std::optional<Fill> GradientDialog::getFill(QWidget* parent, const Fill& initial, const QSizeF& objectSize)
{
    GradientDialog dlg(initial, objectSize, parent);
    if (dlg.exec() != QDialog::Accepted)
        return std::nullopt;
    return dlg.fill();
}

} // namespace vek

#include "NodePopup.h"

#include "ColorDialog.h"
#include "ExpressionSpinBox.h"
#include "Icons.h"

#include <QGridLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QToolButton>

#include <algorithm>
#include <cmath>

namespace vek {

NodePopup::NodePopup(QWidget* canvas)
    : QFrame(canvas)
{
    setObjectName(QStringLiteral("nodePopup"));
    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);
    // Klicks im Fenster nicht an die Zeichenfläche weiterreichen
    setAttribute(Qt::WA_NoMousePropagation);
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(6, 6, 6, 6);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(4);

    m_color = new ColorButton(tr("Knotenfarbe"), this);
    m_color->setToolTip(tr("Knotenfarbe"));
    m_eyedropper = new QToolButton(this);
    m_eyedropper->setIcon(icon(QStringLiteral("eyedropper")));
    m_eyedropper->setToolTip(tr("Farbe von der Zeichenfläche aufnehmen"));
    m_eyedropper->setAutoRaise(true);
    grid->addWidget(m_color, 0, 0, 1, 2);
    grid->addWidget(m_eyedropper, 0, 2);

    m_sliderLabel = new QLabel(tr("Transparenz"), this);
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(0, 100);
    m_slider->setFixedWidth(100);
    m_slider->setToolTip(tr("Transparenz des Knotens"));
    m_sliderValue = new QLabel(this);
    m_sliderValue->setMinimumWidth(34);
    grid->addWidget(m_sliderLabel, 1, 0);
    grid->addWidget(m_slider, 1, 1);
    grid->addWidget(m_sliderValue, 1, 2);

    m_positionLabel = new QLabel(tr("Lage"), this);
    m_position = new ExpressionSpinBox(this);
    m_position->setRange(0.0, 100.0);
    m_position->setDecimals(1);
    m_position->setSuffix(QStringLiteral(" %"));
    m_position->setKeyboardTracking(false);
    m_position->setToolTip(tr("Lage des Knotens auf der Verlaufslinie"));
    grid->addWidget(m_positionLabel, 2, 0);
    grid->addWidget(m_position, 2, 1, 1, 2);

    connect(m_color, &ColorButton::colorChanged, this, [this](const Color& c) {
        // Transparenz des Knotens bleibt, der Farbwähler liefert die Farbe
        m_current = Color(c.r, c.g, c.b, c.a == 255 ? m_current.a : c.a);
        emit colorEdited(m_current, true);
    });
    connect(m_eyedropper, &QToolButton::clicked, this, &NodePopup::eyedropperRequested);
    connect(m_slider, &QSlider::valueChanged, this, [this] { emitSlider(!m_slider->isSliderDown()); });
    connect(m_slider, &QSlider::sliderReleased, this, [this] { emitSlider(true); });
    connect(m_position, &QDoubleSpinBox::editingFinished, this, [this] { emit positionEdited(m_position->value() / 100.0); });
    hide();
}

void NodePopup::emitSlider(bool final)
{
    const int v = m_slider->value();
    m_sliderValue->setText(QStringLiteral("%1 %").arg(v));
    if (m_valueMode) {
        emit valueEdited(v, final);
        return;
    }
    m_current.a = std::uint8_t(std::lround(255.0 * (100 - v) / 100.0));
    emit colorEdited(m_current, final);
}

void NodePopup::showColor(const Color& color, std::optional<double> position, const QPointF& at)
{
    m_valueMode = false;
    m_current = color;
    m_color->setColor(Color(color.r, color.g, color.b));
    m_color->show();
    m_eyedropper->show();
    m_sliderLabel->setText(tr("Transparenz"));
    {
        const QSignalBlocker block(m_slider);
        m_slider->setValue(int(std::lround(100.0 - color.a * 100.0 / 255.0)));
    }
    m_sliderValue->setText(QStringLiteral("%1 %").arg(m_slider->value()));
    m_positionLabel->setVisible(position.has_value());
    m_position->setVisible(position.has_value());
    if (position) {
        const QSignalBlocker block(m_position);
        m_position->setValue(*position * 100.0);
    }
    place(at);
}

void NodePopup::showValue(double value, const QString& label, const QPointF& at)
{
    m_valueMode = true;
    m_color->hide();
    m_eyedropper->hide();
    m_positionLabel->hide();
    m_position->hide();
    m_sliderLabel->setText(label);
    {
        const QSignalBlocker block(m_slider);
        m_slider->setValue(int(std::lround(value)));
    }
    m_sliderValue->setText(QStringLiteral("%1 %").arg(m_slider->value()));
    place(at);
}

void NodePopup::place(const QPointF& at)
{
    adjustSize();
    const QWidget* canvas = parentWidget();
    // Rechts unterhalb des Knotens, innerhalb der Zeichenfläche
    QPoint p = (at + QPointF(14, 14)).toPoint();
    if (p.x() + width() > canvas->width())
        p.setX(int(at.x()) - 14 - width());
    if (p.y() + height() > canvas->height())
        p.setY(int(at.y()) - 14 - height());
    move(std::max(0, p.x()), std::max(0, p.y()));
    show();
    raise();
}

} // namespace vek

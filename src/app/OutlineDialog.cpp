#include "OutlineDialog.h"

#include "Canvas.h"
#include "ColorDialog.h"
#include "Outline.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>

namespace vek {

namespace {

// Kleine Fläche, die mit einer Funktion gezeichnet wird (Vorschau, Feder)
class PaintBox : public QWidget {
public:
    PaintBox(QSize size, std::function<void(QPainter&, const QRect&)> paint, QWidget* parent)
        : QWidget(parent), m_paint(std::move(paint))
    {
        setFixedSize(size);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), Qt::white);
        p.setRenderHint(QPainter::Antialiasing);
        p.save();
        m_paint(p, rect());
        p.restore();
        p.setPen(QColor(0xa0, 0xa0, 0xa0));
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect().adjusted(0, 0, -1, -1));
    }

private:
    std::function<void(QPainter&, const QRect&)> m_paint;
};

QIcon drawnIcon(const std::function<void(QPainter&)>& draw)
{
    QPixmap pm(28, 20);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    draw(p);
    return QIcon(pm);
}

QIcon joinIcon(Qt::PenJoinStyle join)
{
    return drawnIcon([join](QPainter& p) {
        QPen pen(QColor(0x30, 0x30, 0x30), 6);
        pen.setJoinStyle(join);
        p.setPen(pen);
        p.drawPolyline(QPolygonF({QPointF(5, 17), QPointF(14, 5), QPointF(23, 17)}));
    });
}

QIcon capIcon(Qt::PenCapStyle cap)
{
    return drawnIcon([cap](QPainter& p) {
        QPen pen(QColor(0x30, 0x30, 0x30), 8);
        pen.setCapStyle(cap);
        p.setPen(pen);
        p.drawLine(QPointF(6, 10), QPointF(18, 10));
        p.setPen(QPen(Qt::red, 1));
        p.drawLine(QPointF(18, 3), QPointF(18, 17));
    });
}

QIcon positionIcon(int position)
{
    return drawnIcon([position](QPainter& p) {
        const QRectF r(8, 5, 12, 10);
        const double off = position == 2 ? -2 : position == 1 ? 2 : 0; // 2 = aussen, 1 = innen
        p.setPen(QPen(QColor(0x30, 0x30, 0x30), 4));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r.adjusted(-off, -off, off, off));
        p.setPen(QPen(Qt::red, 1));
        p.drawRect(r);
    });
}

QIcon dashIcon(int index)
{
    QPixmap pm(90, 12);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    QPen pen(Qt::black, 2);
    pen.setCapStyle(Qt::FlatCap);
    if (const QList<double> d = outline::dashPattern(index); !d.isEmpty())
        pen.setDashPattern(d);
    p.setPen(pen);
    p.drawLine(QPointF(2, 6), QPointF(88, 6));
    return QIcon(pm);
}

} // namespace

OutlinePenDialog::OutlinePenDialog(const Style& style, QWidget* parent)
    : QDialog(parent), m_style(style)
{
    setWindowTitle(tr("Konturstift"));
    auto* main = new QHBoxLayout(this);
    auto* left = new QVBoxLayout;
    auto* right = new QVBoxLayout;
    main->addLayout(left);
    main->addLayout(right);

    auto* form = new QFormLayout;
    left->addLayout(form);
    m_color = new ColorButton(tr("Konturfarbe"), this);
    m_color->setColor(style.stroke.value_or(Color(0, 0, 0)));
    m_noOutline = new QCheckBox(tr("Keine Kontur"), this);
    m_noOutline->setChecked(!style.stroke);
    auto* colorRow = new QHBoxLayout;
    colorRow->addWidget(m_color);
    colorRow->addWidget(m_noOutline);
    colorRow->addStretch();
    form->addRow(tr("Farbe:"), colorRow);
    m_width = new QDoubleSpinBox(this);
    m_width->setRange(0.0, 500.0);
    m_width->setDecimals(3);
    m_width->setSingleStep(0.1);
    m_width->setSuffix(QStringLiteral(" mm"));
    m_width->setValue(style.strokeWidth);
    form->addRow(tr("Breite:"), m_width);
    m_dash = new QComboBox(this);
    m_dash->setIconSize(QSize(90, 12));
    for (int i = 0; i < outline::dashCount(); ++i)
        m_dash->addItem(dashIcon(i), QString());
    m_dash->setCurrentIndex(std::clamp(style.dash, 0, outline::dashCount() - 1));
    form->addRow(tr("Stil:"), m_dash);

    auto group = [this](const QString& title, const QList<std::pair<QIcon, QString>>& items, int current, QButtonGroup** out) {
        auto* box = new QGroupBox(title, this);
        auto* col = new QVBoxLayout(box);
        auto* buttons = new QButtonGroup(this);
        for (int i = 0; i < items.size(); ++i) {
            auto* b = new QToolButton(box);
            b->setIcon(items[i].first);
            b->setIconSize(QSize(28, 20));
            b->setText(items[i].second);
            b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            b->setCheckable(true);
            b->setAutoRaise(true);
            b->setChecked(i == current);
            buttons->addButton(b, i);
            col->addWidget(b);
        }
        *out = buttons;
        return box;
    };
    auto* shapes = new QHBoxLayout;
    shapes->addWidget(group(tr("Ecken"),
                            {{joinIcon(Qt::MiterJoin), tr("Spitz")}, {joinIcon(Qt::RoundJoin), tr("Rund")}, {joinIcon(Qt::BevelJoin), tr("Abgeschrägt")}},
                            int(style.join), &m_join));
    shapes->addWidget(group(tr("Linienenden"),
                            {{capIcon(Qt::FlatCap), tr("Flach")}, {capIcon(Qt::RoundCap), tr("Rund")}, {capIcon(Qt::SquareCap), tr("Eckig")}},
                            int(style.cap), &m_cap));
    shapes->addWidget(group(tr("Konturposition"),
                            {{positionIcon(0), tr("Mittig")}, {positionIcon(1), tr("Innen")}, {positionIcon(2), tr("Aussen")}},
                            int(style.position), &m_position));
    left->addLayout(shapes);

    auto* arrows = new QGroupBox(tr("Pfeilspitzen"), this);
    auto* arrowRow = new QHBoxLayout(arrows);
    m_startArrow = new QComboBox(arrows);
    m_endArrow = new QComboBox(arrows);
    for (int i = 0; i < outline::arrowCount(); ++i) {
        m_startArrow->addItem(outline::arrowName(i));
        m_endArrow->addItem(outline::arrowName(i));
    }
    m_startArrow->setCurrentIndex(std::clamp(style.startArrow, 0, outline::arrowCount() - 1));
    m_endArrow->setCurrentIndex(std::clamp(style.endArrow, 0, outline::arrowCount() - 1));
    arrowRow->addWidget(new QLabel(tr("Anfang:"), arrows));
    arrowRow->addWidget(m_startArrow);
    arrowRow->addWidget(new QLabel(tr("Ende:"), arrows));
    arrowRow->addWidget(m_endArrow);
    left->addWidget(arrows);

    m_behind = new QCheckBox(tr("Hinter Füllung"), this);
    m_behind->setChecked(style.behindFill);
    m_scale = new QCheckBox(tr("Mit Objekt skalieren"), this);
    m_scale->setChecked(style.scaleWithObject);
    left->addWidget(m_behind);
    left->addWidget(m_scale);

    // Kalligrafie: Strecken, Winkel, Feder als Vorschau
    auto* calli = new QGroupBox(tr("Kalligrafie"), this);
    auto* cform = new QFormLayout(calli);
    m_stretch = new QDoubleSpinBox(calli);
    m_stretch->setRange(1, 100);
    m_stretch->setDecimals(0);
    m_stretch->setSuffix(QStringLiteral(" %"));
    m_stretch->setValue(style.nibStretch);
    m_angle = new QDoubleSpinBox(calli);
    m_angle->setRange(-360, 360);
    m_angle->setDecimals(1);
    m_angle->setSuffix(QStringLiteral(" °"));
    m_angle->setValue(style.nibAngle);
    cform->addRow(tr("Strecken:"), m_stretch);
    cform->addRow(tr("Winkel:"), m_angle);
    m_nib = new PaintBox(QSize(70, 70), [this](QPainter& p, const QRect& r) {
        p.translate(r.center());
        p.rotate(-m_angle->value());
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black);
        p.drawEllipse(QPointF(0, 0), 25.0, 25.0 * m_stretch->value() / 100.0);
    }, calli);
    cform->addRow(m_nib);
    auto* reset = new QPushButton(tr("Standard"), calli);
    cform->addRow(reset);
    connect(reset, &QPushButton::clicked, this, [this] {
        m_stretch->setValue(100);
        m_angle->setValue(0);
    });
    right->addWidget(calli);

    m_preview = new PaintBox(QSize(200, 140), [this](QPainter& p, const QRect& r) {
        const Style s = this->style();
        // Fläche und offene Linie
        QPainterPath area;
        area.addRoundedRect(QRectF(r.left() + 20, r.top() + 15, 90, 60), 4, 4);
        if (!s.behindFill)
            p.fillPath(area, QColor(0x9c, 0xc3, 0xef));
        else {
            Canvas::strokeStyled(p, area, s, 4.0, true);
            p.fillPath(area, QColor(0x9c, 0xc3, 0xef));
        }
        if (!s.behindFill)
            Canvas::strokeStyled(p, area, s, 4.0, true);
        QPainterPath line;
        line.moveTo(r.left() + 25, r.bottom() - 15);
        line.lineTo(r.left() + 95, r.bottom() - 45);
        line.lineTo(r.right() - 20, r.bottom() - 15);
        // Pfeilspitzen brauchen Seitenkoordinaten: Vorschau 1:4
        const QTransform view = QTransform::fromScale(4.0, 4.0);
        Canvas::strokeStyled(p, line, s, 4.0, false, view.inverted().map(line), view);
    }, this);
    right->addWidget(new QLabel(tr("Vorschau:"), this));
    right->addWidget(m_preview);
    right->addStretch();
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    right->addWidget(buttons);

    auto refresh = [this] {
        m_preview->update();
        m_nib->update();
    };
    for (QDoubleSpinBox* s : {m_width, m_stretch, m_angle})
        connect(s, &QDoubleSpinBox::valueChanged, this, refresh);
    for (QComboBox* c : {m_dash, m_startArrow, m_endArrow})
        connect(c, &QComboBox::currentIndexChanged, this, refresh);
    for (QButtonGroup* g : {m_join, m_cap, m_position})
        connect(g, &QButtonGroup::idClicked, this, refresh);
    for (QCheckBox* c : {m_behind, m_scale, m_noOutline})
        connect(c, &QCheckBox::toggled, this, refresh);
    connect(m_color, &ColorButton::colorChanged, this, [this, refresh] {
        m_noOutline->setChecked(false);
        refresh();
    });
}

Style OutlinePenDialog::style() const
{
    Style s = m_style;
    s.stroke = m_noOutline->isChecked() ? std::nullopt : std::optional<Color>(m_color->color());
    s.strokeWidth = m_width->value();
    s.dash = m_dash->currentIndex();
    s.join = Style::Join(std::max(0, m_join->checkedId()));
    s.cap = Style::Cap(std::max(0, m_cap->checkedId()));
    s.position = Style::Position(std::max(0, m_position->checkedId()));
    s.startArrow = m_startArrow->currentIndex();
    s.endArrow = m_endArrow->currentIndex();
    s.behindFill = m_behind->isChecked();
    s.scaleWithObject = m_scale->isChecked();
    s.nibStretch = m_stretch->value();
    s.nibAngle = m_angle->value();
    return s;
}

} // namespace vek

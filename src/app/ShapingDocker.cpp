#include "ShapingDocker.h"

#include "Canvas.h"
#include "PickTool.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>

namespace vek {

namespace {

using Op = Editor::ShapeOp;
constexpr std::array<Op, 7> kOps = {Op::Weld, Op::Trim, Op::Intersect, Op::Simplify, Op::FrontMinusBack, Op::BackMinusFront,
                                    Op::Boundary};

} // namespace

ShapingDocker::ShapingDocker(Editor& editor, Canvas& canvas, QWidget* parent)
    : QWidget(parent), m_editor(editor), m_canvas(canvas)
{
    // Vorgaben: Zuschneiden behält die Quelle, die Schnittmenge beide
    m_keep[std::size_t(Op::Trim)] = {true, false};
    m_keep[std::size_t(Op::Intersect)] = {true, true};

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    m_ops = new QComboBox(this);
    for (Op op : kOps)
        m_ops->addItem(Editor::shapeOpName(op));
    layout->addWidget(m_ops);
    m_preview = new QLabel(this);
    m_preview->setFrameShape(QFrame::StyledPanel);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumHeight(70);
    m_preview->setStyleSheet(QStringLiteral("background: white;"));
    layout->addWidget(m_preview);
    m_keepSource = new QCheckBox(tr("Originalquellobjekt beibehalten"), this);
    m_keepTarget = new QCheckBox(tr("Originalzielobjekt beibehalten"), this);
    layout->addWidget(m_keepSource);
    layout->addWidget(m_keepTarget);
    m_apply = new QPushButton(this);
    auto* row = new QHBoxLayout;
    row->addStretch(1);
    row->addWidget(m_apply);
    layout->addLayout(row);
    layout->addStretch(1);

    connect(m_ops, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(m_keepSource, &QCheckBox::toggled, this, [this](bool on) {
        m_keep[std::size_t(operation())].first = on;
        drawPreview();
    });
    connect(m_keepTarget, &QCheckBox::toggled, this, [this](bool on) {
        m_keep[std::size_t(operation())].second = on;
        drawPreview();
    });
    connect(m_apply, &QPushButton::clicked, this, &ShapingDocker::apply);
    connect(&m_editor, &Editor::selectionChanged, this, &ShapingDocker::refresh);
    refresh();
}

Op ShapingDocker::operation() const
{
    return kOps[std::size_t(std::max(0, m_ops->currentIndex()))];
}

void ShapingDocker::setOperation(Op op)
{
    m_ops->setCurrentIndex(int(std::find(kOps.begin(), kOps.end(), op) - kOps.begin()));
}

bool ShapingDocker::needsTarget(Op op)
{
    return op == Op::Weld || op == Op::Trim || op == Op::Intersect;
}

void ShapingDocker::refresh()
{
    const Op op = operation();
    const bool target = needsTarget(op);
    m_apply->setText(op == Op::Weld ? tr("Verschmelzen mit") : op == Op::Trim ? tr("Zuschneiden")
                     : op == Op::Intersect ? tr("Schneiden mit") : tr("Anwenden"));
    {
        const QSignalBlocker a(m_keepSource);
        const QSignalBlocker b(m_keepTarget);
        m_keepSource->setChecked(m_keep[std::size_t(op)].first);
        m_keepTarget->setChecked(m_keep[std::size_t(op)].second);
    }
    m_keepSource->setVisible(target);
    m_keepTarget->setVisible(target);
    m_apply->setEnabled(target ? !m_editor.selection().isEmpty() : m_editor.canShapeSelection(op));
    drawPreview();
}

void ShapingDocker::apply()
{
    const Op op = operation();
    if (!needsTarget(op)) {
        m_editor.shapeSelection(op);
        return;
    }
    const QList<NodeId> sources = m_editor.selection();
    if (sources.isEmpty())
        return;
    const auto [keepSource, keepTarget] = m_keep[std::size_t(op)];
    // Danach das Zielobjekt anklicken
    m_canvas.setTool(ToolId::Pick);
    m_canvas.pickTool().beginPickTarget([this, op, sources, keepSource, keepTarget](NodeId target) {
        m_editor.shapeWithTarget(op, sources, target, keepSource, keepTarget);
    });
}

void ShapingDocker::drawPreview()
{
    // Dreieck = Quelle (ausgewählt), Rechteck = Ziel, wie die Vorschau
    const Op op = operation();
    const qreal dpr = devicePixelRatioF();
    QPixmap pix(QSize(150, 66) * dpr);
    pix.setDevicePixelRatio(dpr);
    pix.fill(Qt::white);
    QPainterPath tri;
    tri.addPolygon(QPolygonF({QPointF(45, 30), QPointF(75, 30), QPointF(60, 60)}));
    tri.closeSubpath();
    QPainterPath rect;
    rect.addRect(QRectF(62, 10, 46, 30));
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor result(0x1f, 0x5f, 0xae);
    auto ghost = [&p](const QPainterPath& path) {
        QPen pen(QColor(0x80, 0x80, 0x80), 1, Qt::DashLine);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    };
    auto solid = [&p](const QPainterPath& path, const QColor& c) {
        p.setPen(QPen(c.darker(150), 1));
        p.setBrush(c);
        p.drawPath(path);
    };
    const auto [keepSource, keepTarget] = m_keep[std::size_t(op)];
    switch (op) {
    case Op::Weld:
        if (keepTarget)
            ghost(rect);
        if (keepSource)
            ghost(tri);
        solid(rect.united(tri), result);
        break;
    case Op::Trim:
        solid(rect.subtracted(tri), result);
        if (keepSource)
            solid(tri, QColor(0x60, 0x60, 0x60));
        break;
    case Op::Intersect:
        (keepTarget ? solid(rect, QColor(0xc8, 0xc8, 0xc8)) : ghost(rect));
        (keepSource ? solid(tri, QColor(0x60, 0x60, 0x60)) : ghost(tri));
        solid(rect.intersected(tri), result);
        break;
    case Op::Simplify:
        solid(rect.subtracted(tri), QColor(0x9f, 0xb8, 0xdc));
        solid(tri, result);
        break;
    case Op::FrontMinusBack:
        ghost(rect);
        solid(tri.subtracted(rect), result);
        break;
    case Op::BackMinusFront:
        ghost(tri);
        solid(rect.subtracted(tri), result);
        break;
    case Op::Boundary:
        solid(rect, QColor(0xd6, 0xdd, 0xe8));
        solid(tri, QColor(0xd6, 0xdd, 0xe8));
        p.setPen(QPen(Qt::black, 1.6));
        p.setBrush(Qt::NoBrush);
        p.drawPath(rect.united(tri));
        break;
    }
    p.end();
    m_preview->setPixmap(pix);
}

} // namespace vek

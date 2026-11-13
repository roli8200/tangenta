#include "CornersDocker.h"

#include "Canvas.h"
#include "Editor.h"
#include "ExpressionSpinBox.h"
#include "ShapeTool.h"

#include <QButtonGroup>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace vek {

CornersDocker::CornersDocker(Editor& editor, Canvas& canvas, QWidget* parent)
    : QWidget(parent), m_editor(editor), m_canvas(canvas)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    m_ops = new QButtonGroup(this);
    const QList<std::pair<pathops::CornerOp, QString>> ops = {
        {pathops::CornerOp::Fillet, tr("Abrunden")},
        {pathops::CornerOp::Scallop, tr("Ausbuchten")},
        {pathops::CornerOp::Chamfer, tr("Abschrägen")}};
    for (const auto& op : ops) {
        auto* b = new QRadioButton(op.second, this);
        m_ops->addButton(b, int(op.first));
        layout->addWidget(b);
    }
    m_ops->button(int(pathops::CornerOp::Fillet))->setChecked(true);

    auto* form = new QFormLayout;
    m_radiusLabel = new QLabel(this);
    m_radius = new ExpressionSpinBox(this);
    m_radius->setDecimals(3);
    m_radius->setRange(0.001, 10000.0);
    m_radius->setValue(5.0);
    m_radius->setSuffix(QStringLiteral(" mm"));
    form->addRow(m_radiusLabel, m_radius);
    layout->addLayout(form);

    m_scope = new QLabel(this);
    m_scope->setWordWrap(true);
    layout->addWidget(m_scope);
    auto* hint = new QLabel(tr("Grundformen werden dabei in Kurven umgewandelt."), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_apply = new QPushButton(tr("Anwenden"), this);
    auto* row = new QHBoxLayout;
    row->addStretch(1);
    row->addWidget(m_apply);
    layout->addLayout(row);
    layout->addStretch(1);

    connect(m_ops, &QButtonGroup::idToggled, this, [this](int, bool on) {
        if (on)
            refresh();
    });
    connect(m_apply, &QPushButton::clicked, this, [this] { apply(); });
    connect(&m_editor, &Editor::selectionChanged, this, &CornersDocker::refresh);
    connect(&m_editor, &Editor::documentChanged, this, &CornersDocker::refresh);
    connect(&m_canvas, &Canvas::toolStateChanged, this, &CornersDocker::refresh);
    connect(&m_canvas, &Canvas::toolChanged, this, &CornersDocker::refresh);
    refresh();
}

pathops::CornerOp CornersDocker::operation() const
{
    return pathops::CornerOp(std::max(0, m_ops->checkedId()));
}

void CornersDocker::setOperation(pathops::CornerOp op)
{
    m_ops->button(int(op))->setChecked(true);
}

double CornersDocker::radius() const
{
    return m_radius->value();
}

void CornersDocker::setRadius(double mm)
{
    m_radius->setValue(mm);
}

QList<NodeRef> CornersDocker::selectedNodes() const
{
    // Gewählte Knoten des Formwerkzeugs, nur wenn es gerade eine Kurve bearbeitet
    if (m_canvas.toolId() != ToolId::Shape)
        return {};
    const ShapeTool& shape = m_canvas.shapeTool();
    return shape.curve() ? shape.selectedNodes() : QList<NodeRef>{};
}

bool CornersDocker::apply()
{
    return m_editor.applyCorners(operation(), radius(), selectedNodes());
}

void CornersDocker::refresh()
{
    m_radiusLabel->setText(operation() == pathops::CornerOp::Chamfer ? tr("Abstand:") : tr("Radius:"));
    const bool nodes = !selectedNodes().isEmpty();
    m_scope->setText(nodes ? tr("Wirkt auf die gewählten Knoten.") : tr("Wirkt auf alle spitzen Ecken der Auswahl."));
    m_apply->setEnabled(m_editor.canApplyCorners());
}

} // namespace vek

#include "PerfectShapeTool.h"

#include "Canvas.h"
#include "Editor.h"
#include "Options.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>

#include <cmath>

namespace vek {

namespace {

constexpr double kGlyphHandle = 5.0;

QString optionKey(perfect::Category c)
{
    return QStringLiteral("ws/tools/perfect/%1").arg(int(c));
}

} // namespace

int PerfectShapeTool::currentKind(perfect::Category category)
{
    const QList<int> kinds = perfect::kinds(category);
    const QVariant v = Options::instance().value(optionKey(category));
    const int k = v.isValid() ? v.toInt() : -1;
    return kinds.contains(k) ? k : kinds.value(0, 0);
}

void PerfectShapeTool::setCurrentKind(perfect::Category category, int kind)
{
    Options::instance().set(optionKey(category), kind);
}

QIcon PerfectShapeTool::icon(int kind)
{
    QPixmap pm(32, 24);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path = perfect::unitPath(kind, perfect::info(kind).glyph);
    // Legenden reichen über das Quadrat hinaus: in die Fläche einpassen
    const QRectF b = path.boundingRect();
    const QRectF to(3, 2, 26, 20);
    path = (QTransform::fromTranslate(-b.left(), -b.top()) * QTransform::fromScale(to.width() / b.width(), to.height() / b.height())
            * QTransform::fromTranslate(to.left(), to.top()))
               .map(path);
    p.setPen(QPen(QColor(0x1f, 0x5f, 0xae), 1.0));
    p.setBrush(QColor(0x9c, 0xc3, 0xef));
    p.drawPath(path);
    return QIcon(pm);
}

void PerfectShapeTool::activate()
{
    m_canvas.setCursor(Qt::CrossCursor);
    m_drag = Drag::None;
}

void PerfectShapeTool::deactivate()
{
    m_drag = Drag::None;
}

const PerfectShapeObject* PerfectShapeTool::selectedShape() const
{
    const QList<const DrawObject*> sel = m_canvas.editor().selectedObjects();
    return sel.size() == 1 ? node_cast<PerfectShapeObject>(sel.first()) : nullptr;
}

void PerfectShapeTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_press = m_current = doc;
    if (const PerfectShapeObject* s = selectedShape(); s && perfect::info(s->kind()).hasGlyph) {
        const QPointF h = m_canvas.toScreen(s->transform().map(s->glyphLocal()));
        if (QLineF(h, event->position()).length() <= kGlyphHandle + 3) {
            m_drag = Drag::Glyph;
            m_glyphPreview = s->glyph();
            return;
        }
    }
    m_drag = Drag::Create;
}

void PerfectShapeTool::mouseMove(QMouseEvent* event, const QPointF& doc)
{
    if (m_drag == Drag::None)
        return;
    m_current = doc;
    if (event->modifiers() & Qt::ControlModifier && m_drag == Drag::Create) {
        // Strg: gleich breit wie hoch
        const QPointF d = doc - m_press;
        const double side = std::max(std::abs(d.x()), std::abs(d.y()));
        m_current = m_press + QPointF(d.x() < 0 ? -side : side, d.y() < 0 ? -side : side);
    }
    if (m_drag == Drag::Glyph) {
        if (const PerfectShapeObject* s = selectedShape()) {
            const QPointF local = s->transform().inverted().map(doc);
            m_glyphPreview = QPointF(local.x() / std::max(1e-9, s->size().width()), local.y() / std::max(1e-9, s->size().height()));
        }
    }
    m_canvas.update();
}

void PerfectShapeTool::mouseRelease(QMouseEvent* event, const QPointF&)
{
    if (event->button() != Qt::LeftButton || m_drag == Drag::None)
        return;
    const Drag drag = m_drag;
    m_drag = Drag::None;
    Editor& ed = m_canvas.editor();
    if (drag == Drag::Glyph) {
        if (const PerfectShapeObject* s = selectedShape()) {
            const QPointF g = m_glyphPreview;
            ed.modifyObject(s->id(), [g](DrawObject& o) {
                if (auto* shape = node_cast<PerfectShapeObject>(&o))
                    shape->setGlyph(g);
            }, QObject::tr("Form verändern"));
        }
        m_canvas.update();
        return;
    }
    const QRectF r = QRectF(m_press, m_current).normalized();
    if (m_canvas.toScreen(r.bottomRight()).x() - m_canvas.toScreen(r.topLeft()).x() < 3.0) {
        m_canvas.update();
        return;
    }
    auto shape = std::make_unique<PerfectShapeObject>(currentKind(m_category), r.size());
    shape->setTransform(QTransform::fromTranslate(r.left(), r.top()));
    shape->setStyle(ed.defaultStyle());
    ed.addObject(std::move(shape), QObject::tr("Perfekte Form"));
    m_canvas.update();
}

void PerfectShapeTool::paintOverlay(QPainter& p)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const QTransform view = m_canvas.viewTransform();
    if (m_drag == Drag::Create) {
        const QRectF r = QRectF(m_press, m_current).normalized();
        const QPainterPath unit = perfect::unitPath(currentKind(m_category), perfect::info(currentKind(m_category)).glyph);
        p.setPen(QPen(Canvas::previewColor(), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawPath(view.map((QTransform::fromScale(r.width(), r.height()) * QTransform::fromTranslate(r.left(), r.top())).map(unit)));
    }
    if (const PerfectShapeObject* s = selectedShape(); s && perfect::info(s->kind()).hasGlyph) {
        const QTransform toScreen = s->transform() * view;
        if (m_drag == Drag::Glyph) {
            PerfectShapeObject preview(s->kind(), s->size());
            preview.setGlyph(m_glyphPreview);
            p.setPen(QPen(Canvas::previewColor(), 1.0));
            p.setBrush(Qt::NoBrush);
            p.drawPath(toScreen.map(preview.localPath()));
        }
        // Roter Glyphengriff (Raute)
        const QPointF g = m_drag == Drag::Glyph ? m_glyphPreview : s->glyph();
        const QPointF h = toScreen.map(QPointF(g.x() * s->size().width(), g.y() * s->size().height()));
        QPolygonF diamond({h + QPointF(0, -kGlyphHandle), h + QPointF(kGlyphHandle, 0), h + QPointF(0, kGlyphHandle), h + QPointF(-kGlyphHandle, 0)});
        p.setPen(QPen(Qt::black, 1.0));
        p.setBrush(QColor(0xe0, 0x20, 0x20));
        p.drawPolygon(diamond);
    }
    p.restore();
}

} // namespace vek

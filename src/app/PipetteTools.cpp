#include "PipetteTools.h"

#include "Canvas.h"
#include "DrawObject.h"
#include "Icons.h"
#include "Options.h"
#include "Serialization.h"

#include "ColorDialog.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QWidget>

#include <functional>

namespace vek {

// --- Farbpipette ----------------------------------------------------------------------------

void ColorPipetteTool::activate()
{
    m_color.reset();
    updateCursor();
}

void ColorPipetteTool::deactivate()
{
    m_color.reset();
}

namespace {

// Durchsichtiges Fenster über allen Bildschirmen: fängt den Klick ab, damit man auch ausserhalb
// von TANGENTA eine Farbe aufnehmen kann (wie "Auswahl vom Desktop")
class DesktopPicker final : public QWidget {
public:
    explicit DesktopPicker(std::function<void(std::optional<Color>)> done)
        : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool), m_done(std::move(done))
    {
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowOpacity(0.01);
        QRect all;
        for (QScreen* s : QGuiApplication::screens())
            all = all.united(s->geometry());
        setGeometry(all);
        setCursor(QCursor(icon(QStringLiteral("tool-pipette")).pixmap(24), 2, 21));
    }

protected:
    void mousePressEvent(QMouseEvent* e) override
    {
        const QPoint at = e->globalPosition().toPoint();
        hide(); // das eigene Fenster nicht mit aufnehmen
        finish(e->button() == Qt::LeftButton ? ColorDialog::screenColorAt(at) : std::nullopt);
    }
    void keyPressEvent(QKeyEvent* e) override
    {
        if (e->key() == Qt::Key_Escape)
            finish(std::nullopt);
    }

private:
    void finish(std::optional<Color> c)
    {
        if (m_done) {
            const auto done = std::move(m_done);
            m_done = nullptr;
            done(c);
        }
        close();
    }
    std::function<void(std::optional<Color>)> m_done;
};

} // namespace

void ColorPipetteTool::setColor(const std::optional<Color>& color)
{
    m_color = color;
    updateCursor();
    m_canvas.update();
}

void ColorPipetteTool::pickFromDesktop()
{
    auto* picker = new DesktopPicker([this](std::optional<Color> c) {
        if (c)
            setColor(c);
    });
    picker->show();
    picker->raise();
    picker->activateWindow();
}

void ColorPipetteTool::updateCursor()
{
    m_canvas.setCursor(QCursor(icon(m_color ? QStringLiteral("fill-bucket") : QStringLiteral("tool-pipette")).pixmap(24), 2, 21));
    m_canvas.notifyToolState();
}

Color ColorPipetteTool::sample(const QPointF& doc) const
{
    // Die Seite an dieser Stelle fein rendern und das mittlere Pixel lesen
    const double pxPerMm = 8.0;
    QImage img(3, 3, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    {
        QPainter p(&img);
        m_canvas.renderPage(p, m_canvas.editor().currentPageIndex(), pxPerMm, QPointF(1.5, 1.5) - doc * pxPerMm, true);
    }
    return Color::fromQColor(img.pixelColor(1, 1));
}

void ColorPipetteTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_last = doc;
    // Strg: immer neu aufnehmen, sonst im Anwenden-Modus auf das Objekt
    if (!m_color || (event->modifiers() & Qt::ControlModifier)) {
        m_color = sample(doc);
        updateCursor();
        m_canvas.update();
        return;
    }
    Editor& ed = m_canvas.editor();
    const double tol = m_canvas.pixelsToDoc(4.0);
    const DrawObject* hit = ed.objectAt(doc, tol);
    if (!hit)
        return;
    // Auf dem Rand (ausserhalb der Fläche) oder mit Umschalt: Konturfarbe
    const bool onEdge = (event->modifiers() & Qt::ShiftModifier) || !hit->path().contains(doc);
    if (onEdge)
        ed.setObjectStroke(hit->id(), *m_color, QObject::tr("Konturfarbe"));
    else
        ed.setObjectFill(hit->id(), Fill::solid(*m_color), QObject::tr("Füllung"));
}

bool ColorPipetteTool::keyPress(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && m_color) {
        m_color.reset();
        updateCursor();
        m_canvas.update();
        return true;
    }
    return false;
}

void ColorPipetteTool::paintOverlay(QPainter& p)
{
    if (!m_color)
        return;
    // Aufgenommene Farbe als Musterfeld neben der Aufnahmestelle
    const QPointF s = m_canvas.toScreen(m_last) + QPointF(12, 12);
    p.save();
    p.setPen(QPen(Qt::black, 1));
    p.setBrush(m_color->toQColor());
    p.drawRect(QRectF(s, QSizeF(18, 18)));
    p.restore();
}

// --- Attributpipette ------------------------------------------------------------------------

AttributePipetteTool::AttributePipetteTool(Canvas& canvas) : Tool(canvas) {}

AttributePipetteTool::~AttributePipetteTool() = default;

int AttributePipetteTool::what()
{
    const Options& o = Options::instance();
    return (o.flag(QStringLiteral("ws/tools/attrPipette/properties")) ? Properties : 0)
           | (o.flag(QStringLiteral("ws/tools/attrPipette/transform")) ? Transformations : 0)
           | (o.flag(QStringLiteral("ws/tools/attrPipette/effects")) ? Effects : 0);
}

void AttributePipetteTool::activate()
{
    m_source.reset();
    updateCursor();
}

void AttributePipetteTool::deactivate()
{
    m_source.reset();
}

void AttributePipetteTool::updateCursor()
{
    m_canvas.setCursor(QCursor(icon(m_source ? QStringLiteral("fill-bucket") : QStringLiteral("tool-attr-pipette")).pixmap(24), 2, 21));
    m_canvas.notifyToolState();
}

void AttributePipetteTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    Editor& ed = m_canvas.editor();
    const DrawObject* hit = ed.objectAt(doc, m_canvas.pixelsToDoc(4.0));
    if (!hit)
        return;
    if (!m_source || (event->modifiers() & Qt::ControlModifier)) {
        std::unique_ptr<Node> copy = serial::fromJson(serial::toJson(*hit, serial::Ids::Drop), serial::Ids::Drop);
        if (auto* d = node_cast<DrawObject>(copy.get())) {
            copy.release();
            m_source.reset(d);
        }
        updateCursor();
        return;
    }
    ed.applyAttributes(*m_source, hit->id(), what());
}

bool AttributePipetteTool::keyPress(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && m_source) {
        m_source.reset();
        updateCursor();
        return true;
    }
    return false;
}

} // namespace vek

#include "PickTool.h"

#include "Canvas.h"
#include "Editor.h"
#include "TextObject.h"
#include "Options.h"
#include "TextTool.h"

#include <QMenu>
#include <QPixmap>
#include <QtMath>

#include <cmath>

namespace vek {

namespace {

constexpr double kHandleOffset = 7.0; // Abstand der Griffe vom Begrenzungsrechteck, px
constexpr double kHandleSize = 6.0;
constexpr double kHandleHit = 6.0;
constexpr double kDragThreshold = 3.0;
double angleStep()
{
    return std::max(0.1, Options::instance().number(QStringLiteral("ws/edit/angleStep")));
}

bool isCorner(int handle)
{
    return handle % 2 == 0;
}

double snapAngle(double degrees)
{
    return std::round(degrees / angleStep()) * angleStep();
}

// Strg beim Skalieren: ganzzahlige Vielfache (100 %, 200 %, -100 % = spiegeln)
double snapScale(double s)
{
    double r = std::round(s);
    if (r == 0.0)
        r = s < 0.0 ? -1.0 : 1.0;
    return r;
}

// Hilfslinie um einen freien Drehpunkt auf den Winkel angle (gegen den Uhrzeigersinn) drehen:
// der Ankerpunkt läuft mit, damit die Linie ihren Abstand zum Drehpunkt behält
Guide rotatedGuide(const Guide& start, const QPointF& pivot, double angle)
{
    const double t = qDegreesToRadians(start.angle - angle); // im Bild (y nach unten) im Uhrzeigersinn
    const QPointF v = start.anchor - pivot;
    Guide g = start;
    g.anchor = pivot + QPointF(v.x() * std::cos(t) - v.y() * std::sin(t), v.x() * std::sin(t) + v.y() * std::cos(t));
    g.angle = angle;
    return g;
}

double normalizeDegrees(double d)
{
    while (d > 180.0)
        d -= 360.0;
    while (d < -180.0)
        d += 360.0;
    return d;
}

void drawArrowHead(QPainter& p, const QPointF& tip, QPointF dir)
{
    const double len = std::hypot(dir.x(), dir.y());
    if (len == 0.0)
        return;
    dir /= len;
    const QPointF perp(-dir.y(), dir.x());
    const QPointF base = tip - dir * 4.0;
    p.drawPolygon(QPolygonF{tip, base + perp * 2.5, base - perp * 2.5});
}

// Gebogener Doppelpfeil an einer Ecke im Drehmodus
void drawRotateGlyph(QPainter& p, const QPointF& at, const QPointF& outward)
{
    const double theta = std::atan2(outward.y(), outward.x());
    const double radius = 10.0;
    const QPointF c = at - outward * 8.0;
    const double span = qDegreesToRadians(55.0);
    QPolygonF arc;
    for (int i = 0; i <= 10; ++i) {
        const double t = theta - span + 2.0 * span * i / 10.0;
        arc << c + QPointF(std::cos(t), std::sin(t)) * radius;
    }
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(arc);
    p.setBrush(Qt::black);
    drawArrowHead(p, arc.first(), arc.first() - arc[1]);
    drawArrowHead(p, arc.last(), arc.last() - arc[arc.size() - 2]);
}

// Gerader Doppelpfeil an einer Seite im Drehmodus (Neigen)
void drawSkewGlyph(QPainter& p, const QPointF& at, bool horizontal)
{
    const QPointF d = horizontal ? QPointF(6, 0) : QPointF(0, 6);
    p.drawLine(at - d, at + d);
    p.setBrush(Qt::black);
    drawArrowHead(p, at + d, d);
    drawArrowHead(p, at - d, -d);
}

QCursor rotateCursor()
{
    static const QCursor cursor = [] {
        QPixmap pm(24, 24);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        for (const auto& [color, width] : {std::pair{QColor(Qt::white), 3.5}, std::pair{QColor(Qt::black), 1.5}}) {
            p.setPen(QPen(color, width));
            p.setBrush(Qt::NoBrush);
            p.drawArc(QRectF(5, 5, 14, 14), 100 * 16, 290 * 16);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black);
        p.drawPolygon(QPolygonF{QPointF(10, 1), QPointF(15, 5), QPointF(10, 9)});
        return QCursor(pm, 12, 12);
    }();
    return cursor;
}

} // namespace

PickTool::PickTool(Canvas& canvas)
    : Tool(canvas)
{
    // Neue Auswahl beginnt immer im Skaliermodus, Drehpunkt in der Mitte
    QObject::connect(&canvas.editor(), &Editor::selectionChanged, &canvas, [this] {
        m_mode = Mode::Scale;
        m_centerSet = false;
        if (!editor().selection().isEmpty())
            m_canvas.setSelectedGuide(-1);
    });
}

Editor& PickTool::editor() const
{
    return m_canvas.editor();
}

void PickTool::activate()
{
    m_canvas.setCursor(Qt::ArrowCursor);
}

void PickTool::deactivate()
{
    cancelDrag();
}

QPointF PickTool::handleDocPoint(int handle, const QRectF& b) const
{
    switch (handle) {
    case TopLeft: return b.topLeft();
    case Top: return QPointF(b.center().x(), b.top());
    case TopRight: return b.topRight();
    case Right: return QPointF(b.right(), b.center().y());
    case BottomRight: return b.bottomRight();
    case Bottom: return QPointF(b.center().x(), b.bottom());
    case BottomLeft: return b.bottomLeft();
    case Left: return QPointF(b.left(), b.center().y());
    default: return b.center();
    }
}

QPointF PickTool::handleScreenPoint(int handle, const QRectF& bounds) const
{
    const QRectF r = m_canvas.viewTransform().mapRect(bounds).adjusted(-kHandleOffset, -kHandleOffset, kHandleOffset,
                                                                        kHandleOffset);
    return handleDocPoint(handle, r);
}

int PickTool::handleAt(const QPointF& screen) const
{
    if (editor().selection().isEmpty())
        return NoHandle;
    const QRectF bounds = editor().selectionBounds();
    for (int h = 0; h < HandleCount; ++h) {
        const QPointF d = handleScreenPoint(h, bounds) - screen;
        if (std::abs(d.x()) <= kHandleHit && std::abs(d.y()) <= kHandleHit)
            return h;
    }
    return NoHandle;
}

bool PickTool::hitsCenter(const QPointF& screen) const
{
    const QPointF d = m_canvas.toScreen(rotationCenter()) - screen;
    return std::hypot(d.x(), d.y()) <= 7.0;
}

QPointF PickTool::rotationCenter() const
{
    return m_centerSet ? m_center : editor().selectionBounds().center();
}

void PickTool::beginPickTarget(std::function<void(NodeId)> done)
{
    m_placeInFront.reset();
    m_pickTarget = std::move(done);
    m_canvas.setCursor(Qt::PointingHandCursor);
}

void PickTool::beginPlaceRelative(bool inFront)
{
    if (editor().selection().isEmpty())
        return;
    m_placeInFront = inFront;
    m_canvas.setCursor(Qt::PointingHandCursor);
}

bool PickTool::selectionFullyLocked() const
{
    const Editor& ed = editor();
    return !ed.selection().isEmpty() && !ed.selectionHasUnlocked();
}

void PickTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    Editor& ed = editor();
    if (event->button() == Qt::RightButton && m_canvas.guidesVisible()) {
        if (const int guide = m_canvas.guideAt(event->position()); guide >= 0 && !ed.objectAt(doc, m_canvas.pixelsToDoc(4.0))) {
            m_canvas.setSelectedGuide(guide);
            emit m_canvas.guideContextMenuRequested(guide, event->globalPosition().toPoint());
            return;
        }
    }
    if (event->button() == Qt::RightButton) {
        // Kontextmenü: ein nicht gewähltes Objekt unter der Maus wird gewählt.
        // Das Menü kommt beim Loslassen; zieht man vorher, wird es das Menü "Hierher kopieren …"
        const DrawObject* hit = ed.objectAt(doc, m_canvas.pixelsToDoc(4.0));
        if (hit && !ed.isSelected(hit->id()))
            ed.setSelection({hit->id()});
        if (ed.selection().isEmpty())
            return;
        m_pressDoc = m_currentDoc = doc;
        m_pressScreen = event->position();
        m_preview = QTransform();
        m_drag = hit && !selectionFullyLocked() ? Drag::RightPending : Drag::None;
        if (m_drag == Drag::RightPending)
            prepareSnapping();
        else
            emit m_canvas.contextMenuRequested(event->globalPosition().toPoint());
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;
    if (m_pickTarget) {
        const std::function<void(NodeId)> done = std::move(m_pickTarget);
        m_pickTarget = nullptr;
        m_canvas.setCursor(Qt::ArrowCursor);
        if (const DrawObject* target = ed.objectAt(doc, m_canvas.pixelsToDoc(4.0)))
            done(target->id());
        return;
    }
    if (m_placeInFront) {
        const bool inFront = *m_placeInFront;
        m_placeInFront.reset();
        m_canvas.setCursor(Qt::ArrowCursor);
        const DrawObject* target = ed.objectAt(doc, m_canvas.pixelsToDoc(4.0));
        if (target && !ed.isSelected(target->id()))
            ed.placeSelectionRelativeTo(target->id(), inFront);
        return;
    }
    m_pressDoc = doc;
    m_currentDoc = doc;
    m_pressScreen = event->position();
    m_preview = QTransform();
    m_clickOnSelected = false;

    if (const int sel = m_canvas.selectedGuide(); sel >= 0 && m_guideRotateMode) {
        const Guide g = ed.guides().value(sel);
        const QPointF pos = event->position();
        QPointF a;
        QPointF b;
        const QPointF pivot = m_canvas.toScreen(guidePivot(g));
        if (std::hypot(pos.x() - pivot.x(), pos.y() - pivot.y()) <= 7.0) {
            m_drag = Drag::GuideAnchor;
        } else if (m_canvas.guideSegment(g, 16.0, &a, &b)
                   && (std::hypot(pos.x() - a.x(), pos.y() - a.y()) <= 9.0
                       || std::hypot(pos.x() - b.x(), pos.y() - b.y()) <= 9.0)) {
            m_drag = Drag::GuideRotate;
        }
        if (m_drag == Drag::GuideAnchor || m_drag == Drag::GuideRotate) {
            m_guide = sel;
            m_guideStart = m_guideCurrent = g;
            m_guidePivotStart = guidePivot(g);
            return;
        }
    }

    // Untere Lasche eines ausgewählten Mengentextrahmens: verknüpfen
    if (const QList<TextFrameGeometry> frames = selectedTextChain(); !frames.isEmpty()) {
        if (const int tab = m_canvas.textLinkTabAt(event->position(), frames); tab >= 0) {
            const NodeId from = frames[tab].id;
            m_canvas.setTool(ToolId::Text);
            m_canvas.textTool().beginLinking(from, true);
            return;
        }
    }

    if (!ed.selection().isEmpty() && !selectionFullyLocked()) {
        if (m_mode == Mode::Rotate && hitsCenter(event->position())) {
            m_drag = Drag::Center;
            return;
        }
        const int h = handleAt(event->position());
        if (h != NoHandle) {
            m_drag = Drag::Handle;
            m_handle = h;
            m_dragBounds = ed.selectionBounds();
            prepareSnapping();
            return;
        }
    }

    const bool shift = event->modifiers() & Qt::ShiftModifier;
    const DrawObject* hit = ed.objectAt(doc, m_canvas.pixelsToDoc(4.0));
    if (hit) {
        const bool wasSelected = ed.isSelected(hit->id());
        if (shift) {
            QList<NodeId> ids = ed.selection();
            if (wasSelected)
                ids.removeAll(hit->id());
            else
                ids.append(hit->id());
            ed.setSelection(ids);
        } else if (!wasSelected) {
            ed.setSelection({hit->id()});
        } else {
            m_clickOnSelected = true;
        }
        // Gesperrte Objekte bleiben stehen
        m_drag = selectionFullyLocked() ? Drag::None : Drag::Pending;
        m_dragBounds = ed.selectionBounds();
        prepareSnapping();
    } else if (const int guide = m_canvas.guideAt(event->position()); guide >= 0) {
        m_guideClickOnSelected = guide == m_canvas.selectedGuide();
        if (!m_guideClickOnSelected) {
            m_guideRotateMode = false;
            m_guidePivot.reset();
        }
        m_canvas.setSelectedGuide(guide);
        m_guide = guide;
        m_guideStart = m_guideCurrent = ed.guides().at(guide);
        m_guideMoved = false;
        m_drag = Drag::Guide;
    } else {
        m_guideRotateMode = false;
        m_guidePivot.reset();
        m_canvas.setSelectedGuide(-1);
        m_addToSelection = shift;
        if (!shift)
            ed.clearSelection();
        m_drag = Drag::Marquee;
    }
    m_canvas.update();
}

void PickTool::prepareSnapping()
{
    const Editor& ed = editor();
    m_snapEngine.emplace(m_canvas, ed.selection());
    m_snapSources.clear();
    for (const DrawObject* o : ed.selectedObjects()) {
        const QPainterPath path = o->snapPath();
        for (int i = 0; i < path.elementCount() && m_snapSources.size() < 400; ++i) {
            const QPainterPath::Element e = path.elementAt(i);
            if (e.isMoveTo() || e.isLineTo()) {
                m_snapSources.append(e);
            } else if (e.isCurveTo() && i + 2 < path.elementCount()) {
                m_snapSources.append(path.elementAt(i + 2));
                i += 2;
            }
        }
    }
    const QRectF b = ed.selectionBounds();
    m_snapSources << b.center() << b.topLeft() << b.topRight() << b.bottomLeft() << b.bottomRight();
}

void PickTool::mouseMove(QMouseEvent* event, const QPointF& doc)
{
    m_hover = doc;
    if (Options::instance().flag(QStringLiteral("ws/tools/pick/crosshair")))
        m_canvas.update();
    if ((m_drag == Drag::Move || m_drag == Drag::Handle) && !m_dragTimer.isValid())
        m_dragTimer.start();
    m_currentDoc = doc;
    switch (m_drag) {
    case Drag::None:
        updateHoverCursor(event->position(), doc);
        return;
    case Drag::Pending: {
        const QPointF d = event->position() - m_pressScreen;
        if (std::abs(d.x()) + std::abs(d.y()) < kDragThreshold || editor().selection().isEmpty())
            return;
        m_drag = Drag::Move;
        m_canvas.setCursor(Qt::SizeAllCursor);
        break;
    }
    case Drag::RightPending: {
        const QPointF d = event->position() - m_pressScreen;
        if (std::abs(d.x()) + std::abs(d.y()) < kDragThreshold)
            return;
        m_drag = Drag::RightMove;
        m_canvas.setCursor(Qt::SizeAllCursor);
        break;
    }
    case Drag::Center:
        m_center = doc;
        m_centerSet = true;
        m_canvas.update();
        return;
    case Drag::Guide: {
        const QPointF d = event->position() - m_pressScreen;
        if (!m_guideMoved && std::abs(d.x()) + std::abs(d.y()) < kDragThreshold)
            return;
        m_guideMoved = true;
        // Verschieben senkrecht zur Linie; der Anker gleitet nicht entlang der Linie
        const QPointF n = m_guideStart.normal();
        m_guideCurrent = m_guideStart;
        m_guideCurrent.anchor += n * QPointF::dotProduct(doc - m_pressDoc, n);
        m_canvas.setGuidePreview(m_guideCurrent);
        return;
    }
    case Drag::GuideRotate: {
        const QPointF c = m_guidePivotStart;
        const QPointF a = m_pressDoc - c;
        const QPointF b = doc - c;
        // Bildschirmwinkel (y nach unten) im Uhrzeigersinn; Hilfslinienwinkel gegen den Uhrzeigersinn
        const double turned = qRadiansToDegrees(std::atan2(b.y(), b.x()) - std::atan2(a.y(), a.x()));
        double angle = normalizeDegrees(m_guideStart.angle - turned);
        if (event->modifiers() & Qt::ControlModifier)
            angle = snapAngle(angle);
        m_guideCurrent = rotatedGuide(m_guideStart, c, angle);
        m_canvas.setGuidePreview(m_guideCurrent);
        return;
    }
    case Drag::GuideAnchor:
        // Drehpunkt frei verschieben, auch neben die Linie; die Hilfslinie selbst bleibt
        m_guidePivot = doc;
        m_canvas.update();
        return;
    default:
        break;
    }
    if (m_drag == Drag::Move || m_drag == Drag::Handle || m_drag == Drag::RightMove)
        m_preview = computePreview(doc, event->modifiers());
    m_canvas.update();
}

void PickTool::mouseDoubleClick(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    const auto* text = node_cast<TextObject>(editor().objectAt(doc, m_canvas.pixelsToDoc(3.0)));
    if (!text || editor().isLocked(text->id()))
        return;
    m_drag = Drag::None;
    const NodeId id = text->id();
    m_canvas.setTool(ToolId::Text);
    m_canvas.textTool().beginEditing(id, doc);
}

void PickTool::rightRelease(QMouseEvent* event, const QPointF& doc)
{
    Editor& ed = editor();
    const Drag drag = m_drag;
    const QTransform move = drag == Drag::RightMove ? computePreview(doc, event->modifiers()) : QTransform();
    m_drag = Drag::None;
    m_preview = QTransform();
    m_snapEngine.reset();
    m_snapResult = SnapResult();
    m_canvas.setCursor(Qt::ArrowCursor);
    m_canvas.update();
    if (drag == Drag::RightPending) {
        emit m_canvas.contextMenuRequested(event->globalPosition().toPoint());
        return;
    }
    if (drag != Drag::RightMove)
        return;
    // Ziel: oberstes nicht ausgewähltes Objekt unter der Maus
    NodeId target = 0;
    const QList<const DrawObject*> objects = ed.pickableObjects();
    for (auto it = objects.rbegin(); it != objects.rend(); ++it)
        if (!ed.isSelected((*it)->id()) && (*it)->hitTest(doc, m_canvas.pixelsToDoc(4.0), ed.treatAllObjectsAsFilled())) {
            target = (*it)->id();
            break;
        }
    QMenu menu(&m_canvas);
    auto add = [&](const QString& text, DropAction action, bool needsTarget) {
        QAction* a = menu.addAction(text);
        a->setData(int(action));
        a->setEnabled(!needsTarget || target != 0);
    };
    add(QObject::tr("Hierher &verschieben"), DropAction::Move, false);
    add(QObject::tr("Hierher &kopieren"), DropAction::Copy, false);
    menu.addSeparator();
    add(QObject::tr("&Füllung hierher kopieren"), DropAction::CopyFill, true);
    add(QObject::tr("K&ontur hierher kopieren"), DropAction::CopyOutline, true);
    add(QObject::tr("&Alle Eigenschaften hierher kopieren"), DropAction::CopyAll, true);
    menu.addSeparator();
    add(QObject::tr("&SuperClip innen"), DropAction::SuperClipInside, true);
    menu.addSeparator();
    menu.addAction(QObject::tr("Abbrechen"));
    if (const QAction* chosen = menu.exec(event->globalPosition().toPoint()); chosen && chosen->data().isValid())
        rightDrop(DropAction(chosen->data().toInt()), target, move);
}

void PickTool::rightDrop(DropAction action, NodeId target, const QTransform& move)
{
    Editor& ed = editor();
    const QList<NodeId> sel = ed.selection();
    if (sel.isEmpty())
        return;
    const int pen = Editor::CopyOutlinePen | Editor::CopyOutlineColor;
    switch (action) {
    case DropAction::Move:
        if (!move.isIdentity())
            ed.transformSelection(move, QObject::tr("Verschieben"));
        break;
    case DropAction::Copy: ed.duplicateSelectionWith(move, QObject::tr("Kopieren")); break;
    case DropAction::CopyFill: ed.copyPropertiesTo(sel.first(), target, Editor::CopyFill); break;
    case DropAction::CopyOutline: ed.copyPropertiesTo(sel.first(), target, pen); break;
    case DropAction::CopyAll: ed.copyPropertiesTo(sel.first(), target, pen | Editor::CopyFill | Editor::CopyText); break;
    case DropAction::SuperClipInside: ed.placeInsideFrame(target); break;
    }
}

void PickTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() == Qt::RightButton) {
        rightRelease(event, doc);
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;
    Editor& ed = editor();
    switch (m_drag) {
    case Drag::Pending:
        // Zweiter Klick auf die bestehende Auswahl: Skalieren <-> Drehen/Neigen
        if (m_clickOnSelected)
            m_mode = m_mode == Mode::Scale ? Mode::Rotate : Mode::Scale;
        break;
    case Drag::Move:
    case Drag::Handle:
        m_preview = computePreview(doc, event->modifiers());
        if (Options::instance().integer(QStringLiteral("ws/tools/pick/modifiers")) == 1
            && (event->modifiers() & Qt::ControlModifier) && !m_preview.isIdentity())
            ed.duplicateSelectionWith(m_preview, QObject::tr("Duplizieren"));
        else
            commitPreview();
        break;
    case Drag::Marquee: {
        // Nur vollständig umschlossene Objekte
        const QRectF area = QRectF(m_pressDoc, doc).normalized();
        QList<NodeId> ids = m_addToSelection ? ed.selection() : QList<NodeId>();
        for (const DrawObject* o : ed.pickableObjects()) {
            if (area.contains(o->boundingRect()) && !ids.contains(o->id()))
                ids.append(o->id());
        }
        ed.setSelection(ids, true);
        break;
    }
    case Drag::Guide: {
        m_canvas.setGuidePreview(std::nullopt);
        if (!m_guideMoved) {
            // Zweiter Klick auf die ausgewählte Hilfslinie: Drehgriffe ein/aus
            if (m_guideClickOnSelected) {
                m_guideRotateMode = !m_guideRotateMode;
                m_guidePivot.reset();
            }
        } else if (!m_canvas.rect().contains(event->position().toPoint())) {
            // Zurück ins Lineal (aus der Zeichenfläche hinaus) gezogen: löschen
            ed.removeGuide(m_guide);
        } else {
            ed.setGuide(m_guide, m_guideCurrent, QObject::tr("Hilfslinie verschieben"));
            m_guidePivot.reset();
        }
        break;
    }
    case Drag::GuideRotate:
        m_canvas.setGuidePreview(std::nullopt);
        if (event->modifiers() & Qt::ControlModifier) // wie beim Objektdrehen: Strg beim Loslassen zählt
            m_guideCurrent = rotatedGuide(m_guideStart, m_guidePivotStart, snapAngle(m_guideCurrent.angle));
        ed.setGuide(m_guide, m_guideCurrent, QObject::tr("Hilfslinie drehen"));
        break;
    case Drag::GuideAnchor:
        m_canvas.update();
        break;
    case Drag::Center:
    case Drag::None:
    case Drag::RightPending:
    case Drag::RightMove:
        break;
    }
    m_drag = Drag::None;
    m_handle = NoHandle;
    m_preview = QTransform();
    m_dragTimer.invalidate();
    m_snapEngine.reset();
    m_snapResult = SnapResult();
    m_canvas.update();
    updateHoverCursor(event->position(), doc);
}

QTransform PickTool::computePreview(const QPointF& doc, Qt::KeyboardModifiers mods) const
{
    // Klassisch: Umschalt = von der Mitte, Strg = Beschränkung;
    // Windows-Standard: Umschalt = Beschränkung, Alt = von der Mitte (Strg = Kopie beim Loslassen)
    const bool windows = Options::instance().integer(QStringLiteral("ws/tools/pick/modifiers")) == 1;
    const bool shift = windows ? bool(mods & Qt::AltModifier) : bool(mods & Qt::ShiftModifier);
    const bool ctrl = windows ? bool(mods & Qt::ShiftModifier) : bool(mods & Qt::ControlModifier);
    QPointF delta = doc - m_pressDoc;
    m_snapResult = SnapResult();

    if (m_drag == Drag::Move || m_drag == Drag::RightMove) {
        if (ctrl) { // nur waagrecht oder senkrecht
            if (std::abs(delta.x()) > std::abs(delta.y()))
                delta.setY(0);
            else
                delta.setX(0);
        } else if (m_snapEngine) {
            delta = m_snapEngine->snapDelta(m_snapSources, delta, &m_snapResult);
        }
        return xform::translation(delta);
    }

    const QRectF& b = m_dragBounds;
    const QPointF grabbed = handleDocPoint(m_handle, b);
    const QPointF anchor = shift ? b.center() : handleDocPoint((m_handle + 4) % HandleCount, b);

    if (m_mode == Mode::Scale) {
        if (m_snapEngine && !ctrl) {
            m_snapResult = m_snapEngine->snap(grabbed + delta);
            if (m_snapResult.snapped())
                delta = m_snapResult.point - grabbed;
        }
        const QPointF v0 = grabbed - anchor;
        const QPointF v1 = grabbed + delta - anchor;
        double sx = 1.0;
        double sy = 1.0;
        if (isCorner(m_handle)) {
            // Ecke: proportional; Faktor = Projektion der Mausbewegung auf die Diagonale
            const double len2 = QPointF::dotProduct(v0, v0);
            double s = len2 > 0.0 ? QPointF::dotProduct(v1, v0) / len2 : 1.0;
            if (ctrl)
                s = snapScale(s);
            sx = sy = s;
        } else if (m_handle == Left || m_handle == Right) {
            sx = v0.x() != 0.0 ? v1.x() / v0.x() : 1.0;
            if (ctrl)
                sx = snapScale(sx);
        } else {
            sy = v0.y() != 0.0 ? v1.y() / v0.y() : 1.0;
            if (ctrl)
                sy = snapScale(sy);
        }
        return xform::scaling(sx, sy, anchor);
    }

    // Dreh-/Neigemodus
    if (isCorner(m_handle)) {
        const QPointF c = rotationCenter();
        const QPointF a = m_pressDoc - c;
        const QPointF n = doc - c;
        double deg = normalizeDegrees(qRadiansToDegrees(std::atan2(n.y(), n.x()) - std::atan2(a.y(), a.x())));
        if (ctrl)
            deg = snapAngle(deg);
        return xform::rotation(deg, c);
    }
    if (m_handle == Top || m_handle == Bottom) {
        const double arm = grabbed.y() - anchor.y();
        if (arm == 0.0)
            return QTransform();
        double deg = qRadiansToDegrees(std::atan(delta.x() / arm));
        if (ctrl)
            deg = snapAngle(deg);
        return xform::skewing(deg, 0.0, anchor);
    }
    const double arm = grabbed.x() - anchor.x();
    if (arm == 0.0)
        return QTransform();
    double deg = qRadiansToDegrees(std::atan(delta.y() / arm));
    if (ctrl)
        deg = snapAngle(deg);
    return xform::skewing(0.0, deg, anchor);
}

void PickTool::commitPreview()
{
    if (m_preview.isIdentity())
        return;
    QString label;
    if (m_drag == Drag::Move)
        label = QObject::tr("Verschieben");
    else if (m_mode == Mode::Scale)
        label = isCorner(m_handle) ? QObject::tr("Skalieren") : QObject::tr("Strecken");
    else
        label = isCorner(m_handle) ? QObject::tr("Drehen") : QObject::tr("Neigen");

    // Drehpunkt wandert beim Verschieben mit; nach Skalieren wieder in die Mitte
    if (m_drag == Drag::Move && m_centerSet)
        m_center = m_preview.map(m_center);
    else if (m_mode == Mode::Scale)
        m_centerSet = false;

    editor().transformSelection(m_preview, label);
}

void PickTool::cancelDrag()
{
    if (m_drag == Drag::Guide || m_drag == Drag::GuideRotate || m_drag == Drag::GuideAnchor)
        m_canvas.setGuidePreview(std::nullopt);
    m_drag = Drag::None;
    m_handle = NoHandle;
    m_preview = QTransform();
    m_snapEngine.reset();
    m_snapResult = SnapResult();
    m_canvas.update();
}

void PickTool::updateHoverCursor(const QPointF& screen, const QPointF& doc)
{
    const Editor& ed = editor();
    if (!ed.selection().isEmpty()) {
        if (m_mode == Mode::Rotate && hitsCenter(screen)) {
            m_canvas.setCursor(Qt::CrossCursor);
            return;
        }
        const int h = handleAt(screen);
        if (h != NoHandle) {
            if (m_mode == Mode::Rotate) {
                if (isCorner(h))
                    m_canvas.setCursor(rotateCursor());
                else
                    m_canvas.setCursor(h == Top || h == Bottom ? Qt::SizeHorCursor : Qt::SizeVerCursor);
            } else if (h == TopLeft || h == BottomRight) {
                m_canvas.setCursor(Qt::SizeFDiagCursor);
            } else if (h == TopRight || h == BottomLeft) {
                m_canvas.setCursor(Qt::SizeBDiagCursor);
            } else {
                m_canvas.setCursor(h == Left || h == Right ? Qt::SizeHorCursor : Qt::SizeVerCursor);
            }
            return;
        }
    }
    const DrawObject* hit = ed.objectAt(doc, m_canvas.pixelsToDoc(4.0));
    if (hit) {
        m_canvas.setCursor(ed.isSelected(hit->id()) ? Qt::SizeAllCursor : Qt::ArrowCursor);
        return;
    }
    if (const int sel = m_canvas.selectedGuide(); sel >= 0 && m_guideRotateMode) {
        const Guide g = ed.guides().value(sel);
        QPointF a;
        QPointF b;
        const QPointF pivot = m_canvas.toScreen(guidePivot(g));
        if (std::hypot(screen.x() - pivot.x(), screen.y() - pivot.y()) <= 7.0) {
            m_canvas.setCursor(Qt::SizeAllCursor);
            return;
        }
        if (m_canvas.guideSegment(g, 16.0, &a, &b)
            && (std::hypot(screen.x() - a.x(), screen.y() - a.y()) <= 9.0
                || std::hypot(screen.x() - b.x(), screen.y() - b.y()) <= 9.0)) {
            m_canvas.setCursor(rotateCursor());
            return;
        }
    }
    if (const int g = m_canvas.guideAt(screen); g >= 0) {
        const Guide guide = ed.guides().at(g);
        m_canvas.setCursor(guide.isHorizontal() ? Qt::SizeVerCursor
                           : guide.isVertical() ? Qt::SizeHorCursor
                                                : Qt::SizeAllCursor);
        return;
    }
    m_canvas.setCursor(Qt::ArrowCursor);
}

bool PickTool::keyPress(QKeyEvent* event)
{
    Editor& ed = editor();
    switch (event->key()) {
    case Qt::Key_Escape:
        if (m_placeInFront || m_pickTarget) {
            m_placeInFront.reset();
            m_pickTarget = nullptr;
            m_canvas.setCursor(Qt::ArrowCursor);
        } else if (m_drag != Drag::None) {
            cancelDrag();
        } else if (ed.editedSuperClip() != 0) {
            ed.endSuperClipEdit(); // SuperClip bearbeiten beenden wie "Fertig"
        } else {
            ed.clearSelection();
            m_canvas.setSelectedGuide(-1);
        }
        return true;
    case Qt::Key_Delete:
        if (m_canvas.selectedGuide() >= 0) {
            ed.removeGuide(m_canvas.selectedGuide());
            m_canvas.setSelectedGuide(-1);
        } else {
            ed.deleteSelection();
        }
        return true;
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down: {
        if (ed.selection().isEmpty())
            return false;
        // Schubsen um den Verschiebungsabstand der Seite: Umschalt zehnfach, Strg ein Zehntel
        double step = ed.nudge();
        if (event->modifiers() & Qt::ShiftModifier)
            step *= std::max(1, Options::instance().integer(QStringLiteral("doc/rulers/super")));
        else if (event->modifiers() & Qt::ControlModifier)
            step /= std::max(1, Options::instance().integer(QStringLiteral("doc/rulers/micro")));
        QPointF d;
        switch (event->key()) {
        case Qt::Key_Left: d = QPointF(-step, 0); break;
        case Qt::Key_Right: d = QPointF(step, 0); break;
        case Qt::Key_Up: d = QPointF(0, -step); break;
        default: d = QPointF(0, step); break;
        }
        if (m_centerSet)
            m_center += d;
        ed.transformSelection(xform::translation(d), QObject::tr("Schubsen"));
        return true;
    }
    default:
        return false;
    }
}

void PickTool::paintOverlay(QPainter& p)
{
    const Editor& ed = editor();
    const QTransform view = m_canvas.viewTransform();
    p.save();

    // Fadenkreuz über die ganze Zeichenfläche (Optionen > Hilfsmittel 'Auswahl')
    if (Options::instance().flag(QStringLiteral("ws/tools/pick/crosshair")) && !std::isnan(m_hover.x())) {
        p.setPen(QPen(QColor(0x60, 0x60, 0x60), 1, Qt::DotLine));
        const QPointF s = m_canvas.toScreen(m_hover);
        p.drawLine(QPointF(0, s.y()), QPointF(m_canvas.width(), s.y()));
        p.drawLine(QPointF(s.x(), 0), QPointF(s.x(), m_canvas.height()));
    }
    if ((m_drag == Drag::Move || m_drag == Drag::Handle || m_drag == Drag::RightMove) && !m_preview.isIdentity()) {
        // Vorschau: Original bleibt stehen, die neue Lage erscheint als blauer Umriss. "Komplexe
        // Objekte neu zeichnen": nach der Verzögerung erscheint das Objekt selbst
        const Options& opt = Options::instance();
        if (opt.flag(QStringLiteral("ws/tools/pick/redrawComplex"))
            && m_dragTimer.isValid() && m_dragTimer.elapsed() >= opt.number(QStringLiteral("ws/tools/pick/delay")) * 1000.0) {
            for (const DrawObject* o : ed.selectedObjects())
                m_canvas.paintObjectPreview(p, *o, m_preview);
        }
        p.setPen(QPen(Canvas::previewColor(), 1.0));
        p.setBrush(Qt::NoBrush);
        const QTransform m = m_preview * view;
        for (const DrawObject* o : ed.selectedObjects())
            p.drawPath(m.map(node_cast<TextObject>(o) ? o->snapPath() : o->path()));
        m_canvas.paintSnapHint(p, m_snapResult);
    } else if (m_drag == Drag::Marquee) {
        QPen pen(QColor(80, 80, 80), 1.0, Qt::DashLine);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(m_canvas.toScreen(m_pressDoc), m_canvas.toScreen(m_currentDoc)).normalized());
    } else if (selectionFullyLocked()) {
        paintLockHandles(p, ed.selectionBounds());
    } else if (!ed.selection().isEmpty()) {
        paintHandles(p, ed.selectionBounds());
        m_canvas.paintTextChain(p, selectedTextChain(), std::nullopt);
    } else if (m_canvas.selectedGuide() >= 0 && m_drag != Drag::GuideRotate) {
        paintGuideHandles(p);
    }
    p.restore();
}

QList<TextFrameGeometry> PickTool::selectedTextChain() const
{
    const QList<const DrawObject*> objects = editor().selectedObjects();
    if (objects.size() != 1)
        return {};
    const auto* text = node_cast<TextObject>(objects.first());
    if (!text || text->kind() != TextObject::Kind::Paragraph || editor().isLocked(text->id()))
        return {};
    return m_canvas.textChainGeometry(text->id());
}

QPointF PickTool::guidePivot(const Guide& g) const
{
    return m_guidePivot.value_or(g.anchor);
}

void PickTool::paintGuideHandles(QPainter& p) const
{
    const Guide g = editor().guides().value(m_canvas.selectedGuide());
    QPointF a;
    QPointF b;
    if (!m_canvas.guideSegment(g, 16.0, &a, &b))
        return;
    p.setPen(QPen(Qt::black, 1.0));
    const QPointF n = g.normal(); // Bildschirm und Dokument haben dieselbe Achsrichtung
    if (m_guideRotateMode) {
        drawRotateGlyph(p, a, -g.direction());
        drawRotateGlyph(p, b, g.direction());
        const QPointF c = m_canvas.toScreen(guidePivot(g));
        p.setBrush(Qt::white);
        p.drawEllipse(c, 5.0, 5.0);
        p.setBrush(Qt::black);
        p.drawEllipse(c, 1.5, 1.5);
    } else {
        // kleine Doppelpfeile quer zur Linie an beiden Enden: Hilfslinie ist ausgewählt
        for (const QPointF& end : {a, b}) {
            p.drawLine(end - n * 6.0, end + n * 6.0);
            p.setBrush(Qt::black);
            drawArrowHead(p, end + n * 6.0, n);
            drawArrowHead(p, end - n * 6.0, -n);
        }
    }
}

void PickTool::paintLockHandles(QPainter& p, const QRectF& bounds) const
{
    // Kleine Schlösser statt Griffe
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    for (int h = 0; h < HandleCount; ++h) {
        const QPointF c = handleScreenPoint(h, bounds);
        p.setPen(QPen(Qt::black, 1.2));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(c.x() - 2.5, c.y() - 5.5, 5.0, 6.0), 0, 180 * 16);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black);
        p.drawRect(QRectF(c.x() - 3.5, c.y() - 2.0, 7.0, 5.0));
    }
    p.restore();
}

void PickTool::paintHandles(QPainter& p, const QRectF& bounds) const
{
    p.setPen(QPen(Qt::black, 1.0));
    if (m_mode == Mode::Scale) {
        p.setBrush(Qt::black);
        for (int h = 0; h < HandleCount; ++h) {
            const QPointF c = handleScreenPoint(h, bounds);
            p.drawRect(QRectF(c.x() - kHandleSize / 2, c.y() - kHandleSize / 2, kHandleSize, kHandleSize));
        }
        const QPointF c = m_canvas.toScreen(bounds.center());
        p.drawLine(c + QPointF(-3, -3), c + QPointF(3, 3));
        p.drawLine(c + QPointF(-3, 3), c + QPointF(3, -3));
        return;
    }

    const QPointF outward[] = {{-1, -1}, {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}};
    for (int h = 0; h < HandleCount; ++h) {
        const QPointF at = handleScreenPoint(h, bounds);
        if (isCorner(h))
            drawRotateGlyph(p, at, outward[h] / std::sqrt(2.0));
        else
            drawSkewGlyph(p, at, h == Top || h == Bottom);
    }
    const QPointF c = m_canvas.toScreen(rotationCenter());
    p.setBrush(Qt::white);
    p.drawEllipse(c, 5.0, 5.0);
    p.setBrush(Qt::black);
    p.drawEllipse(c, 1.5, 1.5);
}

} // namespace vek

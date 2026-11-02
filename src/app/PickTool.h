#pragma once

#include "Canvas.h"
#include "SnapEngine.h"
#include "Tool.h"

#include <QPointF>
#include <QRectF>
#include <QElapsedTimer>
#include <QTransform>
#include <QtNumeric>

#include <functional>
#include <optional>

namespace vek {

class Editor;

// Auswahlwerkzeug mit dem gewohnten Verhalten:
//
//  - Klick wählt aus, Umschalt+Klick ergänzt/entfernt, Ziehen im Leeren zieht einen Rahmen.
//  - Skaliermodus: acht Griffe um das Begrenzungsrechteck. Ecken skalieren proportional,
//    Seiten nur in eine Richtung. Umschalt = vom Mittelpunkt aus, Strg = in 100-%-Schritten.
//  - Erneuter Klick auf die Auswahl schaltet in den Dreh-/Neigemodus: Ecken drehen um den
//    verschiebbaren Drehpunkt (Strg = 15°-Schritte), Seiten neigen.
//  - Während des Ziehens bleibt das Original stehen, die Vorschau erscheint blau. Erst beim
//    Loslassen entsteht ein Undo-Schritt.
//  - Verschieben und Skalieren rasten an Knoten, Mittelpunkten, Rändern, Hilfslinien und
//    Seite ein; der Hinweis erscheint neben der Einrast-Stelle.
//  - Hilfslinien lassen sich anklicken, ziehen und mit Entf löschen; zurück ins Lineal
//    gezogen verschwinden sie. Ein zweiter Klick auf die ausgewählte Hilfslinie zeigt
//    Drehgriffe an ihren Enden und den Ankerpunkt (verschiebbar entlang der Linie).
class PickTool final : public Tool {
public:
    explicit PickTool(Canvas& canvas);

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    // Doppelklick auf Text: mit dem Textwerkzeug an dieser Stelle bearbeiten (wie)
    void mouseDoubleClick(QMouseEvent* event, const QPointF& doc) override;
    bool keyPress(QKeyEvent* event) override;
    void paintOverlay(QPainter& p) override;

    // "Vor..." / "Hinter...": der nächste Klick wählt das Objekt, vor bzw. hinter das die
    // Auswahl kommt; Esc bricht ab
    void beginPlaceRelative(bool inFront);
    bool placingRelative() const { return m_placeInFront.has_value(); }
    // Formen (Verschmelzen mit, Zuschneiden, Schneiden mit): der nächste Klick wählt das Ziel;
    // Esc bricht ab
    void beginPickTarget(std::function<void(NodeId)> done);
    bool pickingTarget() const { return bool(m_pickTarget); }

    // Mit der rechten Maustaste ziehen: beim Loslassen fragt ein Menü, was
    // passieren soll (target = Objekt unter der Maus, 0 = keines; move = Verschiebung)
    enum class DropAction { Move, Copy, CopyFill, CopyOutline, CopyAll, SuperClipInside };
    void rightDrop(DropAction action, NodeId target, const QTransform& move);

private:
    enum class Mode { Scale, Rotate };
    enum class Drag { None, Pending, Move, Handle, Center, Marquee, Guide, GuideRotate, GuideAnchor, RightPending, RightMove };
    // Reihenfolge im Uhrzeigersinn ab oben links
    enum Handle { TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left, HandleCount, NoHandle = -1 };

    Editor& editor() const;
    QPointF handleDocPoint(int handle, const QRectF& bounds) const;
    QPointF handleScreenPoint(int handle, const QRectF& bounds) const;
    int handleAt(const QPointF& screen) const;
    bool hitsCenter(const QPointF& screen) const;
    QPointF rotationCenter() const;
    QTransform computePreview(const QPointF& doc, Qt::KeyboardModifiers mods) const;
    void commitPreview();
    void prepareSnapping();
    void updateHoverCursor(const QPointF& screen, const QPointF& doc);
    void cancelDrag();
    void rightRelease(QMouseEvent* event, const QPointF& doc);
    void paintHandles(QPainter& p, const QRectF& bounds) const;
    void paintGuideHandles(QPainter& p) const;
    void paintLockHandles(QPainter& p, const QRectF& bounds) const;
    // Auswahl besteht nur aus gesperrten Objekten: nicht verschieben, keine Griffe
    bool selectionFullyLocked() const;
    QPointF guidePivot(const Guide& g) const;
    // Genau ein Mengentextrahmen ausgewählt: seine Kette (für Laschen und Verknüpfen), sonst leer
    QList<TextFrameGeometry> selectedTextChain() const;

    Mode m_mode = Mode::Scale;
    Drag m_drag = Drag::None;
    int m_handle = NoHandle;
    std::optional<bool> m_placeInFront;
    std::function<void(NodeId)> m_pickTarget;
    QPointF m_hover{qQNaN(), qQNaN()}; // Mausposition für das Fadenkreuz
    QElapsedTimer m_dragTimer;          // seit Beginn des Ziehens (komplexe Objekte zeichnen)
    QPointF m_pressDoc;
    QPointF m_pressScreen;
    QPointF m_currentDoc;
    QRectF m_dragBounds;      // Begrenzungsrechteck der Auswahl beim Drücken
    bool m_clickOnSelected = false;
    bool m_addToSelection = false;
    QTransform m_preview;
    // Vom Benutzer verschobener Drehpunkt; sonst Mitte der Auswahl
    QPointF m_center;
    bool m_centerSet = false;
    // Einrasten: beim Drücken aufgebaut, da sich die Ziele während des Zugs nicht ändern
    std::optional<SnapEngine> m_snapEngine;
    QList<QPointF> m_snapSources;
    mutable SnapResult m_snapResult;
    // Hilfslinie, die gerade gezogen oder gedreht wird
    int m_guide = -1;
    Guide m_guideStart;
    Guide m_guideCurrent;
    bool m_guideMoved = false;
    bool m_guideClickOnSelected = false;
    bool m_guideRotateMode = false;
    // Drehpunkt der ausgewählten Hilfslinie: frei verschiebbar (auch neben die Linie), sonst der Anker
    std::optional<QPointF> m_guidePivot;
    QPointF m_guidePivotStart;
};

} // namespace vek

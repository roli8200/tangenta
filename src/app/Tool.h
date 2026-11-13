#pragma once

#include <QCursor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include "Color.h"

#include <optional>

namespace vek {

class Canvas;
class Editor;

// Werkzeug der Zeichenfläche. Ereignisse kommen mit Dokumentkoordinaten (mm) an; das
// Overlay zeichnet in Bildschirmkoordinaten. Während eines Zugs ändert ein Werkzeug das
// Dokument nicht, sondern zeigt eine Vorschau und schreibt erst beim Loslassen eine Transaktion.
class Tool {
public:
    explicit Tool(Canvas& canvas) : m_canvas(canvas) {}
    virtual ~Tool() = default;

    virtual void activate() {}
    virtual void deactivate() {}
    virtual void mousePress(QMouseEvent* event, const QPointF& doc) = 0;
    virtual void mouseMove(QMouseEvent* event, const QPointF& doc) = 0;
    virtual void mouseRelease(QMouseEvent* event, const QPointF& doc) = 0;
    virtual void mouseDoubleClick(QMouseEvent*, const QPointF&) {}
    virtual bool keyPress(QKeyEvent*) { return false; }
    // true: alle Tasten gehen ans Werkzeug, auch solche mit Menükürzel (z.B. Entf beim Tippen)
    virtual bool capturesKeyboard() const { return false; }
    // true: diese Taste gehört gerade dem Werkzeug, auch wenn ein Tastaturbefehl auf ihr liegt
    // (z.B. Leertaste beim Ziehen = Kopie stempeln)
    virtual bool claimsKey(const QKeyEvent*) const { return false; }
    // Schubsen (Tastaturbefehle "Schubsen …"); true = vom Werkzeug ausgeführt
    virtual bool nudge(const QPointF& /*delta*/) { return false; }
    // Farbe aus der Palette; true = vom Werkzeug verbraucht
    virtual bool paletteColor(const std::optional<Color>& /*color*/, bool /*fill*/) { return false; }
    // Aus der Palette auf die Zeichenfläche gezogene Farbe (screen = Bildschirmstelle der
    // Zeichenfläche); true = vom Werkzeug verbraucht, sonst füllt sie das Objekt darunter
    virtual bool colorDropped(const Color& /*color*/, const QPointF& /*screen*/) { return false; }
    virtual void paintOverlay(QPainter&) {}

protected:
    Canvas& m_canvas;
};

} // namespace vek

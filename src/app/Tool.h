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
    // Farbe aus der Palette; true = vom Werkzeug verbraucht
    virtual bool paletteColor(const std::optional<Color>& /*color*/, bool /*fill*/) { return false; }
    virtual void paintOverlay(QPainter&) {}

protected:
    Canvas& m_canvas;
};

} // namespace vek

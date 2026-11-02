#pragma once

#include "Style.h"
#include "Tool.h"

#include <memory>
#include <optional>

namespace vek {

class DrawObject;

// Farbpipette: Klick nimmt die Farbe an der Stelle (wie gezeichnet, samt
// Hintergrund und Transparenz), danach ist das Werkzeug im Anwenden-Modus: Klick ins Objekt
// setzt die Füllung, Klick auf den Rand (oder mit Umschalt) die Konturfarbe. Esc bzw. Strg
// nimmt erneut eine Farbe.
class ColorPipetteTool final : public Tool {
public:
    explicit ColorPipetteTool(Canvas& canvas) : Tool(canvas) {}

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent*, const QPointF&) override {}
    void mouseRelease(QMouseEvent*, const QPointF&) override {}
    bool keyPress(QKeyEvent* event) override;
    void paintOverlay(QPainter& p) override;

    std::optional<Color> color() const { return m_color; }
    bool applying() const { return m_color.has_value(); }
    // Aufgenommene Farbe setzen (z.B. vom Desktop); danach färbt ein Klick ein Objekt
    void setColor(const std::optional<Color>& color);
    // "Auswahl vom Desktop": der nächste Klick irgendwo auf dem Bildschirm nimmt die Farbe auf
    void pickFromDesktop();
    // Farbe der gezeichneten Seite an einer Stelle (Seitenkoordinaten)
    Color sample(const QPointF& doc) const;

private:
    void updateCursor();
    std::optional<Color> m_color;
    QPointF m_last;
};

// Attributpipette: Klick auf ein Objekt nimmt dessen Eigenschaften (Füllung, Kontur, Text),
// Transformationen (Grösse, Drehung) und Effekte (Transparenz, Unschärfe); danach überträgt
// jeder Klick sie auf das angeklickte Objekt. Welche Gruppen, steht in der Eigenschaftsleiste.
class AttributePipetteTool final : public Tool {
public:
    explicit AttributePipetteTool(Canvas& canvas);
    ~AttributePipetteTool() override;

    enum What { Properties = 1, Transformations = 2, Effects = 4 };

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent*, const QPointF&) override {}
    void mouseRelease(QMouseEvent*, const QPointF&) override {}
    bool keyPress(QKeyEvent* event) override;

    bool applying() const { return m_source != nullptr; }
    static int what();

private:
    void updateCursor();
    std::unique_ptr<DrawObject> m_source; // Kopie des Spenders
};

} // namespace vek

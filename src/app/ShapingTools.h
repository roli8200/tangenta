#pragma once

#include "Node.h"
#include "Tool.h"

#include <QElapsedTimer>
#include <QList>
#include <QPolygonF>
#include <QSet>
#include <QTimer>
#include <QTransform>

namespace vek {

// Formgebungswerkzeuge (Ausklappmenü beim Formwerkzeug): eine runde Spitze
// verformt die ausgewählten Objekte, solange man zieht bzw. die Maustaste hält. Grundformen und
// Text werden dabei zu Kurven. Radius und Stärke stehen in der Eigenschaftsleiste.
//  - Verschmieren: Umriss folgt der Mausbewegung (weich auslaufend)
//  - Verwischen:   wie Verschmieren, aber mit harter Spitze (Ausbuchtungen ziehen)
//  - Aufrauen:     zackige Kante unter der Spitze
//  - Wirbel:       dreht den Umriss unter der Spitze, solange man hält
//  - Anziehen / Abstossen: zieht den Umriss zur Mitte der Spitze bzw. drückt ihn weg
class BrushShapingTool final : public Tool {
public:
    enum class Mode { Smear, Smudge, Roughen, Twirl, Attract, Repel };
    BrushShapingTool(Canvas& canvas, Mode mode);

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    void paintOverlay(QPainter& p) override;

    Mode mode() const { return m_mode; }
    // Radius in mm und Stärke in % (Optionen ws/tools/shaping/radius, ws/tools/shaping/strength)
    static double radius();
    static double strength();
    // Ein Schritt der Verformung an doc (für Tests ohne Zeitgeber): Zug von last nach doc
    void step(const QPointF& last, const QPointF& doc);
    // Ergebnis übernehmen (ein Rückgängig-Schritt)
    void commit();

private:
    struct Work {
        NodeId id = 0;
        QList<QPolygonF> polys; // Seitenkoordinaten, fein abgetastet; geschlossen: erster == letzter Punkt
        QList<bool> closed;
        QSet<qint64> roughened; // Aufrauen: schon bearbeitete Punkte (Linienzug << 32 | Punkt)
    };
    bool begin();
    void tick();

    Mode m_mode;
    QList<Work> m_work;
    bool m_pressed = false;
    QPointF m_last;
    QPointF m_mouse;
    bool m_hasMouse = false;
    QTimer m_timer; // Wirbel, Anziehen, Abstossen wirken auch beim Halten
};

// Freie Transformation: Klick setzt den Drehpunkt, Ziehen dreht, spiegelt, skaliert oder neigt
// die Auswahl um diesen Punkt (Modus in der Eigenschaftsleiste)
class FreeTransformTool final : public Tool {
public:
    enum class Mode { Rotate, Mirror, Scale, Skew };
    using Tool::Tool;

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    void paintOverlay(QPainter& p) override;

    static Mode mode();
    // Abbildung für einen Zug von pivot nach to (Seitenkoordinaten); bounds = Auswahl
    static QTransform transformFor(Mode mode, const QPointF& pivot, const QPointF& to, const QRectF& bounds);

private:
    bool m_pressed = false;
    QPointF m_pivot;
    QPointF m_current;
};

} // namespace vek

#pragma once

#include "Page.h"

#include <QWidget>

namespace vek {

class Canvas;

// Lineal in mm am oberen bzw. linken Rand der Zeichenfläche. Folgt Zoom und Bildlauf,
// zeigt die Mausposition an. Aus dem Lineal gezogen entsteht eine Hilfslinie: aus dem
// oberen eine waagrechte, aus dem linken eine senkrechte.
class Ruler : public QWidget {
    Q_OBJECT

public:
    Ruler(Qt::Orientation orientation, Canvas& canvas, QWidget* parent = nullptr);
    QSize sizeHint() const override;

    static constexpr int Thickness = 22;

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

private:
    // Lage im Dokument (mm) für eine Position auf dem Lineal (px)
    double toDoc(double px) const;
    double toPx(double mm) const;
    bool guidePosition(const QPoint& globalPos, double* position) const;
    Guide makeGuide(double position) const;
    // Tabulatoren beim Bearbeiten von Mengentext (nur waagrechtes Lineal)
    bool textFrame(double* left, double* right, QList<double>* tabs) const;
    int tabAt(double px, const QList<double>& tabs, double frameLeft) const;

    Qt::Orientation m_orientation;
    Canvas& m_canvas;
    double m_cursor = qQNaN(); // mm
    bool m_draggingGuide = false;
    int m_draggedTab = -1; // Index in m_tabs, solange ein Tabulator gezogen wird
    QList<double> m_tabs;
};

// Ecke zwischen den Linealen: Doppelklick = Lineal-Optionen, auf die Zeichenfläche ziehen =
// Nullpunkt der Lineale setzen
class RulerCorner : public QWidget {
    Q_OBJECT

public:
    explicit RulerCorner(QWidget* parent = nullptr, Canvas* canvas = nullptr) : QWidget(parent), m_canvas(canvas) {}
    QSize sizeHint() const override { return QSize(Ruler::Thickness, Ruler::Thickness); }

signals:
    void optionsRequested();

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

private:
    Canvas* m_canvas = nullptr;
    bool m_dragging = false;
};

} // namespace vek

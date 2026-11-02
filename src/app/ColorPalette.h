#pragma once

#include "Color.h"

#include <QWidget>

#include <optional>
#include <vector>

namespace vek {

// Farbleiste am rechten Rand: eine Spalte kleiner Farbfelder, mit den
// Pfeilen oder dem Mausrad rollbar. Der Knopf oben klappt sie auf; dann liegen alle
// Farben spaltenweise (von oben nach unten) nebeneinander.
// Linksklick = Füllung, Rechtsklick = Kontur; das Feld mit dem Kreuz steht für "keine".
class ColorPalette : public QWidget {
    Q_OBJECT

public:
    explicit ColorPalette(QWidget* parent = nullptr);
    ~ColorPalette() override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    static constexpr int Cell = 16;
    int colorCount() const { return static_cast<int>(m_colors.size()); }
    const std::optional<Color>& colorAt(int index) const { return m_colors[static_cast<std::size_t>(index)]; }
    QString tooltipFor(int index) const;
    void choose(int index, Qt::MouseButton button);
    // Aufgeklappte Ansicht öffnen (auch für Tests)
    void expand();
    // "Zur Palette hinzufügen": eigene Farbe hinten anhängen (gespeichert, gilt für alle Leisten)
    static void addCustomColor(const Color& color);
    static QList<Color> customColors();

signals:
    void fillChosen(std::optional<vek::Color> color);
    void strokeChosen(std::optional<vek::Color> color);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void leaveEvent(QEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    bool event(QEvent* e) override;

private:
    QRect expandButton() const;
    QRect upArrow() const;
    QRect downArrow() const;
    QRect swatchArea() const;
    int visibleCount() const;
    bool overflows() const;
    int swatchAt(const QPoint& pos) const;
    void scrollBy(int cells);

    void reload();
    std::vector<std::optional<Color>> m_colors;
    int m_first = 0;
    int m_hover = -1;
};

// Zeichnet ein Farbfeld wie in der Leiste (für Leiste und aufgeklappte Ansicht)
void paintSwatch(QPainter& p, const QRect& r, const std::optional<Color>& c, bool hover);

} // namespace vek

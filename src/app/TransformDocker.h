#pragma once

#include <QTransform>
#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;
class QStackedWidget;
class QToolButton;

namespace vek {

class Editor;

// Andockfenster "Änderungen" (Anordnen > Änderungen, Alt+F7…F10):
// Position, Drehen, Skalieren und spiegeln, Grösse, Neigung. Ein 3×3-Raster wählt den
// Bezugspunkt am Begrenzungsrahmen der Auswahl, "Kopien" legt so viele verschobene Kopien
// an (jede relativ zur vorigen). "Zuweisen" wirkt auf die aktuelle Auswahl.
class TransformDocker final : public QWidget {
    Q_OBJECT

public:
    enum class Page { Position, Rotate, Scale, Size, Skew };

    explicit TransformDocker(Editor& editor, QWidget* parent = nullptr);

    void showPage(Page page);
    Page page() const;
    // Für Tests und die Eigenschaftsleiste: Werte setzen und zuweisen
    void setValues(double a, double b);
    void setAnchor(int index); // 0..8 zeilenweise, 4 = Mitte
    void setCopies(int copies);
    void setRelative(bool relative);
    void setMirror(bool horizontal, bool vertical);
    void apply();
    // Die Abbildung, die "Zuweisen" mit den aktuellen Werten auf rect ausführen würde
    QTransform transformFor(const QRectF& rect) const;

private:
    void refresh();
    QPointF anchorPoint(const QRectF& r) const;

    Editor& m_editor;
    QButtonGroup* m_pages;
    QDoubleSpinBox* m_a;
    QDoubleSpinBox* m_b;
    QCheckBox* m_relative;
    QCheckBox* m_proportional;
    QToolButton* m_mirrorH;
    QToolButton* m_mirrorV;
    QButtonGroup* m_anchor;
    QSpinBox* m_copies;
    QLabel* m_title;
    QLabel* m_labelA;
    QLabel* m_labelB;
};

} // namespace vek

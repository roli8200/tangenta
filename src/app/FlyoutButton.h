#pragma once

#include <QList>
#include <QToolButton>

class QAction;

namespace vek {

// Knopf der Hilfsmittelleiste mit Ausklappliste: ein kleines schwarzes
// Dreieck unten rechts zeigt die Liste an. Die Liste führt Symbol, Name und Kurzbefehl.
//  - Werkzeuggruppe: der Knopf zeigt das zuletzt gewählte Werkzeug; Klick wählt es,
//    Klick aufs Dreieck oder langes Drücken öffnet die Liste
//  - Befehlsliste (Kontur, Füllung): jeder Klick öffnet die Liste
class FlyoutButton final : public QToolButton {
    Q_OBJECT

public:
    enum class Kind { ToolGroup, Commands };
    FlyoutButton(Kind kind, const QList<QAction*>& actions, QWidget* parent);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    Kind m_kind;
};

} // namespace vek

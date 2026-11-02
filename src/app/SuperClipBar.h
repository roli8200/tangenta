#pragma once

#include <QFrame>

class QToolButton;

namespace vek {

class Canvas;

// Schwebende Leiste unter einem ausgewählten SuperClip: SuperClip bearbeiten,
// Inhalt auswählen, Inhalt extrahieren, Inhalt sperren, Inhalt einpassen. Während der
// Bearbeitung zeigt sie nur "Fertig".
class SuperClipBar final : public QFrame {
    Q_OBJECT

public:
    explicit SuperClipBar(Canvas& canvas);
    // Knöpfe an den Zustand anpassen (Bearbeitung läuft, Inhalt gesperrt)
    void sync(bool editing, bool locked);

private:
    Canvas& m_canvas;
    QToolButton* m_edit = nullptr;
    QToolButton* m_select = nullptr;
    QToolButton* m_extract = nullptr;
    QToolButton* m_lock = nullptr;
    QToolButton* m_fit = nullptr;
    QToolButton* m_finish = nullptr;
};

} // namespace vek

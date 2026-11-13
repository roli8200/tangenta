#pragma once

#include "MediaLibrary.h"

#include <QIcon>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace vek {

class Canvas;
class Editor;

// Vorschaubild eines Strichs (Wellenlinie mit dem Strich), für Docker und Eigenschaftsleiste
QIcon mediaStrokeIcon(const ArtisticMedia& media, const QSize& size, qreal dpr = 1.0);

// Andockfenster "Künstlerische Medien" (Fenster > Andockfenster, Effekte > Künstlerische
// Medien): Strichbibliothek mit Vorschaubildern – zuletzt benutzte, Voreinstellungen, Pinsel,
// Sprühlisten und eigene Striche. Ein Klick wendet den Strich auf die ausgewählten Objekte an
// und stellt ihn im Werkzeug ein; "Speichern" legt die gewählten Objekte als eigenen Pinsel oder
// als Sprühliste in der Bibliothek ab.
class MediaDocker final : public QWidget {
    Q_OBJECT

public:
    enum class Category { Recent, Presets, Brushes, Sprayers, User };
    enum class SaveAs { Brush, Sprayer };

    MediaDocker(Editor& editor, Canvas& canvas, QWidget* parent = nullptr);

    void setCategory(Category c);
    Category category() const;
    const QList<MediaStroke>& entries() const { return m_entries; }
    // Eintrag anwenden: im Werkzeug einstellen, auf die Auswahl anwenden, als zuletzt benutzt merken
    bool apply(int index);
    // Gewählte Objekte als eigenen Strich speichern; liefert dessen Namen (leer: nichts gewählt)
    QString saveSelection(SaveAs as, const QString& name = {});
    QListWidget* list() const { return m_list; }

private:
    void reload();
    void updateButtons();

    Editor& m_editor;
    Canvas& m_canvas;
    QComboBox* m_category = nullptr;
    QListWidget* m_list = nullptr;
    QPushButton* m_apply = nullptr;
    QPushButton* m_delete = nullptr;
    QLineEdit* m_saveName = nullptr;
    QComboBox* m_saveType = nullptr;
    QPushButton* m_save = nullptr;
    QLabel* m_status = nullptr;
    QList<MediaStroke> m_entries;
};

} // namespace vek

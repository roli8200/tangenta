#pragma once

#include "Node.h"

#include <QHash>
#include <QImage>
#include <QListWidget>
#include <QWidget>

class QLineEdit;
class QMenu;
class QSlider;
class QTimer;
class QToolButton;

namespace vek {

class Canvas;
class Editor;

// Seitenbefehle mit Dialog, gemeinsam für Layout-Menü, Seitenregister, Mehrseitenansicht und
// das Andockfenster "Seiten"
namespace pagecommands {

void rename(QWidget* parent, Editor& editor, int index);
void insertPages(QWidget* parent, Editor& editor, int index);
void duplicate(QWidget* parent, Editor& editor, int index);
void move(QWidget* parent, Editor& editor, int index);
void toggleOrientation(Editor& editor, int index);
// Kontextmenü einer Seite (Register, Seitenname in der Mehrseitenansicht, Andockfenster)
void fillMenu(QMenu& menu, QWidget* parent, Editor& editor, int index);

} // namespace pagecommands

// Liste der Seitenminiaturen. Ziehen mit der linken Maustaste verschiebt eine Seite
// (eigene Mausbehandlung statt QDrag, damit es auch ohne Fenstersystem prüfbar ist).
class PageThumbnailList final : public QListWidget {
    Q_OBJECT

public:
    explicit PageThumbnailList(QWidget* parent = nullptr);
    // Einfügestelle (Seitenindex vor dem Verschieben, 0..Seitenzahl) unter einem Punkt im Viewport
    int insertionAt(const QPoint& pos) const;

signals:
    void pageMoveRequested(int from, int to);

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void paintEvent(QPaintEvent* e) override;

private:
    int m_pressPage = -1;
    QPoint m_pressPos;
    bool m_dragging = false;
    int m_insertion = -1;
};

// Andockfenster "Seiten": Miniaturen aller Seiten (Liste oder Raster), Suchfeld,
// Seiten hinzufügen/löschen, Ziehen zum Umordnen, Doppelklick zeigt die Seite.
class PagesDocker final : public QWidget {
    Q_OBJECT

public:
    PagesDocker(Editor& editor, Canvas& canvas, QWidget* parent = nullptr);

    PageThumbnailList* list() const { return m_list; }
    QLineEdit* searchField() const { return m_search; }
    QSlider* sizeSlider() const { return m_size; }
    // Rasterzelle für Miniaturen der Grösse icon samt Name
    QSize gridCell(int icon) const;
    bool gridView() const;
    void setGridView(bool grid);
    // Seitenindex eines Eintrags
    static int pageOf(const QListWidgetItem* item);
    QListWidgetItem* itemForPage(int index) const;
    // Miniaturen sofort neu zeichnen (sonst kurz nach einer Änderung, wenn sichtbar)
    void updateThumbnails();
    int thumbnailCount() const;

protected:
    void showEvent(QShowEvent* e) override;

private:
    void rebuild();
    void applyFilter();
    void scheduleThumbnails();
    QImage renderThumbnail(int index, int extent) const;

    Editor& m_editor;
    Canvas& m_canvas;
    QLineEdit* m_search;
    PageThumbnailList* m_list;
    QSlider* m_size;
    QToolButton* m_listButton;
    QToolButton* m_gridButton;
    QToolButton* m_multiButton;
    QTimer* m_timer;
    // Zwischengespeicherte Miniaturen je Seite; neu gezeichnet nur, wenn sich die Kennung des Inhalts ändert
    struct Thumbnail {
        QImage image;
        QByteArray signature;
    };
    QHash<NodeId, Thumbnail> m_thumbnails;
    bool m_dirty = true;
};

} // namespace vek

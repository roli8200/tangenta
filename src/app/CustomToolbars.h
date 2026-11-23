#pragma once

#include <QObject>
#include <QPointer>
#include <QToolBar>

class QAction;
class QMainWindow;
class QMimeData;

namespace vek {

class CustomToolbars;

// Eigene Symbolleiste: Befehle aus den Menüs als Schaltflächen. Befehle kommen per Drag & Drop
// aus Optionen > Anpassung > Befehle; in der Leiste lassen sie sich verschieben (während die
// Optionen offen sind oder mit gedrückter Alt-Taste), in eine andere eigene Leiste ziehen oder
// durch Herausziehen entfernen. Die Leiste selbst ist frei verschiebbar und kann schweben.
class CustomToolBar : public QToolBar {
    Q_OBJECT

public:
    CustomToolBar(CustomToolbars& owner, const QString& id, const QString& name, QWidget* parent);

    QString id() const { return m_id; }
    // Befehl bzw. Trennlinie an Position index (-1 = ans Ende)
    void insertCommand(QAction* command, int index = -1);
    void insertSeparatorAt(int index);
    void removeAt(int index);
    void moveItem(int from, int to);
    int itemCount() const { return actions().size(); }
    // Position, an der ein Ablegen bei pos (Leistenkoordinaten) einfügt
    int dropIndex(const QPoint& pos) const;
    // Schaltfläche an Position index (Tests, Video)
    QWidget* itemWidget(int index) const;

protected:
    void actionEvent(QActionEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    bool editing() const;
    void startDrag(int index);
    void updateMinimumSize();
    CustomToolbars& m_owner;
    QString m_id;
    int m_dropIndex = -1;
    QPoint m_pressPos;
    QPointer<QWidget> m_pressed;
};

// Verwaltet die eigenen Symbolleisten eines Hauptfensters: anlegen, umbenennen, löschen,
// speichern (QSettings "toolbars/custom") und laden. Befehle werden über ihren Menüpfad
// gespeichert (z.B. "Datei/Speichern") und zusätzlich über ihre Position in den Menüs, damit
// sie auch nach einem Sprachwechsel wiedergefunden werden.
class CustomToolbars : public QObject {
    Q_OBJECT

public:
    static constexpr const char* kMime = "application/x-tangenta-command";

    explicit CustomToolbars(QMainWindow* window);

    QMainWindow* window() const { return m_window; }
    QList<CustomToolBar*> bars() const { return m_bars; }
    CustomToolBar* bar(const QString& id) const;
    // Neue, leere Leiste in einer eigenen Zeile oben
    CustomToolBar* create(const QString& name);
    void rename(CustomToolBar* bar, const QString& name);
    void remove(CustomToolBar* bar);

    void load();
    void save() const;
    // Vor dem Abbau des Hauptfensters: Leisten weg, ohne zu speichern (Menüs werden gerade gelöscht)
    void shutdown();

    // Anpassen: Optionen sind offen, Schaltflächen lassen sich ziehen statt auslösen
    void setCustomizing(bool on);
    bool customizing() const { return m_customizing; }

    // Alle Befehle (Menüleiste und Befehlsgruppen) mit Pfad
    struct Command {
        QString path;
        QString index;
        QAction* action = nullptr;
    };
    QList<Command> commands() const;
    QAction* resolve(const QString& path, const QString& index) const;
    Command commandFor(QAction* action) const;
    // Namen der eigenen Leisten, die den Befehl enthalten
    QStringList barsContaining(QAction* action) const;

    // Ziehdaten für einen Befehl (aus der Befehlsliste oder einer Leiste)
    static QMimeData* mimeFor(QAction* action, const QString& sourceBar = {}, int sourceIndex = -1);
    static QAction* actionFrom(const QMimeData* mime, QString* sourceBar = nullptr, int* sourceIndex = nullptr);

signals:
    void changed();

private:
    QMainWindow* m_window;
    QList<CustomToolBar*> m_bars;
    bool m_customizing = false;
    bool m_loading = false;
};

} // namespace vek

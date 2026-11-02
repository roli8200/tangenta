#pragma once

#include "Page.h"

#include <QDialog>
#include <QHash>
#include <QVariantMap>

#include <functional>

class QBoxLayout;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QMainWindow;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace vek {

namespace ai {
struct Account;
}

class Editor;

// Dialog "Optionen": links der Baum (Arbeitsbereich, Dokument, Global),
// rechts die Seite mit blauem Titel, unten OK / Abbrechen / Hilfe. Änderungen gelten erst mit
// OK (Dokument-Werte als ein Undo-Schritt). Einstellungen, deren Funktion TANGENTA noch nicht
// hat, sind ausgegraut ("folgt").
class OptionsDialog : public QDialog {
    Q_OBJECT

public:
    OptionsDialog(Editor& editor, QMainWindow* window, QWidget* parent = nullptr);

    // Seite nach Pfad im Baum, z.B. "Dokument/Gitter"
    bool selectPage(const QString& path);
    QStringList pagePaths() const;
    // Feld einer Einstellung (Tests), nullptr wenn es keins gibt
    QWidget* field(const QString& key) const { return m_fields.value(key); }
    // Wert vormerken wie bei einer Eingabe im Feld
    void setPending(const QString& key, const QVariant& value);

    void accept() override;

private:
    // --- Bausteine: fügen in die aktuelle Seite bzw. Gruppe ein
    QWidget* page(const QString& path, const QString& title);
    void group(const QString& title);
    void endGroup();
    QCheckBox* check(const QString& key, const QString& text);
    QDoubleSpinBox* number(const QString& key, const QString& label, double min, double max, int decimals,
                           const QString& suffix = QString());
    QComboBox* choice(const QString& key, const QString& label, const QStringList& items);
    void radios(const QString& key, const QString& label, const QStringList& items, bool vertical = true);
    void colorField(const QString& key, const QString& label);
    // Gibt die Zeile zurück, damit eine Seite noch etwas anhängen kann (z.B. einen Test-Knopf)
    QBoxLayout* textField(const QString& key, const QString& label);
    // KI-Zugang bearbeiten (Optionen > Global > KI); false bei Abbrechen
    bool editAiAccount(ai::Account& account);
    void slider(const QString& key, const QString& label, int min, int max, const QString& unit);
    // Liste mit Häkchen; Wert = eingeschaltete Kennungen durch Komma getrennt (inverted: ausgeschaltete)
    QListWidget* checkList(const QString& key, const QList<std::pair<QString, QString>>& items, bool inverted = false);
    void note(const QString& text);
    QWidget* button(const QString& text, std::function<void()> onClick);
    // Ausgegraut mit Hinweis
    void later(QWidget* w);
    void laterRest(); // alles, was seit dem letzten Merker hinzukam
    void markLaterStart();
    QBoxLayout* row();

    void buildWorkspace();
    void buildText();
    void buildTools();
    void buildCustomization();
    void buildDocument();
    void buildGuidePages();
    void buildGlobal();
    QWidget* buildCommandsPage();

    QVariant current(const QString& key) const;

    Editor& m_editor;
    QMainWindow* m_window;
    QTreeWidget* m_tree = nullptr;
    QStackedWidget* m_stack = nullptr;
    QHash<QString, QTreeWidgetItem*> m_items;
    QBoxLayout* m_layout = nullptr;      // aktuelle Seite bzw. Gruppe
    QBoxLayout* m_pageLayout = nullptr;  // aktuelle Seite
    QWidget* m_pageWidget = nullptr;
    QList<QWidget*> m_laterMark;
    QVariantMap m_pending;
    QHash<QString, QWidget*> m_fields;
    QList<std::function<void()>> m_appliers; // Seiten mit eigenem Zustand (Hilfslinien, Seitengrösse, …)

    // Hilfslinien als Arbeitskopie: (Hilfslinie, nur aktuelle Seite)
    QList<std::pair<Guide, bool>> m_guides;
    bool m_guidesChanged = false;
    QList<std::function<void()>> m_guideRefreshers;
};

} // namespace vek

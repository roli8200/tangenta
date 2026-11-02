#pragma once

#include "Node.h"

#include <QDialog>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace vek {

class Editor;

// Text > Rechtschreibprüfung (Strg+F12): geht alle Texte des Dokuments durch
// und bietet je unbekanntem Wort Vorschläge, Ersetzen, Ignorieren und Hinzufügen an. Jede
// Ersetzung ist ein eigener Rückgängig-Schritt.
class SpellDialog final : public QDialog {
    Q_OBJECT

public:
    explicit SpellDialog(Editor& editor, QWidget* parent = nullptr);

    // Für Tests: aktuelles Wort und Aktionen ohne Knöpfe
    QString currentWord() const { return m_word; }
    bool findNext();
    void replace(const QString& with);
    void ignoreOnce();
    void ignoreAll();
    void addWord();

private:
    QList<NodeId> texts() const;
    QString textOf(NodeId id) const;
    void showCurrent();

    Editor& m_editor;
    QList<NodeId> m_ids;
    int m_index = 0;   // Text in m_ids
    int m_pos = 0;     // Suchbeginn im Text
    int m_start = -1;  // gefundenes Wort
    QString m_word;

    QLabel* m_context = nullptr;
    QLineEdit* m_replaceWith = nullptr;
    QListWidget* m_suggestions = nullptr;
    QPushButton* m_replaceButton = nullptr;
    QLabel* m_status = nullptr;
};

} // namespace vek

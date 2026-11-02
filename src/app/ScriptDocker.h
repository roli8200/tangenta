#pragma once

#include <QWidget>

class QPlainTextEdit;
class QComboBox;

namespace vek {

class Editor;
class ScriptHost;

// Andockfenster "Skripte": JavaScript schreiben und ausführen (Strg+Eingabe), Ausgabe von
// console.log/tg.print darunter. Skripte liegen als .js im Skriptordner (Optionen > Skripte);
// die Auswahlliste lädt sie, Speichern legt sie dort ab.
class ScriptDocker final : public QWidget {
    Q_OBJECT

public:
    ScriptDocker(Editor& editor, QWidget* parent = nullptr);

    QPlainTextEdit* codeEdit() const { return m_code; }
    QString outputText() const;
    void runCode();
    // Skript aus einer Datei ausführen (Extras > Skripte)
    bool runFile(const QString& path);
    static QString scriptFolder();
    static QStringList scripts();

private:
    void refreshList();
    void showResult(bool ok, const QString& error, const QStringList& output);

    Editor& m_editor;
    ScriptHost* m_host;
    QPlainTextEdit* m_code;
    QPlainTextEdit* m_output;
    QComboBox* m_list;
};

} // namespace vek

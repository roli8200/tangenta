#pragma once

#include <QElapsedTimer>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QMainWindow;
class QPlainTextEdit;
class QPushButton;
class QTextBrowser;
class QTimer;

namespace vek {

class AiChat;
class Canvas;
class Editor;

// Andockfenster "KI": Gespräch mit der KI über das offene Dokument (wie das KI-Fenster in
// ThurWiki). Oben Zugang und neues Gespräch, in der Mitte der Verlauf (Antworten laufend,
// Werkzeugschritte mit ✓/⚠), unten Eingabe (Enter sendet, Umschalt+Enter = neue Zeile),
// "darf ändern" und Senden/Stopp. Das letzte Gespräch bleibt erhalten.
class AiPanel : public QWidget {
    Q_OBJECT

public:
    AiPanel(Editor& editor, Canvas& canvas, QMainWindow* window, QWidget* parent = nullptr);
    ~AiPanel() override;

    AiChat& chat() { return *m_chat; }
    // Zugänge neu einlesen (nach Optionen)
    void reloadAccounts();
    // Für Tests und Tastenkürzel
    void send(const QString& question);
    QString transcriptText() const;

signals:
    void optionsRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void render();
    void scheduleRender();
    void updateControls();
    void saveConversation() const;
    void loadConversation();
    static QString conversationFile();

    AiChat* m_chat = nullptr;
    QComboBox* m_account = nullptr;
    QTextBrowser* m_view = nullptr;
    QLabel* m_status = nullptr;
    QPlainTextEdit* m_input = nullptr;
    QCheckBox* m_mayWrite = nullptr;
    QPushButton* m_send = nullptr;
    QTimer* m_renderTimer = nullptr;
    QTimer* m_clockTimer = nullptr;
    QString m_live;       // laufende Antwort
    QString m_liveStatus; // z.B. "Zeichnen (Skript) …"
    QElapsedTimer m_clock;
};

} // namespace vek

#pragma once

#include "ImageSearch.h"

#include <QDialog>
#include <QHash>
#include <QThreadPool>

class QCheckBox;
class QDialog;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace vek {

// Datei > Bildersuche: in Bildarchiven und Symbolsammlungen suchen und einfügen (wie die
// Bildersuche in ThurWiki). Jede Kachel zeigt Quelle, Urheber und Lizenz; lizenzpflichtige
// Vorschauen (Wasserzeichen) sind sichtbar, aber nicht einfügbar. Nach OK liegen das
// heruntergeladene Bild bzw. SVG in data() und der Treffer in hit().
class ImageSearchDialog : public QDialog {
    Q_OBJECT

public:
    explicit ImageSearchDialog(QWidget* parent = nullptr);
    ~ImageSearchDialog() override;

    const imagesearch::Hit& hit() const { return m_hit; }
    const QByteArray& data() const { return m_data; }

    // Für Tests und die KI: Suchbegriff setzen, Ergebnisse direkt zeigen
    void setTerm(const QString& term);
    void showResults(const QList<imagesearch::Hit>& hits, const QStringList& complaints, bool append = false);
    QStringList checkedSources() const;
    int visibleCount() const;
    // Treffer an Position (in der Liste) einfügbar?
    bool canInsert(int row) const;
    // Auswahl der Auflösung des gewählten Treffers (0 = grösste)
    QComboBox* sizeBox() const { return m_size; }
    // Anzeige auf diese Archive eingrenzen (wie die Kästchen über den Kacheln)
    void showSources(const QStringList& shown);
    void search(bool more = false);
    ImageSearch* engine() const { return m_search; }
    // Betrachter für den Treffer in dieser Listenzeile öffnen (Doppelklick, Leertaste)
    QDialog* showViewer(int row);
    QDialog* viewer() const { return m_viewer; }
    // Globus auf der Kachel und "Im Browser": wie Links geöffnet werden (Tests ersetzen das)
    void setUrlOpener(std::function<void(const QUrl&)> open);
    // Wo der Globus auf einer Kachel zuletzt gezeichnet wurde (Viewport-Koordinaten)
    QRect globeRect(int row) const;

signals:
    void optionsRequested();

private:
    void updateSources();
    void updateDetails();
    void filter();
    void insertCurrent();
    void loadPreview(int index, bool fallback = false);
    void setPreview(int index, const QImage& tile);
    void showNoHits();
    void updateViewBoxes();
    void updateViewAll();

    ImageSearch* m_search = nullptr;
    QLineEdit* m_term = nullptr;
    QComboBox* m_kind = nullptr;
    QCheckBox* m_onlyFree = nullptr;
    QHash<QString, QCheckBox*> m_sourceBoxes;
    QLabel* m_complaints = nullptr;
    QWidget* m_viewRow = nullptr;
    QCheckBox* m_viewAll = nullptr;
    QHash<QString, QCheckBox*> m_viewBoxes; // Anzeige je Archiv
    QListWidget* m_list = nullptr;
    QLabel* m_details = nullptr;
    QPushButton* m_more = nullptr;
    QPushButton* m_insert = nullptr;
    class TileDelegate* m_delegate = nullptr;
    QDialog* m_viewer = nullptr;
    int m_viewerRow = -1;
    int m_viewerToken = 0;
    std::function<void(const QUrl&)> m_openUrl;
    QLabel* m_sizeLabel = nullptr;
    QComboBox* m_size = nullptr;
    QList<imagesearch::Hit> m_hits;
    QString m_lastTerm;
    int m_page = 1;
    bool m_fresh = false; // nächste Antwort ersetzt die Liste
    bool m_busy = false;  // Suche läuft
    int m_generation = 0;   // je neue Trefferliste
    QThreadPool* m_decoder = nullptr; // verkleinert Vorschaubilder im Hintergrund
    imagesearch::Hit m_hit;
    QByteArray m_data;
};

} // namespace vek

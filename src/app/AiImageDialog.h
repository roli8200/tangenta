#pragma once

#include "Ai.h"
#include "ImageAdjust.h"
#include "ImageDialogs.h"

#include <QPointer>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QNetworkAccessManager;
class QNetworkReply;
class QFormLayout;
class QSlider;

namespace vek {

// Bild mit KI optimieren, zwei Verfahren:
// - KI bearbeitet das Bild (Vorgabe): Ein Bildmodell (OpenAI gpt-image, Google Gemini) bearbeitet
//   eine Kopie nach Aufgaben, Stil und Wunsch. Übernommen werden entweder nur Farbe und Licht
//   (colortransfer: das Original bleibt in voller Auflösung, Farbtiefe und Schärfe) oder das
//   KI-Bild selbst (für Schärfen, Entrauschen, Restaurieren, Kolorieren; Details können sich ändern).
//   Jede Anfrage gibt eine weitere Variante; Stärke mischt zwischen Original und KI.
// - KI schlägt Reglerwerte vor: Die KI sieht eine verkleinerte Kopie samt Tonwertstatistik und
//   schlägt Werte für den Bildanpassungseditor vor. TANGENTA rechnet selbst auf dem Original.
// Die Regler dienen in beiden Verfahren der Feinabstimmung.
//
// Freistellen: lokal mit einem Segmentierungsmodell (matting, Standard: pixelgenau auf dem
// Original, offline) oder über OpenAI (gpt-image-1.5, Hintergrund transparent; die KI zeichnet das
// Motiv dabei neu, deshalb nur als ausdrückliche Wahl).
namespace aiopt {

enum class Goal { Natural, Vivid, Warm, Cool, Bright, Moody, Print };

struct Settings {
    Goal goal = Goal::Natural;
    bool color = true;      // Farbstich (Temperatur, Tönung)
    bool tone = true;       // Belichtung (Helligkeit, Kontrast, Glanzlichter, Schatten, Mitteltöne)
    bool saturation = true; // Sättigung
    QString wish;           // freier Wunsch, z.B. "Himmel etwas blauer"
};

struct Suggestion {
    bool ok = false;
    adjust::Basic basic;
    bool stretch = false; // vorher Tonwerte strecken (wie Automatische Anpassung)
    QString comment;
    QString error;
};

// Anweisung an die KI (Antwort als JSON) und die Frage zum Bild
QString systemPrompt();
QString question(const Settings& settings, const QImage& image, const QString& language);
// Kurze Tonwertstatistik (Perzentile je Kanal, Sättigung), damit die KI nicht nur schätzt
QString statistics(const QImage& image);
// Antwort der KI lesen: JSON (auch in Text oder Codeblock eingebettet), Werte auf -100..100,
// gesperrte Bereiche (settings) bleiben 0
Suggestion parse(const QString& reply, const Settings& settings);
// Vorschlag mit Stärke (Prozent) skalieren
adjust::Basic scaled(const adjust::Basic& basic, int percent);
// --- KI bearbeitet das Bild ---
enum Task {
    CastTask = 1,        // Farbstich, Weissabgleich
    ExposureTask = 2,    // Belichtung und Kontrast
    ColorsTask = 4,      // natürliche, kräftige Farben
    SkyTask = 8,         // Himmel
    SharpenTask = 16,    // schärfen
    DenoiseTask = 32,    // entrauschen
    SkinTask = 64,       // Haut glätten
    RestoreTask = 128,   // altes Foto restaurieren
    ColorizeTask = 256,  // Schwarzweiss kolorieren
};
struct TaskInfo {
    int flag;
    QString label;
    const char* prompt;
    bool changesContent; // nur mit dem KI-Bild selbst übertragbar
};
const QList<TaskInfo>& tasks();
bool changesContent(int tasks);

struct EditSettings {
    Goal goal = Goal::Natural;
    int tasks = CastTask | ExposureTask | ColorsTask;
    QString wish;
};
// Auftrag an das Bildmodell (englisch)
QString editPrompt(const EditSettings& settings);
// Bildmodelle je Zugangsart (erstes = Vorgabe); leer = Art kann keine Bilder bearbeiten
QStringList imageModels(const QString& kind);
// Google Gemini (generateContent): Anfrage, Antwort (Bild; sonst Text der KI in *text), Adresse
QByteArray geminiRequest(const QString& prompt, const QByteArray& png);
QImage geminiImage(const QByteArray& body, QString* text = nullptr);
QString geminiEndpoint(const QString& baseUrl, const QString& model);

// OpenAI-Bildformate: das nächstgelegene Seitenverhältnis, Bild darauf mit Rand aufgefüllt, und
// zurück (Rand weg, Originalgrösse)
QSize cloudSize(const QSize& image);
QImage padForCloud(const QImage& image, QRect* content);
QImage unpadFromCloud(const QImage& result, const QRect& content, const QSize& original);

} // namespace aiopt

class AiImageDialog : public ImagePreviewDialog {
    Q_OBJECT

public:
    enum class Method { Edit, Values };
    enum class Apply { Transfer, Replace };

    // cutout: Freistellen gleich eingeschaltet (Bitmaps > Bild freistellen)
    explicit AiImageDialog(const QImage& image, QWidget* parent = nullptr, bool cutout = false);
    ~AiImageDialog() override;

    Method method() const;
    void setMethod(Method method);
    Apply applyMode() const;
    void setApplyMode(Apply apply);
    aiopt::EditSettings editSettings() const;
    // KI bearbeitet das Bild (Knopf); jedes Ergebnis wird eine weitere Variante
    void editImage();
    // Ergebnis der KI als neue Variante (auch für Tests)
    void addEditedImage(const QImage& edited, const QString& note = {});
    int variantCount() const { return int(m_edits.size()); }

    aiopt::Settings settings() const;
    adjust::Basic basic() const;
    void setSuggestion(const aiopt::Suggestion& suggestion);
    // KI fragen (Knopf); läuft im Hintergrund, Ergebnis landet in den Reglern
    void ask();
    bool asking() const;
    // Freistellen
    enum class CutoutMethod { Local, Cloud };
    void setCutout(bool on);
    bool cutout() const;
    CutoutMethod cutoutMethod() const;
    // Für Tests: fertige Maske bzw. Cloud-Ergebnis setzen
    void setCutoutMask(const QImage& mask);
    void setCloudCutout(const QImage& rgba);
    bool cutoutWorking() const { return m_cutoutBusy; }
    // Zugänge neu einlesen (nach Optionen > Global > KI)
    void reloadAccounts();

signals:
    // Knopf „Einrichten …“: Hauptfenster öffnet Optionen > Global > KI
    void accountSetupRequested();

protected:
    QImage process(const QImage& image) const override;

private:
    void setBusy(bool busy, const QString& status = {});
    void startCutout();
    void runLocalCutout();
    void downloadModel();
    void runCloudCutout();
    void cutoutStatus(const QString& text);
    void saveSettings() const;
    void updateMode();
    void fillImageModels();
    const ai::Account* selectedAccount() const;
    void requestOpenAiEdit(const ai::Account& account, const QString& prompt);
    void requestGeminiEdit(const ai::Account& account, const QString& prompt);
    void editFailed(const QString& message);
    QImage editedBase(const QImage& image) const;
    QList<ai::Account> m_accounts;
    QComboBox* m_method = nullptr;
    QComboBox* m_imageModel = nullptr;
    QComboBox* m_quality = nullptr;
    QWidget* m_taskBox = nullptr;
    QList<QCheckBox*> m_tasks;
    QWidget* m_allowedBox = nullptr;
    QComboBox* m_apply = nullptr;
    QComboBox* m_variant = nullptr;
    QSlider* m_local = nullptr;
    QList<QImage> m_edits;
    QPointer<QNetworkReply> m_editReply;
    mutable QList<std::pair<QString, QImage>> m_baseCache;
    QFormLayout* m_form = nullptr;
    QFormLayout* m_values = nullptr;
    QComboBox* m_account = nullptr;
    QComboBox* m_goal = nullptr;
    QCheckBox* m_color = nullptr;
    QCheckBox* m_tone = nullptr;
    QCheckBox* m_saturation = nullptr;
    QLineEdit* m_wish = nullptr;
    QPushButton* m_ask = nullptr;
    QLabel* m_status = nullptr;
    QSlider* m_strength = nullptr;
    QCheckBox* m_stretch = nullptr;
    QList<QSlider*> m_sliders;
    QPointer<ai::Provider> m_provider;

    QCheckBox* m_cutout = nullptr;
    QComboBox* m_cutoutMethod = nullptr;
    QSlider* m_hardness = nullptr;
    QLabel* m_cutoutStatus = nullptr;
    QImage m_mask;        // lokal: Maske in Originalgrösse
    QImage m_cloudCutout; // Cloud: freigestelltes Bild in Originalgrösse
    bool m_cutoutBusy = false;
    QNetworkAccessManager* m_net = nullptr;
};

} // namespace vek

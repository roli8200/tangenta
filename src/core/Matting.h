#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>

#include <atomic>

namespace vek::matting {

// Motiv freistellen mit einem lokalen Segmentierungsmodell (ONNX). Die Maske wird auf dem Bild
// selbst berechnet und auf das Original gelegt: Kein Pixel des Motivs wird verändert, und kein Bild
// verlässt den Rechner.
//
// ONNX Runtime (MIT, Microsoft) wird zur Laufzeit geladen (onnxruntime.dll neben dem Programm,
// libonnxruntime.so(.1) bzw. .dylib aus dem System oder neben dem Programm, oder die Datei aus
// VEK_ONNXRUNTIME). Fehlt sie, ist die Funktion nicht verfügbar; TANGENTA läuft trotzdem.
struct Model {
    QString file;        // Dateiname im Modellordner
    QString name;        // für die Oberfläche
    QString url;         // Herkunft für den einmaligen Download
    QByteArray sha256;   // hex
    qint64 bytes = 0;
    QString license;
    int size = 1024;     // Eingabe size x size
    float mean[3] = {0.485f, 0.456f, 0.406f};
    float std[3] = {0.229f, 0.224f, 0.225f};
    bool sigmoid = true; // Ausgabe sind Logits
};
// BiRefNet (general, Swin-T, MIT): saubere Kanten, auch Haare
const Model& defaultModel();

// Ordner der Modelle (AppLocalDataLocation/models), VEK_MATTING_MODEL_DIR überschreibt ihn
QString modelDirectory();
QString modelPath(const Model& model);
bool modelPresent(const Model& model);

// ONNX Runtime laden; false mit verständlicher Meldung, wenn sie fehlt
bool runtimeAvailable(QString* error = nullptr);
QString runtimeVersion();

// Maske (Format_Grayscale8, Grösse des Bildes): 255 = Motiv, 0 = Hintergrund. Dauert je nach
// Rechner einige Sekunden; darf in einem eigenen Thread laufen. Leeres Bild bei Fehler.
QImage subjectMask(const QImage& image, const Model& model, QString* error = nullptr);

// Maske mit Kantenhärte anwenden: hardness 0 = Maske wie berechnet, 100 = fast hart. Das Ergebnis
// behält die Farbtiefe (16 Bit bleibt 16 Bit), der Hintergrund wird durchsichtig.
QImage applyMask(const QImage& image, const QImage& mask, int hardness = 0);

} // namespace vek::matting

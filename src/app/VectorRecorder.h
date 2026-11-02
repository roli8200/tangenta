#pragma once

#include <QBrush>
#include <QImage>
#include <QList>
#include <QPaintDevice>
#include <QPaintEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QTransform>

#include <memory>
#include <optional>

namespace vek {

// Zeichengerät, das mitschreibt statt Pixel zu setzen: Pfade mit Füllung/Kontur, Bilder,
// Beschneidungen – alles in Gerätekoordinaten. Damit entstehen EPS und DXF genau aus dem, was
// auch Bildschirm und PDF zeichnen (Canvas::renderPage), ohne jeden Objekttyp zu kennen.
struct RecordedItem {
    enum class Kind { Path, Image };
    Kind kind = Kind::Path;
    QPainterPath path;            // Gerätekoordinaten
    std::optional<QBrush> fill;   // Pinsel samt Transformation ins Gerät
    std::optional<QPen> stroke;   // Breite schon in Geräteeinheiten
    QImage image;                 // nur Image
    QTransform imageTransform;    // Bildpunkte -> Gerät
    std::optional<QPainterPath> clip;
    double opacity = 1.0;
    QPainter::CompositionMode composition = QPainter::CompositionMode_SourceOver;

    // Braucht Durchsicht oder Mischen mit dem Untergrund (Deckung, Alpha, Mischmodus)
    bool needsBlending() const;
    // Fläche, die das Element bedeckt (Füllung, Kontur als Fläche, Bildrechteck), samt Beschneidung
    QPainterPath coverage() const;
};

class VectorRecorder final : public QPaintDevice {
public:
    // size in Geräteeinheiten, dpi = Einheiten je Zoll (72 = Punkt)
    VectorRecorder(const QSizeF& size, double dpi);
    ~VectorRecorder() override;

    QPaintEngine* paintEngine() const override;
    const QList<RecordedItem>& items() const;
    QSizeF size() const { return m_size; }

protected:
    int metric(PaintDeviceMetric metric) const override;

private:
    QSizeF m_size;
    double m_dpi;
    std::unique_ptr<QPaintEngine> m_engine;
};

} // namespace vek

#pragma once

#include "CurveObject.h"
#include "DrawObject.h"

#include <QImage>
#include <QSizeF>

namespace vek {

// Bitmap: Pixel aus dem Bildpool, gezeigt in einem Rahmen.
//
// Lokal liegt das Bild von (0,0) bis size (mm). Der Rahmen ist ein Knotenpfad in denselben
// lokalen Koordinaten und anfangs das Bildrechteck; mit dem Formwerkzeug verschoben, ergänzt
// oder zu Kurven gemacht, beschneidet er das Bild, ohne Pixel zu verwerfen.
// Skalieren, Drehen und Neigen bleiben in der Transformation.
class ImageObject final : public DrawObject {
public:
    ImageObject() = default;
    ImageObject(const QString& key, const QSizeF& size);

    QString typeName() const override { return QStringLiteral("image"); }
    // Der Rahmen
    QPainterPath localPath() const override;
    bool hitTest(const QPointF& point, double tolerance, bool treatAsFilled) const override;

    QString key() const { return m_key; }
    // Neue Pixel (z.B. nach Bildanpassung); Grösse und Rahmen bleiben
    void setKey(const QString& key) { m_key = key; }
    // Volle Genauigkeit (16 Bit je Kanal) für Bearbeitung und Vektorisieren
    QImage image() const;
    // 8 Bit fürs Zeichnen am Bildschirm
    QImage displayImage() const;
    QSizeF size() const { return m_size; }
    void setSize(const QSizeF& size) { m_size = size; }

    const QList<SubPath>& frame() const { return m_frame; }
    void setFrame(const QList<SubPath>& frame) { m_frame = frame; }
    void resetFrame();
    bool isCropped() const;

    void writeProperties(QJsonObject& out) const override;
    void readProperties(const QJsonObject& in) override;

private:
    QString m_key;
    QSizeF m_size;
    QList<SubPath> m_frame;
};

} // namespace vek

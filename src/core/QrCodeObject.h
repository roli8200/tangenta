#pragma once

#include "DrawObject.h"

#include <QMap>
#include <QSizeF>
#include <QStringList>

#include <memory>
#include <optional>
#include <vector>

namespace vek {

// QR-Code: Inhalte aufbereiten und kodieren (Bibliothek qrcodegen, MIT)
namespace qr {

// Art des Inhalts; jede Art hat eigene Felder und wird zu einem Text, den
// Telefone verstehen (Webadresse, mailto:, tel:, SMSTO:, WIFI:, vCard)
enum class Kind { Url, Text, Email, Phone, Sms, Wifi, Contact };
constexpr int kKindCount = 7;
QString kindLabel(Kind kind);

struct Field {
    QString id;
    QString label;
    bool multiline = false;
    QStringList choices{};      // Auswahl statt Textfeld (Anzeige)
    QStringList choiceValues{}; // gespeicherte Werte zur Auswahl
};
QList<Field> fields(Kind kind);

// Text im QR-Code aus Art und Feldern (fehlende Felder = leer)
QString payload(Kind kind, const QMap<QString, QString>& values);

// Fehlerkorrektur: 0 = L (7 %), 1 = M (15 %), 2 = Q (25 %), 3 = H (30 %)
QStringList errorCorrectionLabels();

struct Matrix {
    int size = 0;    // Module je Seite (21 … 177), 0 = nichts kodiert
    int version = 0; // 1 … 40
    std::vector<bool> dark;
    bool at(int x, int y) const { return x >= 0 && y >= 0 && x < size && y < size && dark[std::size_t(y * size + x)]; }
};
// Leere Matrix, wenn der Text zu lang ist (error nennt dann den Grund)
Matrix encode(const QString& text, int errorCorrection, QString* error = nullptr);

} // namespace qr

// QR-Code als Objekt: bleibt bearbeitbar (Inhalt, Fehlerkorrektur, Form der Punkte).
// Lokal von (0,0) bis size, samt Ruhezone. Die Module werden mit der Füllung des Objekts
// gezeichnet, darunter auf Wunsch eine Hintergrundfläche über den ganzen Code.
class QrCodeObject final : public DrawObject {
public:
    enum class Shape { Squares, Dots };

    QrCodeObject();

    QString typeName() const override { return QStringLiteral("qrcode"); }
    QPainterPath localPath() const override;
    QPainterPath snapPath() const override;
    QRectF boundingRect() const override;
    bool hitTest(const QPointF& point, double tolerance, bool treatAsFilled) const override;
    void applyTransform(const QTransform& m) override;

    QSizeF size() const { return m_size; }
    void setSize(const QSizeF& size) { m_size = size; }

    qr::Kind kind() const { return m_kind; }
    QMap<QString, QString> values() const { return m_values; }
    void setContent(qr::Kind kind, const QMap<QString, QString>& values);
    QString payload() const { return qr::payload(m_kind, m_values); }

    int errorCorrection() const { return m_ecc; }
    void setErrorCorrection(int level) { m_ecc = std::clamp(level, 0, 3); }
    Shape shape() const { return m_shape; }
    void setShape(Shape s) { m_shape = s; }
    // Ruhezone in Modulen (Norm: 4)
    int quietZone() const { return m_quiet; }
    void setQuietZone(int modules) { m_quiet = std::clamp(modules, 0, 10); }
    std::optional<Color> background() const { return m_background; }
    void setBackground(std::optional<Color> c) { m_background = c; }
    // Ganze Fläche des Codes (lokal), für den Hintergrund
    QPainterPath backgroundPath() const;

    const qr::Matrix& matrix() const;

    QList<PropertyDescriptor> propertyDescriptors() const override;
    QVariant propertyValue(const QString& id) const override;
    bool setPropertyValue(const QString& id, const QVariant& value) override;

    void writeProperties(QJsonObject& out) const override;
    void readProperties(const QJsonObject& in) override;

private:
    QSizeF m_size{30.0, 30.0};
    qr::Kind m_kind = qr::Kind::Url;
    QMap<QString, QString> m_values;
    int m_ecc = 1;
    Shape m_shape = Shape::Squares;
    int m_quiet = 2;
    std::optional<Color> m_background = Color(255, 255, 255);

    // Kodierung hängt nur an Text und Fehlerkorrektur
    struct Cache {
        QString text;
        int ecc = -1;
        qr::Matrix matrix;
    };
    mutable std::shared_ptr<Cache> m_cache;
};

} // namespace vek

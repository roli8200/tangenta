#pragma once

#include "DrawObject.h"

#include <QList>
#include <QPainterPath>
#include <QPointF>
#include <QSizeF>
#include <QString>

namespace vek {

// Perfekte Formen (Grundformen, Pfeile, Flussdiagramm, Banner/Sterne,
// Legenden). Jede Form hat höchstens einen Glyphengriff: einen Punkt im Einheitsquadrat der
// Form, den sie auf ihre Weise deutet (Einzug, Dicke, Spitze der Legende …).
namespace perfect {

enum class Category { Basic, Arrow, Flowchart, Banner, Callout };

struct Info {
    Category category;
    QString name;
    QPointF glyph;    // Vorgabe des Griffs (Einheitskoordinaten)
    bool hasGlyph;
};

int count();
const Info& info(int kind);
QList<int> kinds(Category category);
QString categoryName(Category category);
// Umriss im Einheitsquadrat (0..1, y nach unten)
QPainterPath unitPath(int kind, const QPointF& glyph);

} // namespace perfect

class PerfectShapeObject final : public DrawObject {
public:
    PerfectShapeObject() = default;
    PerfectShapeObject(int kind, const QSizeF& size);

    QString typeName() const override { return QStringLiteral("perfectshape"); }
    QPainterPath localPath() const override;
    void applyTransform(const QTransform& m) override;

    int kind() const { return m_kind; }
    void setKind(int kind);
    QSizeF size() const { return m_size; }
    // Glyphengriff in Einheitskoordinaten bzw. lokal (mm)
    QPointF glyph() const { return m_glyph; }
    void setGlyph(const QPointF& unit);
    QPointF glyphLocal() const { return QPointF(m_glyph.x() * m_size.width(), m_glyph.y() * m_size.height()); }

    QList<PropertyDescriptor> propertyDescriptors() const override;
    QVariant propertyValue(const QString& id) const override;
    bool setPropertyValue(const QString& id, const QVariant& value) override;

    void writeProperties(QJsonObject& out) const override;
    void readProperties(const QJsonObject& in) override;

private:
    int m_kind = 0;
    QSizeF m_size{10, 10};
    QPointF m_glyph{0.25, 0.5};
};

} // namespace vek

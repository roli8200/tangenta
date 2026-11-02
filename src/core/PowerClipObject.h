#pragma once

#include "DrawObject.h"

namespace vek {

// PowerClip: ein Rahmen (erstes Kind, beliebiges Objekt) zeigt Inhalte
// (weitere Kinder) nur innerhalb seiner Form. Wie eine Gruppe ohne eigene Transformation; die
// Kinder liegen in Seitenkoordinaten. Mit "Inhalt sperren" (Vorgabe) bewegen sich die Inhalte
// mit dem Rahmen, sonst bleiben sie stehen.
class PowerClipObject final : public DrawObject {
public:
    QString typeName() const override { return QStringLiteral("powerclip"); }
    bool canContain(const Node& child) const override { return node_cast<DrawObject>(&child) != nullptr; }

    const DrawObject* frame() const;
    QList<const DrawObject*> contents() const;

    QPainterPath localPath() const override { return path(); }
    QPainterPath path() const override;
    QRectF boundingRect() const override;
    bool hitTest(const QPointF& point, double tolerance, bool treatAsFilled) const override;
    void applyTransform(const QTransform& m) override;
    void setStyle(const Style& s) override;

    bool contentsLocked() const { return m_locked; }
    void setContentsLocked(bool on) { m_locked = on; }

    void writeProperties(QJsonObject& out) const override;
    void readProperties(const QJsonObject& in) override;

private:
    bool m_locked = true;
};

} // namespace vek

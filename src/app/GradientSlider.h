#pragma once

#include <QLineF>
#include <QPainter>
#include <QPointF>
#include <QString>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace vek::slider {

// Regler auf der gestrichelten Linie zwischen Anfangs- und Endpunkt
// (Schatten: Deckung, Verläufe: Mittelpunkt). Alles in Bildschirmkoordinaten; f = Lage 0..1.

inline QPointF at(const QPointF& s, const QPointF& e, double f)
{
    return s + (e - s) * f;
}

inline bool hit(const QPointF& pos, const QPointF& s, const QPointF& e, double f)
{
    return QLineF(pos, at(s, e, f)).length() <= 7.0;
}

// Lage des auf die Linie projizierten Punkts, auf [lo, hi] begrenzt
inline double fraction(const QPointF& pos, const QPointF& s, const QPointF& e, double lo, double hi)
{
    const QPointF axis = e - s;
    const double len2 = QPointF::dotProduct(axis, axis);
    const double f = len2 > 0 ? QPointF::dotProduct(pos - s, axis) / len2 : 0.5;
    return std::clamp(f, lo, hi);
}

// Kleiner Balken quer zur Linie, darunter optional der Wert
inline void paint(QPainter& p, const QPointF& s, const QPointF& e, double f, const QString& text = {})
{
    const QPointF m = at(s, e, f);
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.translate(m);
    p.rotate(qRadiansToDegrees(std::atan2(e.y() - s.y(), e.x() - s.x())));
    p.setPen(QPen(Qt::black, 1.0));
    p.setBrush(QColor(0xf0, 0xf0, 0xf0));
    p.drawRect(QRectF(-2.5, -6.5, 5.0, 13.0));
    p.restore();
    if (text.isEmpty())
        return;
    const QPointF where = m + QPointF(9, -9);
    p.save();
    p.setPen(Qt::white);
    for (const QPointF& d : {QPointF(-1, 0), QPointF(1, 0), QPointF(0, -1), QPointF(0, 1)})
        p.drawText(where + d, text);
    p.setPen(Qt::black);
    p.drawText(where, text);
    p.restore();
}

} // namespace vek::slider

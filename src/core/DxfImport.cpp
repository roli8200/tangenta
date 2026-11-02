#include "DxfImport.h"

#include "CurveObject.h"
#include "ImageObject.h"
#include "ImagePool.h"
#include "PathOps.h"
#include "Serialization.h"
#include "TextObject.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QStringList>
#include <QTransform>
#include <QtMath>

#include <cmath>
#include <map>
#include <numbers>

namespace vek::dxf {

QColor aciColor(int index)
{
    static const QColor base[10] = {QColor(0, 0, 0),     QColor(255, 0, 0),   QColor(255, 255, 0), QColor(0, 255, 0),     QColor(0, 255, 255),
                                    QColor(0, 0, 255),   QColor(255, 0, 255), QColor(0, 0, 0) /* 7: Schwarz auf weissem Papier */,
                                    QColor(128, 128, 128), QColor(192, 192, 192)};
    if (index >= 0 && index <= 9)
        return base[index];
    if (index >= 250 && index <= 255) {
        const int g = 51 + (index - 250) * 41;
        return QColor(g, g, g);
    }
    if (index < 10 || index > 249)
        return Qt::black;
    // 10..249: 24 Farbtöne zu je 10 Stufen (Helligkeit und Sättigung)
    const int hue = ((index - 10) / 10) * 15;
    static const double value[5] = {1.0, 0.8, 0.6, 0.5, 0.3};
    const int k = index % 10;
    const double v = value[k / 2];
    const double s = k % 2 ? 0.5 : 1.0;
    return QColor::fromHsvF(float(hue / 360.0), float(s), float(v));
}

int nearestAci(const QColor& color)
{
    int best = 7;
    double bestD = 1e300;
    for (int i = 1; i <= 255; ++i) {
        const QColor c = aciColor(i);
        const double d = std::pow(c.red() - color.red(), 2) * 3 + std::pow(c.green() - color.green(), 2) * 4 + std::pow(c.blue() - color.blue(), 2) * 2;
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return best;
}

namespace {

using Entity = QList<std::pair<int, QString>>;

QString value(const Entity& e, int code, const QString& def = {})
{
    for (const auto& [c, v] : e)
        if (c == code)
            return v;
    return def;
}

double num(const Entity& e, int code, double def = 0.0)
{
    const QString v = value(e, code);
    bool ok = false;
    const double d = v.toDouble(&ok);
    return ok ? d : def;
}

QList<double> nums(const Entity& e, int code)
{
    QList<double> out;
    for (const auto& [c, v] : e)
        if (c == code)
            out << v.toDouble();
    return out;
}

// Segment mit Wölbung (bulge = tan(Winkel/4)) als Bogen anhängen
void bulgeTo(QPainterPath& path, const QPointF& from, const QPointF& to, double bulge)
{
    if (std::abs(bulge) < 1e-9) {
        path.lineTo(to);
        return;
    }
    const double angle = 4.0 * std::atan(bulge); // Gegenuhrzeigersinn, wenn positiv (y nach oben)
    const double chord = QLineF(from, to).length();
    if (chord < 1e-12)
        return;
    const double radius = chord / (2.0 * std::sin(std::abs(angle) / 2.0));
    const QPointF mid = (from + to) / 2.0;
    const QPointF dir = (to - from) / chord;
    const QPointF normal(-dir.y(), dir.x());
    const double h = radius * std::cos(angle / 2.0);
    const QPointF center = mid + normal * (bulge > 0 ? h : -h) * (std::abs(angle) > std::numbers::pi ? -1 : 1);
    const double a0 = std::atan2(from.y() - center.y(), from.x() - center.x());
    const int steps = std::max(2, int(std::ceil(std::abs(angle) / (std::numbers::pi / 16))));
    for (int i = 1; i <= steps; ++i) {
        const double a = a0 + angle * i / steps;
        path.lineTo(i == steps ? to : center + QPointF(std::cos(a), std::sin(a)) * radius);
    }
}

// NURBS-Kurve (de Boor) als Punktfolge
QList<QPointF> nurbs(int degree, const QList<QPointF>& ctrl, QList<double> knots, QList<double> weights)
{
    QList<QPointF> out;
    const int n = int(ctrl.size());
    if (n < 2)
        return ctrl;
    degree = std::clamp(degree, 1, n - 1);
    if (knots.size() != n + degree + 1) {
        // Gleichmässig, an den Enden geklemmt
        knots.clear();
        for (int i = 0; i < n + degree + 1; ++i)
            knots << (i <= degree ? 0.0 : i >= n ? double(n - degree) : double(i - degree));
    }
    if (weights.size() != n)
        weights = QList<double>(n, 1.0);
    const double t0 = knots[degree], t1 = knots[n];
    const int samples = std::clamp(n * 16, 16, 2000);
    for (int s = 0; s <= samples; ++s) {
        const double t = std::min(t0 + (t1 - t0) * s / samples, t1 - 1e-12 * (t1 - t0 + 1));
        int k = degree;
        while (k < n - 1 && knots[k + 1] <= t)
            ++k;
        // Homogene Koordinaten
        QList<std::array<double, 3>> d;
        for (int j = 0; j <= degree; ++j) {
            const int i = k - degree + j;
            const double w = weights[i];
            d << std::array<double, 3>{ctrl[i].x() * w, ctrl[i].y() * w, w};
        }
        for (int r = 1; r <= degree; ++r)
            for (int j = degree; j >= r; --j) {
                const int i = k - degree + j;
                const double den = knots[i + degree - r + 1] - knots[i];
                const double a = den > 0 ? (t - knots[i]) / den : 0.0;
                for (int c = 0; c < 3; ++c)
                    d[j][c] = (1 - a) * d[j - 1][c] + a * d[j][c];
            }
        const auto& p = d[degree];
        out << QPointF(p[0] / p[2], p[1] / p[2]);
    }
    return out;
}

QString mtextPlain(QString t)
{
    // Formatierungen von MTEXT entfernen
    t.replace(QLatin1String("\\P"), QLatin1String("\n"));
    static const QRegularExpression codes(QStringLiteral(R"(\\[A-Za-z][^;\\{}]*;)"));
    t.remove(codes);
    t.remove(QLatin1Char('{')).remove(QLatin1Char('}'));
    t.replace(QLatin1String("\\~"), QLatin1String(" "));
    t.replace(QLatin1String("%%d"), QStringLiteral("°")).replace(QLatin1String("%%c"), QStringLiteral("⌀")).replace(QLatin1String("%%p"), QStringLiteral("±"));
    return t;
}

struct Layer {
    int aci = 7;
    std::optional<QColor> trueColor;
};

class Reader {
public:
    bool parse(const QByteArray& data, QString* error)
    {
        // Paare aus Gruppencode und Wert, Zeile für Zeile
        QList<QByteArray> lines = data.split('\n');
        QList<std::pair<int, QString>> pairs;
        for (qsizetype i = 0; i + 1 < lines.size(); i += 2) {
            bool ok = false;
            const int code = lines[i].trimmed().toInt(&ok);
            if (!ok) {
                if (error)
                    *error = QObject::tr("Keine DXF-Datei im Textformat (binäres DXF wird nicht unterstützt).");
                return false;
            }
            QByteArray v = lines[i + 1];
            if (v.endsWith('\r'))
                v.chop(1);
            pairs << std::pair{code, QString::fromLocal8Bit(v).trimmed()};
        }
        QString section;
        QString blockName;
        Entity current;
        QList<Entity>* target = &m_entities;
        auto flush = [&] {
            if (!current.isEmpty())
                target->append(current);
            current.clear();
        };
        for (int i = 0; i < pairs.size(); ++i) {
            const auto& [code, val] = pairs[i];
            if (code == 0) {
                flush();
                if (val == QLatin1String("SECTION")) {
                    section = i + 1 < pairs.size() ? pairs[i + 1].second : QString();
                    continue;
                }
                if (val == QLatin1String("ENDSEC")) {
                    section.clear();
                    target = &m_entities;
                    continue;
                }
                if (section == QLatin1String("BLOCKS")) {
                    if (val == QLatin1String("BLOCK")) {
                        // Name und Basispunkt im Kopf des Blocks
                        Entity head;
                        int j = i + 1;
                        for (; j < pairs.size() && pairs[j].first != 0; ++j)
                            head << pairs[j];
                        blockName = value(head, 2);
                        m_blockBase[blockName] = QPointF(num(head, 10), num(head, 20));
                        target = &m_blocks[blockName];
                        i = j - 1;
                        continue;
                    }
                    if (val == QLatin1String("ENDBLK")) {
                        target = &m_blocks[QStringLiteral("\x01")];
                        continue;
                    }
                }
                if (section == QLatin1String("TABLES") && val == QLatin1String("LAYER")) {
                    Entity layer;
                    int j = i + 1;
                    for (; j < pairs.size() && pairs[j].first != 0; ++j)
                        layer << pairs[j];
                    if (!value(layer, 2).isEmpty()) {
                        Layer l;
                        l.aci = std::abs(int(num(layer, 62, 7)));
                        if (const QString tc = value(layer, 420); !tc.isEmpty())
                            l.trueColor = QColor::fromRgb(QRgb(tc.toUInt()));
                        m_layers[value(layer, 2)] = l;
                    }
                    i = j - 1;
                    continue;
                }
                if (section == QLatin1String("HEADER"))
                    continue;
                if (section == QLatin1String("OBJECTS") && val == QLatin1String("IMAGEDEF")) {
                    Entity def;
                    int j = i + 1;
                    for (; j < pairs.size() && pairs[j].first != 0; ++j)
                        def << pairs[j];
                    m_imageFiles[value(def, 5)] = value(def, 1);
                    i = j - 1;
                    continue;
                }
                if (section == QLatin1String("ENTITIES") || section == QLatin1String("BLOCKS"))
                    current << std::pair{0, val};
                continue;
            }
            if (section == QLatin1String("HEADER") && code == 9 && val == QLatin1String("$INSUNITS") && i + 1 < pairs.size())
                m_units = pairs[i + 1].second.toInt();
            if (!current.isEmpty())
                current << std::pair{code, val};
        }
        flush();
        return true;
    }

    QString m_baseDir;

    svg::Import build()
    {
        // Erst in DXF-Koordinaten (y nach oben) sammeln, dann umrechnen
        convertAll(m_entities, QTransform(), QString(), 0);
        svg::Import result;
        result.group = std::make_unique<GroupObject>();
        QRectF extent;
        for (const Shape& s : std::as_const(m_shapes))
            extent = extent.isNull() ? s.extent() : extent.united(s.extent());
        if (extent.isNull())
            return result;
        double unit = 1.0; // mm je Zeichnungseinheit
        switch (m_units) {
        case 1: unit = 25.4; break;
        case 2: unit = 304.8; break;
        case 5: unit = 10.0; break;
        case 6: unit = 1000.0; break;
        case 8: unit = 0.0000254; break;
        case 9: unit = 0.0254; break;
        case 10: unit = 914.4; break;
        default: break;
        }
        const QTransform toMm = QTransform::fromTranslate(-extent.left(), -extent.bottom()) * QTransform(unit, 0, 0, -unit, 0, 0);
        result.size = QSizeF(extent.width() * unit, extent.height() * unit);
        std::map<QString, std::unique_ptr<GroupObject>> layers;
        for (Shape& s : m_shapes) {
            std::unique_ptr<DrawObject> obj;
            const QColor c = s.color;
            if (s.image) {
                auto img = std::make_unique<ImageObject>(s.imageKey, s.imageSize);
                img->setTransform(s.imageToDxf * toMm);
                obj = std::move(img);
            } else if (s.text) {
                const double height = s.textHeight * unit;
                auto t = std::make_unique<TextObject>(TextObject::Kind::Artistic);
                t->setPlainText(s.textContent, CharFormat(QStringLiteral("Arial"), std::max(1.0, height / 0.7 * 72.0 / 25.4)));
                const QPointF at = toMm.map(s.textAt);
                QTransform rot;
                rot.rotate(-s.textAngle);
                // Einfügepunkt = Grundlinie links; Text-Block beginnt eine Oberlänge darüber
                t->setTransform(QTransform::fromTranslate(0, -height / 0.7 * 0.8) * rot * QTransform::fromTranslate(at.x(), at.y()));
                vek::Style st;
                st.fill = Fill::solid(Color(std::uint8_t(c.red()), std::uint8_t(c.green()), std::uint8_t(c.blue())));
                st.stroke.reset();
                t->setStyle(st);
                obj = std::move(t);
            } else {
                // Doppelte Punkte (Kantenende = nächster Kantenanfang) stören die Kurvenanpassung
                QPainterPath mm;
                for (const QPolygonF& poly : toMm.map(s.path).toSubpathPolygons()) {
                    QPolygonF clean;
                    for (const QPointF& pt : poly)
                        if (clean.isEmpty() || QLineF(clean.last(), pt).length() > 1e-6)
                            clean << pt;
                    if (clean.size() < 2)
                        continue;
                    mm.moveTo(clean.first());
                    for (qsizetype k = 1; k < clean.size(); ++k)
                        mm.lineTo(clean[k]);
                    if (poly.isClosed() && clean.size() > 2)
                        mm.closeSubpath();
                }
                mm.setFillRule(Qt::OddEvenFill);
                std::unique_ptr<CurveObject> curve = std::make_unique<CurveObject>(pathops::fitPath(mm, 0.01));
                if (curve->subPaths().isEmpty())
                    continue;
                vek::Style st;
                const Color col(std::uint8_t(c.red()), std::uint8_t(c.green()), std::uint8_t(c.blue()));
                if (s.filled) {
                    st.fill = s.gradient ? *s.gradient : Fill::solid(col);
                    st.stroke.reset();
                } else {
                    st.fill = Fill::none();
                    st.stroke = col;
                    st.strokeWidth = s.lineWeight > 0 ? s.lineWeight / 100.0 : 0.25;
                }
                curve->setStyle(st);
                obj = std::move(curve);
            }
            if (s.alpha < 0.999)
                obj->setEffect(Effect::Transparency, Ramp::uniform((1.0 - s.alpha) * 100.0));
            auto& group = layers[s.layer];
            if (!group) {
                group = std::make_unique<GroupObject>();
                group->setName(s.layer.isEmpty() ? QStringLiteral("0") : s.layer);
            }
            attachDetachedChild(*group, std::move(obj));
        }
        for (auto& [name, group] : layers)
            attachDetachedChild(*result.group, std::move(group));
        return result;
    }

private:
    struct Shape {
        QPainterPath path;
        QColor color;
        QString layer;
        bool filled = false;
        double lineWeight = 0;
        bool text = false;
        QString textContent;
        QPointF textAt;
        double textHeight = 0;
        double textAngle = 0;
        double alpha = 1.0;
        std::optional<Fill> gradient;
        bool image = false;
        QString imageKey;
        QSizeF imageSize;
        QTransform imageToDxf; // Bildpunkte (oben links) -> Zeichnung
        QRectF extent() const
        {
            if (image)
                return imageToDxf.mapRect(QRectF(QPointF(0, 0), imageSize));
            if (!text)
                return path.boundingRect();
            return QRectF(textAt, QSizeF(textHeight * textContent.size() * 0.6, textHeight));
        }
    };

    QColor colorOf(const Entity& e, const QString& layer, const QColor& byBlock) const
    {
        if (const QString tc = value(e, 420); !tc.isEmpty())
            return QColor::fromRgb(QRgb(tc.toUInt()));
        const int aci = int(num(e, 62, 256));
        if (aci == 0)
            return byBlock;
        if (aci == 256 || aci < 0) {
            const Layer l = m_layers.value(layer);
            return l.trueColor ? *l.trueColor : aciColor(l.aci);
        }
        return aciColor(aci);
    }

    static double alphaOf(const Entity& e)
    {
        // 440: 0x020000TT, TT = Deckung 0..255
        const QString v = value(e, 440);
        if (v.isEmpty())
            return 1.0;
        const quint32 t = quint32(v.toLongLong());
        return (t & 0x02000000u) ? (t & 0xFF) / 255.0 : 1.0;
    }

    void add(QPainterPath path, const QTransform& t, const Entity& e, const QString& layer, const QColor& byBlock, bool filled = false)
    {
        Shape s;
        s.path = t.map(path);
        s.color = colorOf(e, layer, byBlock);
        s.layer = layer;
        s.filled = filled;
        s.lineWeight = num(e, 370, 0);
        s.alpha = alphaOf(e);
        if (!s.path.isEmpty())
            m_shapes << s;
    }

    void convertAll(const QList<Entity>& list, const QTransform& t, const QString& parentLayer, int depth, const QColor& byBlock = Qt::black)
    {
        if (depth > 16)
            return;
        for (int i = 0; i < list.size(); ++i) {
            const Entity& e = list[i];
            const QString type = e.first().second;
            QString layer = value(e, 8, QStringLiteral("0"));
            if (layer == QLatin1String("0") && !parentLayer.isEmpty())
                layer = parentLayer; // Ebene 0 im Block erbt die Ebene der Einfügung
            if (type == QLatin1String("POLYLINE")) {
                // Eckpunkte folgen als VERTEX bis SEQEND
                QList<Entity> vertices;
                int j = i + 1;
                for (; j < list.size() && list[j].first().second == QLatin1String("VERTEX"); ++j)
                    vertices << list[j];
                if (j < list.size() && list[j].first().second == QLatin1String("SEQEND"))
                    ++j;
                i = j - 1;
                const int flags = int(num(e, 70));
                if (flags & (16 | 64))
                    continue; // Netze
                QList<QPointF> pts;
                QList<double> bulges;
                for (const Entity& v : vertices) {
                    pts << QPointF(num(v, 10), num(v, 20));
                    bulges << num(v, 42);
                }
                add(polyPath(pts, bulges, flags & 1), t, e, layer, byBlock);
                continue;
            }
            convert(e, type, t, layer, depth, byBlock);
        }
    }

    QPainterPath polyPath(const QList<QPointF>& pts, const QList<double>& bulges, bool closed) const
    {
        QPainterPath p;
        if (pts.isEmpty())
            return p;
        p.moveTo(pts.first());
        for (int k = 1; k < pts.size(); ++k)
            bulgeTo(p, pts[k - 1], pts[k], bulges.value(k - 1));
        if (closed && pts.size() > 2) {
            bulgeTo(p, pts.last(), pts.first(), bulges.value(int(pts.size()) - 1));
            p.closeSubpath();
        }
        return p;
    }

    void convert(const Entity& e, const QString& type, const QTransform& t, const QString& layer, int depth, const QColor& byBlock)
    {
        QPainterPath p;
        if (type == QLatin1String("LINE")) {
            p.moveTo(num(e, 10), num(e, 20));
            p.lineTo(num(e, 11), num(e, 21));
        } else if (type == QLatin1String("LWPOLYLINE")) {
            // Eckpunkte: je 10/20, Wölbung 42 gehört zum vorangehenden Punkt
            QList<QPointF> pts;
            QList<double> bulges;
            double x = 0;
            bool haveX = false;
            for (const auto& [code, v] : e) {
                if (code == 10) {
                    x = v.toDouble();
                    haveX = true;
                } else if (code == 20 && haveX) {
                    pts << QPointF(x, v.toDouble());
                    bulges << 0.0;
                    haveX = false;
                } else if (code == 42 && !bulges.isEmpty()) {
                    bulges.last() = v.toDouble();
                }
            }
            p = polyPath(pts, bulges, int(num(e, 70)) & 1);
        } else if (type == QLatin1String("CIRCLE")) {
            const double r = num(e, 40);
            p.addEllipse(QPointF(num(e, 10), num(e, 20)), r, r);
        } else if (type == QLatin1String("ARC")) {
            const QPointF c(num(e, 10), num(e, 20));
            const double r = num(e, 40);
            double a0 = qDegreesToRadians(num(e, 50)), a1 = qDegreesToRadians(num(e, 51));
            if (a1 <= a0)
                a1 += 2 * std::numbers::pi;
            const int steps = std::max(4, int((a1 - a0) / (std::numbers::pi / 32)));
            for (int k = 0; k <= steps; ++k) {
                const double a = a0 + (a1 - a0) * k / steps;
                const QPointF pt = c + QPointF(std::cos(a), std::sin(a)) * r;
                k == 0 ? p.moveTo(pt) : p.lineTo(pt);
            }
        } else if (type == QLatin1String("ELLIPSE")) {
            const QPointF c(num(e, 10), num(e, 20)), major(num(e, 11), num(e, 21));
            const double ratio = num(e, 40, 1.0);
            double t0 = num(e, 41, 0.0), t1 = num(e, 42, 2 * std::numbers::pi);
            if (t1 <= t0)
                t1 += 2 * std::numbers::pi;
            const QPointF minor = QPointF(-major.y(), major.x()) * ratio;
            const int steps = std::max(8, int((t1 - t0) / (std::numbers::pi / 32)));
            for (int k = 0; k <= steps; ++k) {
                const double a = t0 + (t1 - t0) * k / steps;
                const QPointF pt = c + major * std::cos(a) + minor * std::sin(a);
                k == 0 ? p.moveTo(pt) : p.lineTo(pt);
            }
            if (std::abs(t1 - t0 - 2 * std::numbers::pi) < 1e-6)
                p.closeSubpath();
        } else if (type == QLatin1String("SPLINE")) {
            QList<QPointF> ctrl;
            const QList<double> xs = nums(e, 10), ys = nums(e, 20);
            for (int k = 0; k < std::min(xs.size(), ys.size()); ++k)
                ctrl << QPointF(xs[k], ys[k]);
            QList<QPointF> pts;
            if (ctrl.size() >= 2) {
                pts = nurbs(int(num(e, 71, 3)), ctrl, nums(e, 40), nums(e, 41));
            } else {
                // nur Stützpunkte (fit points): als Linienzug
                const QList<double> fx = nums(e, 11), fy = nums(e, 21);
                for (int k = 0; k < std::min(fx.size(), fy.size()); ++k)
                    pts << QPointF(fx[k], fy[k]);
            }
            for (int k = 0; k < pts.size(); ++k)
                k == 0 ? p.moveTo(pts[k]) : p.lineTo(pts[k]);
            if (int(num(e, 70)) & 1)
                p.closeSubpath();
        } else if (type == QLatin1String("SOLID") || type == QLatin1String("3DFACE") || type == QLatin1String("TRACE")) {
            // Ecken 1,2,4,3 (SOLID) bzw. der Reihe nach (3DFACE)
            QList<QPointF> c{QPointF(num(e, 10), num(e, 20)), QPointF(num(e, 11), num(e, 21)), QPointF(num(e, 12), num(e, 22)),
                             QPointF(num(e, 13, num(e, 12)), num(e, 23, num(e, 22)))};
            if (type != QLatin1String("3DFACE"))
                std::swap(c[2], c[3]);
            p.addPolygon(QPolygonF(c));
            p.closeSubpath();
            add(p, t, e, layer, byBlock, type != QLatin1String("3DFACE"));
            return;
        } else if (type == QLatin1String("HATCH")) {
            p = hatchPath(e);
            if (int(num(e, 70)) & 1) {
                add(p, t, e, layer, byBlock, true);
                const QList<double> colors = nums(e, 421);
                if (num(e, 450) == 1 && colors.size() >= 2 && !m_shapes.isEmpty()) {
                    auto rgb = [](double v) {
                        const QRgb c = QRgb(quint32(v));
                        return Color(std::uint8_t(qRed(c)), std::uint8_t(qGreen(c)), std::uint8_t(qBlue(c)));
                    };
                    const bool linear = value(e, 470).contains(QLatin1String("LINEAR"), Qt::CaseInsensitive);
                    Fill f = Fill::gradient(linear ? Fill::Type::Linear : Fill::Type::Radial, rgb(colors[0]), rgb(colors[1]));
                    if (linear) {
                        // Winkel in der Zeichnung (y nach oben) -> Anteile des Begrenzungsrechtecks (y nach unten)
                        const double a = num(e, 460) + std::atan2(t.m12(), t.m11());
                        const QPointF d(std::cos(a) / 2, -std::sin(a) / 2);
                        f.start = QPointF(0.5, 0.5) - d;
                        f.end = QPointF(0.5, 0.5) + d;
                    } else {
                        f.start = QPointF(0.5, 0.5);
                        f.end = QPointF(1.0, 0.5);
                    }
                    m_shapes.last().gradient = f;
                }
                return;
            }
        } else if (type == QLatin1String("TEXT") || type == QLatin1String("MTEXT") || type == QLatin1String("ATTRIB")) {
            QString content = value(e, 1);
            if (type == QLatin1String("MTEXT")) {
                QString chunks;
                for (const auto& [code, v] : e)
                    if (code == 3)
                        chunks += v;
                content = mtextPlain(chunks + content);
            }
            if (content.trimmed().isEmpty())
                return;
            Shape s;
            s.text = true;
            s.textContent = content.split(QLatin1Char('\n')).value(0);
            const double scale = std::sqrt(std::abs(t.determinant()));
            s.textHeight = num(e, 40, 2.5) * scale;
            QPointF at(num(e, 10), num(e, 20));
            // Ausgerichteter TEXT: zweiter Ausrichtungspunkt
            if (type == QLatin1String("TEXT") && (num(e, 72) != 0 || num(e, 73) != 0) && !value(e, 11).isEmpty())
                at = QPointF(num(e, 11), num(e, 21));
            if (type == QLatin1String("MTEXT")) {
                // Anheftung 1..9: oben links .. unten rechts; Einfügepunkt ist die Anheftung
                const int attach = std::max(1, int(num(e, 71, 1)));
                const int row = (attach - 1) / 3;
                at.ry() -= num(e, 40, 2.5) * (row == 0 ? 1.0 : row == 1 ? 0.5 : 0.0);
            }
            s.textAt = t.map(at);
            s.textAngle = num(e, 50) + qRadiansToDegrees(std::atan2(t.m12(), t.m11()));
            s.color = colorOf(e, layer, byBlock);
            s.layer = layer;
            m_shapes << s;
            return;
        } else if (type == QLatin1String("IMAGE")) {
            const QString file = m_imageFiles.value(value(e, 340));
            if (file.isEmpty())
                return;
            // Pfad absolut, sonst neben der Zeichnung; AutoCAD speichert oft nur den Namen
            QStringList tries{file, QDir(m_baseDir).filePath(file), QDir(m_baseDir).filePath(QFileInfo(QString(file).replace(QLatin1Char('\\'), QLatin1Char('/'))).fileName())};
            QByteArray bytes;
            for (const QString& path : std::as_const(tries)) {
                QFile f(path);
                if (!path.isEmpty() && f.open(QIODevice::ReadOnly)) {
                    bytes = f.readAll();
                    break;
                }
            }
            const QString key = bytes.isEmpty() ? QString() : imagepool::add(bytes);
            if (key.isEmpty())
                return;
            const QImage img = imagepool::image(key);
            const double w = num(e, 13, img.width()), h = num(e, 23, img.height());
            const QPointF ins(num(e, 10), num(e, 20)), u(num(e, 11), num(e, 21)), v(num(e, 12), num(e, 22));
            // Bildpunkt (x, y von oben) -> Einfügepunkt + u*x + v*(h - y)
            Shape s;
            s.image = true;
            s.imageKey = key;
            s.imageSize = QSizeF(img.width(), img.height());
            const double sx = w / std::max(1, img.width()), sy = h / std::max(1, img.height());
            s.imageToDxf = QTransform::fromScale(sx, sy) * QTransform(u.x(), u.y(), -v.x(), -v.y(), ins.x() + v.x() * h, ins.y() + v.y() * h) * t;
            s.layer = layer;
            s.alpha = alphaOf(e);
            m_shapes << s;
            return;
        } else if (type == QLatin1String("INSERT") || type == QLatin1String("DIMENSION")) {
            const QString name = value(e, 2);
            if (!m_blocks.contains(name))
                return;
            QTransform m;
            if (type == QLatin1String("INSERT")) {
                const QPointF base = m_blockBase.value(name);
                QTransform rot;
                rot.rotate(num(e, 50));
                m = QTransform::fromTranslate(-base.x(), -base.y()) * QTransform::fromScale(num(e, 41, 1.0), num(e, 42, 1.0)) * rot
                    * QTransform::fromTranslate(num(e, 10), num(e, 20));
            }
            convertAll(m_blocks.value(name), m * t, layer, depth + 1, colorOf(e, layer, byBlock));
            return;
        } else {
            return;
        }
        add(p, t, e, layer, byBlock);
    }

    // Ränder einer Schraffur (Polylinie oder Kanten aus Linien/Bögen)
    QPainterPath hatchPath(const Entity& e) const
    {
        QPainterPath path;
        int i = 0;
        auto at = [&](int k) { return k < e.size() ? e[k] : std::pair<int, QString>{-1, QString()}; };
        // bis zur Anzahl der Ränder (91)
        while (i < e.size() && e[i].first != 91)
            ++i;
        const int loops = at(i).second.toInt();
        ++i;
        for (int l = 0; l < loops && i < e.size(); ++l) {
            while (i < e.size() && e[i].first != 92)
                ++i;
            const int flags = at(i).second.toInt();
            ++i;
            if (flags & 2) {
                // Polylinie: 72 Wölbung ja/nein, 73 geschlossen, 93 Anzahl, dann 10/20(/42)
                bool hasBulge = false;
                int count = 0;
                for (; i < e.size() && e[i].first != 10; ++i) {
                    if (e[i].first == 72)
                        hasBulge = e[i].second.toInt() != 0;
                    if (e[i].first == 93)
                        count = e[i].second.toInt();
                }
                QList<QPointF> pts;
                QList<double> bulges;
                for (int k = 0; k < count && i < e.size(); ++k) {
                    const double x = at(i).second.toDouble();
                    const double y = at(i + 1).second.toDouble();
                    i += 2;
                    double b = 0;
                    if (hasBulge && at(i).first == 42) {
                        b = at(i).second.toDouble();
                        ++i;
                    }
                    pts << QPointF(x, y);
                    bulges << b;
                }
                path.addPath(polyPath(pts, bulges, true));
            } else {
                // Kanten: 93 Anzahl, je 72 Typ (1 Linie, 2 Bogen)
                while (i < e.size() && e[i].first != 93)
                    ++i;
                const int edges = at(i).second.toInt();
                ++i;
                QPainterPath loop;
                for (int k = 0; k < edges && i < e.size(); ++k) {
                    while (i < e.size() && e[i].first != 72)
                        ++i;
                    const int edgeType = at(i).second.toInt();
                    ++i;
                    auto read = [&](int code) {
                        while (i < e.size() && e[i].first != code)
                            ++i;
                        return i < e.size() ? e[i++].second.toDouble() : 0.0;
                    };
                    // Reihenfolge fest: x vor y (Argumente eines Konstruktors hätten keine)
                    auto point = [&](int code) {
                        const double x = read(code);
                        const double y = read(code + 10);
                        return QPointF(x, y);
                    };
                    if (edgeType == 1) {
                        const QPointF a = point(10), b = point(11);
                        loop.elementCount() == 0 ? loop.moveTo(a) : loop.lineTo(a);
                        loop.lineTo(b);
                    } else if (edgeType == 2) {
                        const QPointF c = point(10);
                        const double r = read(40);
                        double a0 = qDegreesToRadians(read(50)), a1 = qDegreesToRadians(read(51));
                        const bool ccw = read(73) != 0;
                        if (!ccw) {
                            a0 = -a0;
                            a1 = -a1;
                        }
                        if (a1 <= a0)
                            a1 += 2 * std::numbers::pi;
                        const int steps = std::max(4, int((a1 - a0) / (std::numbers::pi / 32)));
                        for (int s = 0; s <= steps; ++s) {
                            const double a = a0 + (a1 - a0) * s / steps;
                            const QPointF pt = c + QPointF(std::cos(a), (ccw ? 1 : -1) * std::sin(a)) * r;
                            loop.elementCount() == 0 ? loop.moveTo(pt) : loop.lineTo(pt);
                        }
                    } else if (edgeType == 3) {
                        const QPointF c = point(10), major = point(11);
                        const double ratio = read(40);
                        double a0 = qDegreesToRadians(read(50)), a1 = qDegreesToRadians(read(51));
                        const bool ccw = read(73) != 0;
                        if (!ccw) {
                            a0 = -a0;
                            a1 = -a1;
                        }
                        if (a1 <= a0)
                            a1 += 2 * std::numbers::pi;
                        const QPointF minor = QPointF(-major.y(), major.x()) * ratio;
                        const int steps = std::max(8, int((a1 - a0) / (std::numbers::pi / 32)));
                        for (int s = 0; s <= steps; ++s) {
                            const double a = a0 + (a1 - a0) * s / steps;
                            const QPointF pt = c + major * std::cos(a) + minor * (ccw ? 1 : -1) * std::sin(a);
                            loop.elementCount() == 0 ? loop.moveTo(pt) : loop.lineTo(pt);
                        }
                    } else if (edgeType == 4) {
                        // 94 Grad, 73 rational, 74 periodisch, 95 Knoten, 96 Kontrollpunkte (+42 Gewichte)
                        const int degree = int(read(94));
                        const bool rational = read(73) != 0;
                        read(74);
                        const int knotCount = int(read(95));
                        const int ctrlCount = int(read(96));
                        QList<double> knots, weights;
                        QList<QPointF> ctrl;
                        for (int k = 0; k < knotCount; ++k)
                            knots << read(40);
                        for (int k = 0; k < ctrlCount; ++k) {
                            const double x = read(10), y = read(20);
                            ctrl << QPointF(x, y);
                            if (rational && at(i).first == 42)
                                weights << e[i++].second.toDouble();
                        }
                        // ab DXF 2010: Stützpunkte (97) samt Tangenten
                        if (at(i).first == 97) {
                            const int fit = at(i).second.toInt();
                            ++i;
                            for (int k = 0; k < fit; ++k) {
                                read(11);
                                read(21);
                            }
                            if (fit > 0) {
                                for (int code : {12, 22, 13, 23})
                                    if (at(i).first == code)
                                        ++i;
                            }
                        }
                        for (const QPointF& pt : nurbs(degree, ctrl, knots, weights))
                            loop.elementCount() == 0 ? loop.moveTo(pt) : loop.lineTo(pt);
                    } else {
                        break;
                    }
                }
                loop.closeSubpath();
                path.addPath(loop);
            }
        }
        return path;
    }

    QList<Entity> m_entities;
    QHash<QString, QList<Entity>> m_blocks;
    QHash<QString, QPointF> m_blockBase;
    QHash<QString, Layer> m_layers;
    QHash<QString, QString> m_imageFiles; // IMAGEDEF-Handle -> Datei
    QList<Shape> m_shapes;
    int m_units = 0;
};

} // namespace

svg::Import read(const QByteArray& data, QString* error, const QString& baseDir)
{
    Reader r;
    r.m_baseDir = baseDir;
    if (!r.parse(data, error))
        return {};
    svg::Import result = r.build();
    if ((!result.group || result.group->childCount() == 0) && error && error->isEmpty())
        *error = QObject::tr("Die DXF-Datei enthält keine darstellbaren Objekte.");
    return result;
}

} // namespace vek::dxf

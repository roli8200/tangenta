#include "SvgImport.h"

#include "CurveObject.h"
#include "ImageObject.h"
#include "ImagePool.h"
#include "Serialization.h"
#include "TextObject.h"

#include <QColor>
#include <QHash>
#include <QRegularExpression>
#include <QStringList>
#include <QTransform>
#include <QXmlStreamReader>
#include <QtMath>

#include <cmath>
#include <numbers>

namespace vek::svg {

namespace {

constexpr double kPxToMm = 25.4 / 96.0;

struct Element {
    QString name;
    QHash<QString, QString> attrs;
    QList<std::shared_ptr<Element>> children;
    QString text;
};
using ElementPtr = std::shared_ptr<Element>;

ElementPtr parseTree(const QByteArray& data, QString* error)
{
    QXmlStreamReader xml(data);
    ElementPtr root;
    QList<ElementPtr> stack;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            auto e = std::make_shared<Element>();
            e->name = xml.name().toString();
            for (const QXmlStreamAttribute& a : xml.attributes()) {
                const QString key = a.prefix().isEmpty() ? a.name().toString() : a.prefix().toString() + QLatin1Char(':') + a.name().toString();
                e->attrs.insert(key, a.value().toString());
                if (a.name() == QLatin1String("href"))
                    e->attrs.insert(QStringLiteral("href"), a.value().toString());
            }
            if (!stack.isEmpty())
                stack.last()->children << e;
            else
                root = e;
            stack << e;
        } else if (xml.isEndElement()) {
            if (!stack.isEmpty())
                stack.removeLast();
        } else if (xml.isCharacters() && !stack.isEmpty()) {
            stack.last()->text += xml.text().toString();
        }
    }
    if (xml.hasError() && error)
        *error = xml.errorString();
    return root;
}

// Zahlen aus Listen wie "10,20 30 -4e2"
QList<double> numbers(const QString& s)
{
    static const QRegularExpression re(QStringLiteral(R"([-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)"));
    QList<double> out;
    for (auto it = re.globalMatch(s); it.hasNext();)
        out << it.next().captured(0).toDouble();
    return out;
}

// Länge mit Einheit in Benutzereinheiten (px); ref für Prozent
double length(const QString& s, double ref = 0.0, double fallback = 0.0)
{
    static const QRegularExpression re(QStringLiteral(R"(^\s*([-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)\s*([a-z%]*)\s*$)"));
    const auto m = re.match(s);
    if (!m.hasMatch())
        return fallback;
    const double v = m.captured(1).toDouble();
    const QString unit = m.captured(2);
    if (unit == QLatin1String("mm"))
        return v / kPxToMm;
    if (unit == QLatin1String("cm"))
        return v * 10.0 / kPxToMm;
    if (unit == QLatin1String("in"))
        return v * 96.0;
    if (unit == QLatin1String("pt"))
        return v * 96.0 / 72.0;
    if (unit == QLatin1String("pc"))
        return v * 16.0;
    if (unit == QLatin1String("%"))
        return v / 100.0 * ref;
    if (unit == QLatin1String("em"))
        return v * 16.0;
    return v;
}

QTransform parseTransform(const QString& s)
{
    QTransform t;
    static const QRegularExpression re(QStringLiteral(R"((matrix|translate|scale|rotate|skewX|skewY)\s*\(([^)]*)\))"));
    for (auto it = re.globalMatch(s); it.hasNext();) {
        const auto m = it.next();
        const QString op = m.captured(1);
        const QList<double> a = numbers(m.captured(2));
        QTransform step;
        if (op == QLatin1String("matrix") && a.size() >= 6)
            step = QTransform(a[0], a[1], a[2], a[3], a[4], a[5]);
        else if (op == QLatin1String("translate") && !a.isEmpty())
            step = QTransform::fromTranslate(a[0], a.value(1, 0.0));
        else if (op == QLatin1String("scale") && !a.isEmpty())
            step = QTransform::fromScale(a[0], a.value(1, a[0]));
        else if (op == QLatin1String("rotate") && !a.isEmpty()) {
            const double cx = a.value(1, 0.0), cy = a.value(2, 0.0);
            step = QTransform::fromTranslate(-cx, -cy) * QTransform().rotate(a[0]) * QTransform::fromTranslate(cx, cy);
        } else if (op == QLatin1String("skewX") && !a.isEmpty())
            step = QTransform(1, 0, std::tan(qDegreesToRadians(a[0])), 1, 0, 0);
        else if (op == QLatin1String("skewY") && !a.isEmpty())
            step = QTransform(1, std::tan(qDegreesToRadians(a[0])), 0, 1, 0, 0);
        // SVG: die zuerst genannte Transformation wirkt zuletzt
        t = step * t;
    }
    return t;
}

// Elliptischer Bogen (Endpunktform) als kubische Kurven
void arcTo(QPainterPath& path, QPointF from, double rx, double ry, double phiDeg, bool large, bool sweep, QPointF to)
{
    if (rx == 0 || ry == 0 || from == to) {
        path.lineTo(to);
        return;
    }
    rx = std::abs(rx);
    ry = std::abs(ry);
    const double phi = qDegreesToRadians(phiDeg);
    const double cosP = std::cos(phi), sinP = std::sin(phi);
    const double dx = (from.x() - to.x()) / 2.0, dy = (from.y() - to.y()) / 2.0;
    const double x1 = cosP * dx + sinP * dy, y1 = -sinP * dx + cosP * dy;
    double lambda = (x1 * x1) / (rx * rx) + (y1 * y1) / (ry * ry);
    if (lambda > 1) {
        rx *= std::sqrt(lambda);
        ry *= std::sqrt(lambda);
    }
    const double num = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1;
    const double den = rx * rx * y1 * y1 + ry * ry * x1 * x1;
    double coef = den > 0 ? std::sqrt(std::max(0.0, num / den)) : 0.0;
    if (large == sweep)
        coef = -coef;
    const double cx1 = coef * rx * y1 / ry, cy1 = -coef * ry * x1 / rx;
    const double cx = cosP * cx1 - sinP * cy1 + (from.x() + to.x()) / 2.0;
    const double cy = sinP * cx1 + cosP * cy1 + (from.y() + to.y()) / 2.0;
    auto angle = [](double ux, double uy, double vx, double vy) {
        const double a = std::atan2(ux * vy - uy * vx, ux * vx + uy * vy);
        return a;
    };
    const double theta = angle(1, 0, (x1 - cx1) / rx, (y1 - cy1) / ry);
    double delta = angle((x1 - cx1) / rx, (y1 - cy1) / ry, (-x1 - cx1) / rx, (-y1 - cy1) / ry);
    if (!sweep && delta > 0)
        delta -= 2 * std::numbers::pi;
    else if (sweep && delta < 0)
        delta += 2 * std::numbers::pi;
    const int segments = std::max(1, int(std::ceil(std::abs(delta) / (std::numbers::pi / 2) - 1e-9)));
    const double step = delta / segments;
    const double k = 4.0 / 3.0 * std::tan(step / 4.0);
    auto point = [&](double t) {
        const double x = rx * std::cos(t), y = ry * std::sin(t);
        return QPointF(cosP * x - sinP * y + cx, sinP * x + cosP * y + cy);
    };
    auto deriv = [&](double t) {
        const double x = -rx * std::sin(t), y = ry * std::cos(t);
        return QPointF(cosP * x - sinP * y, sinP * x + cosP * y);
    };
    double t = theta;
    for (int i = 0; i < segments; ++i) {
        const double t2 = t + step;
        const QPointF p1 = point(t), p2 = point(t2);
        path.cubicTo(p1 + deriv(t) * k, p2 - deriv(t2) * k, i + 1 == segments ? to : p2);
        t = t2;
    }
}

struct Style {
    QString fill = QStringLiteral("black");
    QString stroke = QStringLiteral("none");
    double strokeWidth = 1.0;
    double fillOpacity = 1.0;
    double strokeOpacity = 1.0;
    QString fillRule = QStringLiteral("nonzero");
    QString linecap = QStringLiteral("butt");
    QString linejoin = QStringLiteral("miter");
    QString dash = QStringLiteral("none");
    QString color = QStringLiteral("black");
    double fontSize = 16.0;
    QString fontFamily = QStringLiteral("Arial");
    bool bold = false;
    bool italic = false;
    bool hidden = false;
    QString textAnchor = QStringLiteral("start");
};

struct Gradient {
    bool radial = false;
    bool userSpace = false;
    QList<GradientStop> stops;
    QHash<QString, QString> attrs;
    QTransform transform;
};

class Importer {
public:
    explicit Importer(const ElementPtr& root) : m_root(root) {}

    Import run()
    {
        Import result;
        collect(m_root);
        // Zeichenfläche: width/height in mm, viewBox in Benutzereinheiten
        const QList<double> vb = numbers(m_root->attrs.value(QStringLiteral("viewBox")));
        const double wpx = length(m_root->attrs.value(QStringLiteral("width")), 0, vb.size() == 4 ? vb[2] : 793.7);
        const double hpx = length(m_root->attrs.value(QStringLiteral("height")), 0, vb.size() == 4 ? vb[3] : 1122.5);
        result.size = QSizeF(wpx * kPxToMm, hpx * kPxToMm);
        QTransform root = QTransform::fromScale(kPxToMm, kPxToMm);
        if (vb.size() == 4 && vb[2] > 0 && vb[3] > 0) {
            // preserveAspectRatio xMidYMid meet
            const double s = std::min(wpx / vb[2], hpx / vb[3]);
            const double ox = (wpx - vb[2] * s) / 2.0, oy = (hpx - vb[3] * s) / 2.0;
            root = QTransform::fromTranslate(-vb[0], -vb[1]) * QTransform::fromScale(s, s) * QTransform::fromTranslate(ox, oy) * root;
            m_viewport = QSizeF(vb[2], vb[3]);
        } else {
            m_viewport = QSizeF(wpx, hpx);
        }
        result.group = std::make_unique<GroupObject>();
        Style base;
        for (const ElementPtr& child : m_root->children)
            convert(*child, root, base, *result.group);
        return result;
    }

private:
    void collect(const ElementPtr& e)
    {
        if (const QString id = e->attrs.value(QStringLiteral("id")); !id.isEmpty())
            m_ids.insert(id, e);
        if (e->name == QLatin1String("style"))
            parseCss(e->text);
        for (const ElementPtr& c : e->children)
            collect(c);
    }

    void parseCss(const QString& css)
    {
        static const QRegularExpression rule(QStringLiteral(R"(([^{}]+)\{([^}]*)\})"));
        for (auto it = rule.globalMatch(css); it.hasNext();) {
            const auto m = it.next();
            for (QString sel : m.captured(1).split(QLatin1Char(','))) {
                sel = sel.trimmed();
                m_css[sel] += m.captured(2) + QLatin1Char(';');
            }
        }
    }

    // Eigenschaften eines Elements: Attribute, dann Klassen (CSS), dann style=""
    QHash<QString, QString> properties(const Element& e) const
    {
        QHash<QString, QString> p = e.attrs;
        auto applyDecl = [&p](const QString& decl) {
            for (const QString& part : decl.split(QLatin1Char(';'))) {
                const int colon = int(part.indexOf(QLatin1Char(':')));
                if (colon > 0)
                    p.insert(part.left(colon).trimmed(), part.mid(colon + 1).trimmed());
            }
        };
        applyDecl(m_css.value(e.name));
        for (const QString& cls : e.attrs.value(QStringLiteral("class")).split(QLatin1Char(' '), Qt::SkipEmptyParts))
            applyDecl(m_css.value(QLatin1Char('.') + cls));
        if (const QString id = e.attrs.value(QStringLiteral("id")); !id.isEmpty())
            applyDecl(m_css.value(QLatin1Char('#') + id));
        applyDecl(e.attrs.value(QStringLiteral("style")));
        return p;
    }

    Style inherit(const Style& parent, const QHash<QString, QString>& p) const
    {
        Style s = parent;
        auto take = [&p](const char* key, QString& target) {
            const QString v = p.value(QLatin1String(key));
            if (!v.isEmpty() && v != QLatin1String("inherit"))
                target = v;
        };
        take("fill", s.fill);
        take("stroke", s.stroke);
        take("fill-rule", s.fillRule);
        take("stroke-linecap", s.linecap);
        take("stroke-linejoin", s.linejoin);
        take("stroke-dasharray", s.dash);
        take("color", s.color);
        take("font-family", s.fontFamily);
        take("text-anchor", s.textAnchor);
        s.fontFamily.remove(QLatin1Char('\'')).remove(QLatin1Char('"'));
        s.fontFamily = s.fontFamily.split(QLatin1Char(',')).value(0).trimmed();
        if (p.contains(QStringLiteral("stroke-width")))
            s.strokeWidth = length(p.value(QStringLiteral("stroke-width")), m_viewport.width(), 1.0);
        if (p.contains(QStringLiteral("fill-opacity")))
            s.fillOpacity = p.value(QStringLiteral("fill-opacity")).toDouble();
        if (p.contains(QStringLiteral("stroke-opacity")))
            s.strokeOpacity = p.value(QStringLiteral("stroke-opacity")).toDouble();
        if (p.contains(QStringLiteral("font-size")))
            s.fontSize = length(p.value(QStringLiteral("font-size")), s.fontSize, s.fontSize);
        if (const QString w = p.value(QStringLiteral("font-weight")); !w.isEmpty())
            s.bold = w == QLatin1String("bold") || w.toInt() >= 600;
        if (const QString st = p.value(QStringLiteral("font-style")); !st.isEmpty())
            s.italic = st == QLatin1String("italic") || st == QLatin1String("oblique");
        s.hidden = p.value(QStringLiteral("display")) == QLatin1String("none") || p.value(QStringLiteral("visibility")) == QLatin1String("hidden");
        return s;
    }

    std::optional<Color> color(QString spec, const Style& s, double opacity) const
    {
        spec = spec.trimmed();
        if (spec.isEmpty() || spec == QLatin1String("none") || spec == QLatin1String("transparent"))
            return std::nullopt;
        if (spec == QLatin1String("currentColor"))
            spec = s.color;
        QColor c;
        static const QRegularExpression rgb(QStringLiteral(R"(rgba?\(([^)]*)\))"));
        if (const auto m = rgb.match(spec); m.hasMatch()) {
            const QStringList parts = m.captured(1).split(QLatin1Char(','));
            auto channel = [&](int i) {
                const QString v = parts.value(i).trimmed();
                return v.endsWith(QLatin1Char('%')) ? int(std::lround(v.chopped(1).toDouble() * 2.55)) : v.toInt();
            };
            c = QColor(channel(0), channel(1), channel(2));
            if (parts.size() > 3)
                opacity *= parts[3].toDouble();
        } else {
            c = QColor::fromString(spec);
        }
        if (!c.isValid())
            return std::nullopt;
        return Color(std::uint8_t(c.red()), std::uint8_t(c.green()), std::uint8_t(c.blue()), std::uint8_t(std::clamp(opacity, 0.0, 1.0) * 255 + 0.5));
    }

    // Verlauf zu url(#id) für ein Objekt mit Rechteck bbox (mm) und Abbildung ctm
    std::optional<Fill> gradientFill(const QString& spec, const QRectF& bbox, const QTransform& ctm, double opacity) const
    {
        static const QRegularExpression urlRe(QStringLiteral(R"(url\(\s*['"]?#([^)'"]+)['"]?\s*\))"));
        const auto m = urlRe.match(spec);
        if (!m.hasMatch())
            return std::nullopt;
        // Verweiskette (href) auflösen: Stopps und Attribute erben
        Gradient g;
        ElementPtr e = m_ids.value(m.captured(1));
        if (!e || (e->name != QLatin1String("linearGradient") && e->name != QLatin1String("radialGradient")))
            return std::nullopt;
        g.radial = e->name == QLatin1String("radialGradient");
        for (int depth = 0; e && depth < 8; ++depth) {
            for (auto it = e->attrs.constBegin(); it != e->attrs.constEnd(); ++it)
                if (!g.attrs.contains(it.key()))
                    g.attrs.insert(it.key(), it.value());
            if (g.stops.isEmpty())
                for (const ElementPtr& stop : e->children) {
                    if (stop->name != QLatin1String("stop"))
                        continue;
                    const QHash<QString, QString> p = properties(*stop);
                    QString off = p.value(QStringLiteral("offset"), QStringLiteral("0"));
                    double o = off.endsWith(QLatin1Char('%')) ? off.chopped(1).toDouble() / 100.0 : off.toDouble();
                    const double so = p.value(QStringLiteral("stop-opacity"), QStringLiteral("1")).toDouble();
                    if (const std::optional<Color> c = color(p.value(QStringLiteral("stop-color"), QStringLiteral("black")), Style(), so * opacity))
                        g.stops << GradientStop{std::clamp(o, 0.0, 1.0), *c};
                }
            const QString href = e->attrs.value(QStringLiteral("href"));
            e = href.startsWith(QLatin1Char('#')) ? m_ids.value(href.mid(1)) : nullptr;
        }
        if (g.stops.isEmpty())
            return std::nullopt;
        g.userSpace = g.attrs.value(QStringLiteral("gradientUnits")) == QLatin1String("userSpaceOnUse");
        g.transform = parseTransform(g.attrs.value(QStringLiteral("gradientTransform")));
        auto value = [&](const char* key, double def, double ref) {
            const QString v = g.attrs.value(QLatin1String(key));
            if (v.isEmpty())
                return def;
            if (!g.userSpace)
                return v.endsWith(QLatin1Char('%')) ? v.chopped(1).toDouble() / 100.0 : v.toDouble();
            return length(v, ref, def);
        };
        QPointF a, b;
        if (g.radial) {
            const double cx = value("cx", g.userSpace ? 0 : 0.5, m_viewport.width()), cy = value("cy", g.userSpace ? 0 : 0.5, m_viewport.height());
            const double r = value("r", g.userSpace ? 0 : 0.5, m_viewport.width());
            a = QPointF(cx, cy);
            b = QPointF(cx + r, cy);
        } else {
            a = QPointF(value("x1", 0, m_viewport.width()), value("y1", 0, m_viewport.height()));
            b = QPointF(value("x2", g.userSpace ? 0 : 1, m_viewport.width()), value("y2", 0, m_viewport.height()));
        }
        a = g.transform.map(a);
        b = g.transform.map(b);
        if (g.userSpace && bbox.width() > 0 && bbox.height() > 0) {
            // Benutzereinheiten -> Anteile am Rechteck der Form
            auto frac = [&](const QPointF& p) {
                const QPointF mm = ctm.map(p);
                return QPointF((mm.x() - bbox.left()) / bbox.width(), (mm.y() - bbox.top()) / bbox.height());
            };
            a = frac(a);
            b = frac(b);
        }
        Fill f;
        f.type = g.radial ? Fill::Type::Radial : Fill::Type::Linear;
        f.stops = g.stops;
        f.start = a;
        f.end = b;
        f.sortStops();
        return f;
    }

    void applyStyle(DrawObject& o, const Style& s, const QTransform& ctm, const QRectF& bbox, bool closedShape) const
    {
        vek::Style st;
        if (closedShape || s.fill != QLatin1String("none")) {
            if (const std::optional<Fill> g = gradientFill(s.fill, bbox, ctm, s.fillOpacity))
                st.fill = *g;
            else if (const std::optional<Color> c = color(s.fill, s, s.fillOpacity))
                st.fill = Fill::solid(*c);
        }
        if (const std::optional<Color> c = color(s.stroke, s, s.strokeOpacity)) {
            st.stroke = *c;
            st.strokeWidth = std::max(0.0, s.strokeWidth * std::sqrt(std::abs(ctm.determinant())));
        } else {
            st.stroke.reset();
        }
        st.cap = s.linecap == QLatin1String("round") ? vek::Style::Cap::Round : s.linecap == QLatin1String("square") ? vek::Style::Cap::Square : vek::Style::Cap::Flat;
        st.join = s.linejoin == QLatin1String("round") ? vek::Style::Join::Round : s.linejoin == QLatin1String("bevel") ? vek::Style::Join::Bevel : vek::Style::Join::Miter;
        if (s.dash != QLatin1String("none") && !numbers(s.dash).isEmpty())
            st.dash = 1;
        o.setStyle(st);
    }

    void addOpacity(DrawObject& o, const QHash<QString, QString>& p) const
    {
        if (p.contains(QStringLiteral("opacity"))) {
            const double op = std::clamp(p.value(QStringLiteral("opacity")).toDouble(), 0.0, 1.0);
            if (op < 1.0)
                o.setEffect(Effect::Transparency, Ramp::uniform((1.0 - op) * 100.0));
        }
    }

    void addShape(const QPainterPath& userPath, const QTransform& ctm, const Style& s, const QHash<QString, QString>& p, DrawObject& parent,
                  bool closedShape)
    {
        if (userPath.isEmpty())
            return;
        QPainterPath mm = ctm.map(userPath);
        mm.setFillRule(s.fillRule == QLatin1String("evenodd") ? Qt::OddEvenFill : Qt::WindingFill);
        std::unique_ptr<CurveObject> c = CurveObject::fromPath(mm);
        if (c->subPaths().isEmpty())
            return;
        applyStyle(*c, s, ctm, mm.boundingRect(), closedShape);
        addOpacity(*c, p);
        if (const QString id = p.value(QStringLiteral("id")); !id.isEmpty())
            c->setName(id);
        attachDetachedChild(parent, std::move(c));
    }

    void convert(const Element& e, const QTransform& parentCtm, const Style& parentStyle, DrawObject& parent, int depth = 0)
    {
        if (depth > 64)
            return;
        const QString& n = e.name;
        if (n == QLatin1String("defs") || n == QLatin1String("style") || n == QLatin1String("title") || n == QLatin1String("desc")
            || n == QLatin1String("metadata") || n == QLatin1String("linearGradient") || n == QLatin1String("radialGradient")
            || n == QLatin1String("clipPath") || n == QLatin1String("mask") || n == QLatin1String("symbol") || n == QLatin1String("pattern")
            || n == QLatin1String("marker") || n == QLatin1String("filter"))
            return;
        const QHash<QString, QString> p = properties(e);
        const Style s = inherit(parentStyle, p);
        if (s.hidden)
            return;
        QTransform ctm = parseTransform(p.value(QStringLiteral("transform"))) * parentCtm;
        auto num = [&](const char* key, double ref = 0.0) { return length(p.value(QLatin1String(key)), ref, 0.0); };
        const double vw = m_viewport.width(), vh = m_viewport.height();
        if (n == QLatin1String("g") || n == QLatin1String("svg") || n == QLatin1String("a") || n == QLatin1String("switch")) {
            if (n == QLatin1String("svg") && depth > 0)
                ctm = QTransform::fromTranslate(num("x", vw), num("y", vh)) * ctm;
            auto group = std::make_unique<GroupObject>();
            for (const ElementPtr& c : e.children)
                convert(*c, ctm, s, *group, depth + 1);
            if (group->childCount() == 0)
                return;
            addOpacity(*group, p);
            if (const QString id = p.value(QStringLiteral("id")); !id.isEmpty())
                group->setName(id);
            // Gruppen mit nur einem Objekt auflösen (wie "Gruppierung aufheben")
            if (group->childCount() == 1 && !p.contains(QStringLiteral("opacity"))) {
                std::unique_ptr<Node> only = serial::fromJson(serial::toJson(*group->child(0), serial::Ids::Drop), serial::Ids::Drop);
                if (only)
                    attachDetachedChild(parent, std::move(only));
            } else {
                attachDetachedChild(parent, std::move(group));
            }
            return;
        }
        if (n == QLatin1String("use")) {
            const QString href = p.value(QStringLiteral("href"));
            const ElementPtr target = href.startsWith(QLatin1Char('#')) ? m_ids.value(href.mid(1)) : nullptr;
            if (!target)
                return;
            const QTransform at = QTransform::fromTranslate(num("x", vw), num("y", vh)) * ctm;
            if (target->name == QLatin1String("symbol")) {
                Element g = *target;
                g.name = QStringLiteral("g");
                convert(g, at, s, parent, depth + 1);
            } else {
                convert(*target, at, s, parent, depth + 1);
            }
            return;
        }
        QPainterPath path;
        bool closed = true;
        if (n == QLatin1String("path")) {
            path = parsePathData(p.value(QStringLiteral("d")));
            closed = false; // offene Teilpfade werden trotzdem gefüllt, wie in SVG
            closed = true;
        } else if (n == QLatin1String("rect")) {
            const double w = num("width", vw), h = num("height", vh);
            double rx = num("rx", vw), ry = num("ry", vh);
            if (!p.contains(QStringLiteral("rx")))
                rx = ry;
            if (!p.contains(QStringLiteral("ry")))
                ry = rx;
            if (w <= 0 || h <= 0)
                return;
            if (rx > 0 || ry > 0)
                path.addRoundedRect(QRectF(num("x", vw), num("y", vh), w, h), std::min(rx, w / 2), std::min(ry, h / 2));
            else
                path.addRect(QRectF(num("x", vw), num("y", vh), w, h));
        } else if (n == QLatin1String("circle")) {
            const double r = num("r", std::hypot(vw, vh) / std::sqrt(2.0));
            if (r <= 0)
                return;
            path.addEllipse(QPointF(num("cx", vw), num("cy", vh)), r, r);
        } else if (n == QLatin1String("ellipse")) {
            path.addEllipse(QPointF(num("cx", vw), num("cy", vh)), num("rx", vw), num("ry", vh));
        } else if (n == QLatin1String("line")) {
            path.moveTo(num("x1", vw), num("y1", vh));
            path.lineTo(num("x2", vw), num("y2", vh));
            closed = false;
        } else if (n == QLatin1String("polyline") || n == QLatin1String("polygon")) {
            const QList<double> v = numbers(p.value(QStringLiteral("points")));
            for (int i = 0; i + 1 < v.size(); i += 2)
                i == 0 ? path.moveTo(v[i], v[i + 1]) : path.lineTo(v[i], v[i + 1]);
            if (n == QLatin1String("polygon"))
                path.closeSubpath();
            else
                closed = false;
        } else if (n == QLatin1String("text")) {
            addText(e, ctm, s, p, parent);
            return;
        } else if (n == QLatin1String("image")) {
            addImage(p, ctm, parent);
            return;
        } else {
            return;
        }
        addShape(path, ctm, s, p, parent, closed);
    }

    void addText(const Element& e, const QTransform& ctm, const Style& s, const QHash<QString, QString>& p, DrawObject& parent)
    {
        QString content = e.text;
        double x = length(p.value(QStringLiteral("x")).split(QRegularExpression(QStringLiteral("[ ,]+"))).value(0), m_viewport.width());
        double y = length(p.value(QStringLiteral("y")).split(QRegularExpression(QStringLiteral("[ ,]+"))).value(0), m_viewport.height());
        Style ts = s;
        for (const ElementPtr& span : e.children) {
            if (span->name != QLatin1String("tspan"))
                continue;
            const QHash<QString, QString> sp = properties(*span);
            if (content.trimmed().isEmpty()) {
                if (sp.contains(QStringLiteral("x")))
                    x = length(sp.value(QStringLiteral("x")).split(QLatin1Char(' ')).value(0), m_viewport.width());
                if (sp.contains(QStringLiteral("y")))
                    y = length(sp.value(QStringLiteral("y")).split(QLatin1Char(' ')).value(0), m_viewport.height());
                ts = inherit(s, sp);
            }
            content += span->text;
        }
        content = content.simplified();
        if (content.isEmpty())
            return;
        const double scale = std::sqrt(std::abs(ctm.determinant()));
        const double sizeMm = ts.fontSize * scale;
        auto text = std::make_unique<TextObject>(TextObject::Kind::Artistic);
        text->setPlainText(content, CharFormat(ts.fontFamily, sizeMm * 72.0 / 25.4, ts.bold, ts.italic));
        // Grundlinie bei y: der Textblock beginnt etwa eine Oberlänge darüber
        const QRectF frame = text->frameRect();
        double left = x;
        if (ts.textAnchor == QLatin1String("middle"))
            left -= frame.width() / scale / 2.0;
        else if (ts.textAnchor == QLatin1String("end"))
            left -= frame.width() / scale;
        const QPointF at = ctm.map(QPointF(left, y));
        // Drehung/Neigung der Abbildung übernehmen, Grösse steckt schon in der Schrift
        const QTransform linear(ctm.m11() / scale, ctm.m12() / scale, ctm.m21() / scale, ctm.m22() / scale, 0, 0);
        text->setTransform(QTransform::fromTranslate(0, -sizeMm * 0.8) * linear * QTransform::fromTranslate(at.x(), at.y()));
        applyStyle(*text, ts, ctm, QRectF(), true);
        vek::Style st = text->style();
        st.stroke.reset();
        text->setStyle(st);
        addOpacity(*text, p);
        attachDetachedChild(parent, std::move(text));
    }

    void addImage(const QHash<QString, QString>& p, const QTransform& ctm, DrawObject& parent)
    {
        const QString href = p.value(QStringLiteral("href"));
        const int comma = int(href.indexOf(QLatin1Char(',')));
        if (!href.startsWith(QLatin1String("data:")) || comma < 0)
            return; // verknüpfte Dateien kennen wir hier nicht
        const QByteArray bytes = href.mid(5, comma - 5).contains(QLatin1String("base64"))
                                     ? QByteArray::fromBase64(href.mid(comma + 1).toLatin1())
                                     : QByteArray::fromPercentEncoding(href.mid(comma + 1).toUtf8());
        const QString key = imagepool::add(bytes);
        if (key.isEmpty())
            return;
        const double w = length(p.value(QStringLiteral("width")), m_viewport.width());
        const double h = length(p.value(QStringLiteral("height")), m_viewport.height());
        const QImage img = imagepool::image(key);
        const QSizeF userSize = w > 0 && h > 0 ? QSizeF(w, h) : QSizeF(img.width(), img.height());
        auto image = std::make_unique<ImageObject>(key, userSize);
        image->setTransform(QTransform::fromTranslate(length(p.value(QStringLiteral("x"))), length(p.value(QStringLiteral("y")))) * ctm);
        attachDetachedChild(parent, std::move(image));
    }

    ElementPtr m_root;
    QHash<QString, ElementPtr> m_ids;
    QHash<QString, QString> m_css;
    QSizeF m_viewport;
};

} // namespace

QPainterPath parsePathData(const QString& d)
{
    QPainterPath path;
    static const QRegularExpression token(QStringLiteral(R"(([MmLlHhVvCcSsQqTtAaZz])|([-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?))"));
    QList<QString> tokens;
    for (auto it = token.globalMatch(d); it.hasNext();)
        tokens << it.next().captured(0);
    int i = 0;
    QChar cmd;
    QPointF cur, start, lastCtrl;
    QChar lastCmd;
    auto isNumber = [&](int k) { return k < tokens.size() && !tokens[k].front().isLetter(); };
    auto next = [&]() { return i < tokens.size() ? tokens[i++].toDouble() : 0.0; };
    // Bogen-Flags dürfen ohne Trenner stehen ("a5 5 0 016 6"): Ziffer für Ziffer lesen
    auto flag = [&]() {
        if (i >= tokens.size())
            return false;
        QString& t = tokens[i];
        const bool f = t.startsWith(QLatin1Char('1'));
        if (t.size() > 1 && (t.startsWith(QLatin1Char('0')) || t.startsWith(QLatin1Char('1'))) && !t.startsWith(QLatin1String("0.")))
            t = t.mid(1);
        else
            ++i;
        return f;
    };
    while (i < tokens.size()) {
        if (!isNumber(i))
            cmd = tokens[i++].front();
        else if (cmd.isNull())
            break;
        const bool rel = cmd.isLower();
        const QPointF base = rel ? cur : QPointF();
        switch (cmd.toUpper().unicode()) {
        case 'M': {
            const double x = next(), y = next();
            cur = base + QPointF(x, y);
            path.moveTo(cur);
            start = cur;
            cmd = rel ? QChar('l') : QChar('L'); // weitere Paare sind Linien
            break;
        }
        case 'L': {
            const double x = next(), y = next();
            cur = base + QPointF(x, y);
            path.lineTo(cur);
            break;
        }
        case 'H': cur.setX((rel ? cur.x() : 0.0) + next()); path.lineTo(cur); break;
        case 'V': cur.setY((rel ? cur.y() : 0.0) + next()); path.lineTo(cur); break;
        case 'C': {
            const double x1 = next(), y1 = next(), x2 = next(), y2 = next(), x = next(), y = next();
            const QPointF c1 = base + QPointF(x1, y1), c2 = base + QPointF(x2, y2);
            cur = base + QPointF(x, y);
            path.cubicTo(c1, c2, cur);
            lastCtrl = c2;
            break;
        }
        case 'S': {
            const double x2 = next(), y2 = next(), x = next(), y = next();
            const QPointF c1 = (lastCmd.toUpper() == QLatin1Char('C') || lastCmd.toUpper() == QLatin1Char('S')) ? 2 * cur - lastCtrl : cur;
            const QPointF c2 = base + QPointF(x2, y2);
            cur = base + QPointF(x, y);
            path.cubicTo(c1, c2, cur);
            lastCtrl = c2;
            break;
        }
        case 'Q': {
            const double x1 = next(), y1 = next(), x = next(), y = next();
            const QPointF c = base + QPointF(x1, y1);
            cur = base + QPointF(x, y);
            path.quadTo(c, cur);
            lastCtrl = c;
            break;
        }
        case 'T': {
            const double x = next(), y = next();
            const QPointF c = (lastCmd.toUpper() == QLatin1Char('Q') || lastCmd.toUpper() == QLatin1Char('T')) ? 2 * cur - lastCtrl : cur;
            cur = base + QPointF(x, y);
            path.quadTo(c, cur);
            lastCtrl = c;
            break;
        }
        case 'A': {
            const double rx = next(), ry = next(), rot = next();
            const bool large = flag(), sweep = flag();
            const double x = next(), y = next();
            const QPointF to = base + QPointF(x, y);
            arcTo(path, cur, rx, ry, rot, large, sweep, to);
            cur = to;
            break;
        }
        case 'Z':
            path.closeSubpath();
            cur = start;
            break;
        default: ++i; break;
        }
        lastCmd = cmd;
        if (cmd.toUpper() == QLatin1Char('Z'))
            cmd = QChar();
    }
    return path;
}

Import read(const QByteArray& data, QString* error)
{
    const ElementPtr root = parseTree(data, error);
    if (!root || root->name != QLatin1String("svg")) {
        if (error && error->isEmpty())
            *error = QObject::tr("Keine SVG-Datei.");
        return {};
    }
    return Importer(root).run();
}

} // namespace vek::svg

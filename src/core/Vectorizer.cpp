#include "Vectorizer.h"

#include "vek_fit.h"

#include <QHash>
#include <QRectF>
#include <QObject>
#include <QPolygonF>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <random>
#include <vector>

namespace vek::trace {

namespace {

struct Rgb {
    float r = 0, g = 0, b = 0;
};

float distance2(const Rgb& a, const Rgb& b)
{
    // Grün zählt stärker, wie das Auge es sieht
    const float dr = a.r - b.r, dg = a.g - b.g, db = a.b - b.b;
    return 0.3f * dr * dr + 0.59f * dg * dg + 0.11f * db * db;
}

// 16 Bit je Kanal auf die Skala 0..255 (stufenlos)
Rgb toRgb(QRgba64 c)
{
    return Rgb{c.red() / 257.0f, c.green() / 257.0f, c.blue() / 257.0f};
}

// k-Means mit deterministischer k-Means++-Initialisierung auf einer Stichprobe
std::vector<Rgb> quantize(const std::vector<QRgba64>& pixels, const std::vector<bool>& opaque, int k)
{
    std::vector<Rgb> sample;
    const std::size_t step = std::max<std::size_t>(1, pixels.size() / 30000);
    for (std::size_t i = 0; i < pixels.size(); i += step) {
        if (opaque[i])
            sample.push_back(toRgb(pixels[i]));
    }
    if (sample.empty())
        return {};
    std::mt19937 rng(20261026);
    std::vector<Rgb> centers{sample[sample.size() / 2]};
    std::vector<float> dist(sample.size(), std::numeric_limits<float>::max());
    while (int(centers.size()) < k) {
        double total = 0.0;
        for (std::size_t i = 0; i < sample.size(); ++i) {
            dist[i] = std::min(dist[i], distance2(sample[i], centers.back()));
            total += dist[i];
        }
        if (total <= 0.0)
            break; // weniger verschiedene Farben als k
        std::uniform_real_distribution<double> pick(0.0, total);
        double target = pick(rng);
        std::size_t chosen = 0;
        for (; chosen + 1 < sample.size(); ++chosen) {
            target -= dist[chosen];
            if (target <= 0.0)
                break;
        }
        centers.push_back(sample[chosen]);
    }
    std::vector<int> assign(sample.size(), 0);
    for (int iteration = 0; iteration < 15; ++iteration) {
        bool changed = false;
        for (std::size_t i = 0; i < sample.size(); ++i) {
            int best = 0;
            float bestD = std::numeric_limits<float>::max();
            for (int c = 0; c < int(centers.size()); ++c) {
                const float d = distance2(sample[i], centers[std::size_t(c)]);
                if (d < bestD) {
                    bestD = d;
                    best = c;
                }
            }
            changed = changed || assign[i] != best;
            assign[i] = best;
        }
        std::vector<Rgb> sum(centers.size());
        std::vector<int> count(centers.size(), 0);
        for (std::size_t i = 0; i < sample.size(); ++i) {
            Rgb& s = sum[std::size_t(assign[i])];
            s.r += sample[i].r;
            s.g += sample[i].g;
            s.b += sample[i].b;
            ++count[std::size_t(assign[i])];
        }
        for (std::size_t c = 0; c < centers.size(); ++c) {
            if (count[c] > 0)
                centers[c] = Rgb{sum[c].r / count[c], sum[c].g / count[c], sum[c].b / count[c]};
        }
        if (!changed && iteration > 0)
            break;
    }
    return centers;
}

// Farbabstand (gewichtet, Quadrat), ab dem ein kleiner Fleck als eigene Form gilt: etwa 60 Stufen Helligkeit
constexpr float kKeepContrast = 3600.0f;

// Flecken (4-zusammenhängend) unter minArea dem farblich ähnlichsten Nachbarn zuschlagen.
// So bleiben dunkle Kantenpixel bei der Schrift statt im hellen Hintergrund zu verschwinden.
// Kontrastreiche Flecken (Schrift, feine Linien) bleiben schon ab minContrastArea erhalten.
bool removeSpecklesOnce(std::vector<int>& labels, int w, int h, int minArea, int minContrastArea, const std::vector<Rgb>& centers)
{
    bool changed = false;
    std::vector<int> component(labels.size(), -1);
    std::vector<int> stack;
    std::vector<int> members;
    int next = 0;
    for (int start = 0; start < w * h; ++start) {
        if (component[std::size_t(start)] >= 0)
            continue;
        const int label = labels[std::size_t(start)];
        members.clear();
        stack.assign(1, start);
        component[std::size_t(start)] = next;
        std::map<int, int> neighbours;
        while (!stack.empty()) {
            const int i = stack.back();
            stack.pop_back();
            members.push_back(i);
            const int x = i % w, y = i / w;
            const int around[4] = {x > 0 ? i - 1 : -1, x + 1 < w ? i + 1 : -1, y > 0 ? i - w : -1, y + 1 < h ? i + w : -1};
            for (int j : around) {
                if (j < 0)
                    continue;
                if (labels[std::size_t(j)] == label) {
                    if (component[std::size_t(j)] < 0) {
                        component[std::size_t(j)] = next;
                        stack.push_back(j);
                    }
                } else {
                    ++neighbours[labels[std::size_t(j)]];
                }
            }
        }
        ++next;
        if (int(members.size()) < minArea && !neighbours.empty()) {
            int target = neighbours.begin()->first;
            float best = std::numeric_limits<float>::max();
            for (const auto& [other, count] : neighbours) {
                // Transparent (-1) nur, wenn es nichts anderes gibt
                const float d = other < 0 || label < 0 ? 1e9f - count
                                                       : distance2(centers[std::size_t(label)], centers[std::size_t(other)]);
                if (d < best) {
                    best = d;
                    target = other;
                }
            }
            // Deutlich andere Farbe als die Umgebung: eigene Form (kleine Schrift), kein Rauschen
            if (label >= 0 && target >= 0 && int(members.size()) >= minContrastArea
                && distance2(centers[std::size_t(label)], centers[std::size_t(target)]) > kKeepContrast)
                continue;
            for (int i : members)
                labels[std::size_t(i)] = target;
            changed = true;
        }
    }
    return changed;
}

// Mehrere Durchgänge: ein Fleck kann in einen anderen kleinen Fleck übergehen, der erst im
// nächsten Durchgang als Ganzes erkannt wird
void removeSpeckles(std::vector<int>& labels, int w, int h, int minArea, int minContrastArea, const std::vector<Rgb>& centers)
{
    if (minArea <= 1)
        return;
    for (int pass = 0; pass < 6 && removeSpecklesOnce(labels, w, h, minArea, minContrastArea, centers); ++pass) {
    }
}

double signedArea(const std::vector<QPoint>& loop)
{
    double a = 0.0;
    for (std::size_t i = 0; i < loop.size(); ++i) {
        const QPoint& p = loop[i];
        const QPoint& q = loop[(i + 1) % loop.size()];
        a += double(p.x()) * q.y() - double(q.x()) * p.y();
    }
    return a / 2.0;
}

// Umrisse einer Maske entlang der Pixelkanten. Die Fläche liegt in Laufrichtung rechts
// (im Uhrzeigersinn um Flächen, gegen ihn um Löcher). An Sattelpunkten biegt der Weg rechts
// ab, so bleiben nur über Eck verbundene Pixel getrennt.
std::vector<std::vector<QPoint>> traceMask(const std::vector<char>& mask, int w, int h)
{
    auto inside = [&](int x, int y) { return x >= 0 && y >= 0 && x < w && y < h && mask[std::size_t(y * w + x)]; };
    const int vw = w + 1;
    // Je Ecke bis zu zwei ausgehende Kanten, Richtung 0=+x 1=+y 2=-x 3=-y
    std::vector<std::uint8_t> out(std::size_t(vw) * (h + 1), 0);
    auto addEdge = [&](int x, int y, int dir) { out[std::size_t(y * vw + x)] |= std::uint8_t(1 << dir); };
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!inside(x, y))
                continue;
            if (!inside(x, y - 1))
                addEdge(x, y, 0);
            if (!inside(x + 1, y))
                addEdge(x + 1, y, 1);
            if (!inside(x, y + 1))
                addEdge(x + 1, y + 1, 2);
            if (!inside(x - 1, y))
                addEdge(x, y + 1, 3);
        }
    }
    static const int dx[4] = {1, 0, -1, 0};
    static const int dy[4] = {0, 1, 0, -1};
    std::vector<std::vector<QPoint>> loops;
    for (int start = 0; start < int(out.size()); ++start) {
        while (out[std::size_t(start)]) {
            int startDir = 0;
            while (!(out[std::size_t(start)] & (1 << startDir)))
                ++startDir;
            std::vector<QPoint> loop;
            int x = start % vw, y = start / vw;
            int dir = startDir;
            for (;;) {
                loop.emplace_back(x, y);
                out[std::size_t(y * vw + x)] &= std::uint8_t(~(1 << dir));
                x += dx[dir];
                y += dy[dir];
                // Am Start zählt die schon benutzte Startkante mit: wählt die Regel sie, ist
                // der Umriss geschlossen (auch wenn der Start ein Sattelpunkt ist)
                const bool atStart = y * vw + x == start;
                const std::uint8_t here = out[std::size_t(y * vw + x)] | (atStart ? (1 << startDir) : 0);
                int next = -1;
                for (int turn : {1, 0, 3}) { // rechts, geradeaus, links
                    const int d = (dir + turn) % 4;
                    if (here & (1 << d)) {
                        next = d;
                        break;
                    }
                }
                if (next < 0 || (atStart && next == startDir))
                    break;
                dir = next;
            }
            if (loop.size() >= 4)
                loops.push_back(std::move(loop));
        }
    }
    return loops;
}

// Mehrheitsfilter über eine Kreisscheibe mit Radius r: rundet Ecken ab und nimmt kleine Zacken
// und Einbuchtungen weg (Glättung). -1 (durchsichtig) zählt als eigene Farbe.
void smoothLabels(std::vector<int>& labels, int w, int h, int r, int colorCount)
{
    if (r <= 0)
        return;
    std::vector<int> span(std::size_t(2 * r + 1)); // halbe Breite der Scheibe je Zeile
    for (int dy = -r; dy <= r; ++dy)
        span[std::size_t(dy + r)] = int(std::sqrt(double(r * r + r - dy * dy)));
    const std::vector<int> source = labels;
    const auto at = [&](int x, int y) {
        return source[std::size_t(std::clamp(y, 0, h - 1) * w + std::clamp(x, 0, w - 1))] + 1;
    };
    std::vector<int> count(std::size_t(colorCount) + 1);
    for (int y = 0; y < h; ++y) {
        // Fenster am Zeilenanfang füllen, dann beim Weiterschieben nur die Ränder nachführen
        std::fill(count.begin(), count.end(), 0);
        for (int dy = -r; dy <= r; ++dy) {
            const int sx = span[std::size_t(dy + r)];
            for (int dx = -sx; dx <= sx; ++dx)
                ++count[std::size_t(at(dx, y + dy))];
        }
        for (int x = 0; x < w; ++x) {
            if (x > 0) {
                for (int dy = -r; dy <= r; ++dy) {
                    const int sx = span[std::size_t(dy + r)];
                    --count[std::size_t(at(x - sx - 1, y + dy))];
                    ++count[std::size_t(at(x + sx, y + dy))];
                }
            }
            int best = at(x, y); // bei Gleichstand bleibt die eigene Farbe
            for (int k = 0; k <= colorCount; ++k) {
                if (count[std::size_t(k)] > count[std::size_t(best)])
                    best = k;
            }
            labels[std::size_t(y * w + x)] = best - 1;
        }
    }
}

// Umriss mit Potrace in Geraden, Ecken und Bézierkurven umwandeln. grow verschiebt die Kurve
// nach aussen (links der Laufrichtung, die Fläche liegt rechts); unit = Rechenpixel je Bildpixel.
SubPath fitLoop(const std::vector<QPoint>& loop, const Settings& s, double grow, double unit)
{
    std::vector<long> xy;
    xy.reserve(loop.size() * 2);
    for (const QPoint& p : loop) {
        xy.push_back(p.x());
        xy.push_back(p.y());
    }
    // Kantenglättung 0..100 -> Potraces alphamax (1.0 = Potraces Vorgabe: echte Ecken bleiben,
    // Rundungen werden Kurven .. 1.33 = keine Ecken), Glättung 0..100 -> Toleranz beim
    // Zusammenfassen der Kurven (in Pixeln)
    const double alphamax = 1.0 + std::clamp(s.cornerSmoothing, 0, 100) * 0.0033;
    const int smoothing = std::clamp(s.smoothing, 0, 100);
    const double tolerance = (0.1 + smoothing * 0.012 + std::max(0, smoothing - 25) * 0.02) * unit;
    vek_fit_segment_t* segments = nullptr;
    const int n = vek_fit(xy.data(), int(loop.size()), alphamax, tolerance, &segments);
    SubPath path;
    path.closed = true;
    if (n <= 0) {
        free(segments);
        for (const QPoint& p : loop)
            path.nodes.append(CurveNode{QPointF(p)});
        return path;
    }
    for (int i = 0; i < n; ++i) {
        const vek_fit_segment_t& seg = segments[i];
        const QPointF a(seg.c[0], seg.c[1]), b(seg.c[2], seg.c[3]), end(seg.c[4], seg.c[5]);
        if (seg.corner) {
            path.nodes.append(CurveNode{b, false, {}, {}, NodeKind::Cusp});
            path.nodes.append(CurveNode{end, false, {}, {}, NodeKind::Smooth});
        } else {
            path.nodes.append(CurveNode{end, true, a, b, NodeKind::Smooth});
        }
    }
    free(segments);
    // Kantenmitten zwischen zwei Geraden liegen auf derselben Geraden: weglassen
    for (int i = int(path.nodes.size()) - 1; i >= 0 && path.nodes.size() > 3; --i) {
        const int next = (i + 1) % int(path.nodes.size());
        if (path.nodes[i].kind == NodeKind::Smooth && !path.nodes[i].curveIn && !path.nodes[next].curveIn)
            path.nodes.removeAt(i);
    }
    // Potrace hält den Übergang einer langen Geraden in eine kleine Rundung für eine Ecke
    // (flacher Knick neben einer kurzen Kante): dort weich in die Rundung überleiten.
    // Echte Ecken (stärker als 60°) und Vielecke mit gleich langen Kanten bleiben.
    for (int i = 0; i < int(path.nodes.size()) && path.nodes.size() > 3; ++i) {
        const int m = int(path.nodes.size());
        const int prev = (i - 1 + m) % m, next = (i + 1) % m;
        CurveNode& v = path.nodes[i];
        if (v.kind != NodeKind::Cusp || v.curveIn || path.nodes[next].curveIn)
            continue;
        const QPointF P = path.nodes[prev].point, V = v.point, N = path.nodes[next].point;
        const QPointF in = V - P, out = N - V;
        const double a = std::hypot(in.x(), in.y()), b = std::hypot(out.x(), out.y());
        if (a <= 0 || b <= 0 || std::min(a, b) > 0.35 * std::max(a, b))
            continue;
        const double cosTurn = (in.x() * out.x() + in.y() * out.y()) / (a * b);
        if (cosTurn < 0.5) // mehr als 60° Knick
            continue;
        const double d = std::min(a, b);
        const bool reusePrev = d >= 0.98 * a, reuseNext = d >= 0.98 * b;
        const QPointF A = reusePrev ? P : V - in * (d / a);
        const QPointF B = reuseNext ? N : V + out * (d / b);
        const double k = 0.55;
        CurveNode curve{B, true, A + (V - A) * k, B + (V - B) * k, NodeKind::Smooth};
        if (reuseNext) {
            path.nodes[next] = curve;
            path.nodes.removeAt(i);
        } else {
            path.nodes[i] = curve;
        }
        if (!reusePrev) {
            path.nodes.insert(i, CurveNode{A, false, {}, {}, NodeKind::Smooth});
            ++i;
        }
        if (reusePrev)
            path.nodes[prev].kind = NodeKind::Smooth;
    }
    if (grow != 0.0) {
        const int m = int(path.nodes.size());
        QList<QPointF> offsets(m);
        for (int i = 0; i < m; ++i) {
            const QPointF d = path.nodes[(i + 1) % m].point - path.nodes[(i - 1 + m) % m].point;
            const double len = std::hypot(d.x(), d.y());
            if (len > 0)
                offsets[i] = QPointF(d.y(), -d.x()) * (grow / len);
        }
        for (int i = 0; i < m; ++i) {
            CurveNode& node = path.nodes[i];
            node.point += offsets[i];
            if (node.curveIn) {
                node.c1 += offsets[(i - 1 + m) % m];
                node.c2 += offsets[i];
            }
        }
    }
    return path;
}

int countNodes(const QList<SubPath>& paths)
{
    int n = 0;
    for (const SubPath& p : paths)
        n += int(p.nodes.size());
    return n;
}

QRectF pathBounds(const QList<SubPath>& paths)
{
    QPolygonF points;
    for (const SubPath& p : paths) {
        for (const CurveNode& n : p.nodes) {
            points << n.point;
            if (n.curveIn)
                points << n.c1 << n.c2;
        }
    }
    return points.boundingRect();
}

// Kantenglättung im Bild: Mischfarben-Cluster ohne eigene Fläche (kaum Pixel, deren vier
// Nachbarn dieselbe Farbe haben) auf die farblich nächste echte Farbe der Umgebung verteilen
void absorbEdgeColors(std::vector<int>& labels, int w, int h, const std::vector<Rgb>& centers, int radius, int depth)
{
    const int k = int(centers.size());
    std::vector<int> count(std::size_t(k), 0), interior(std::size_t(k), 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int l = labels[std::size_t(y * w + x)];
            if (l < 0)
                continue;
            ++count[std::size_t(l)];
            auto same = [&](int xx, int yy) {
                return xx < 0 || yy < 0 || xx >= w || yy >= h || labels[std::size_t(yy * w + xx)] == l;
            };
            // Innen = auch "depth" Pixel weiter in alle Richtungen dieselbe Farbe: nach dem Vergrössern
            // zählen so dünne Striche (kleine Schrift) nicht als eigene Fläche
            if (same(x - depth, y) && same(x + depth, y) && same(x, y - depth) && same(x, y + depth))
                ++interior[std::size_t(l)];
        }
    }
    std::vector<bool> solid(std::size_t(k), false);
    for (int c = 0; c < k; ++c)
        solid[std::size_t(c)] = count[std::size_t(c)] > 0 && interior[std::size_t(c)] >= 0.2 * count[std::size_t(c)];
    // Nur Farben weg, die zwischen zwei echten Farben liegen (oder nahe an einer):
    // eine dünne, aber eigenständige Linie (rot auf weiss) bleibt
    std::vector<bool> absorb(std::size_t(k), false);
    for (int c = 0; c < k; ++c) {
        if (solid[std::size_t(c)] || count[std::size_t(c)] == 0)
            continue;
        const Rgb& p = centers[std::size_t(c)];
        for (int a = 0; a < k && !absorb[std::size_t(c)]; ++a) {
            if (!solid[std::size_t(a)])
                continue;
            if (distance2(p, centers[std::size_t(a)]) < 400.0f)
                absorb[std::size_t(c)] = true;
            for (int b = a + 1; b < k && !absorb[std::size_t(c)]; ++b) {
                if (!solid[std::size_t(b)])
                    continue;
                const Rgb& A = centers[std::size_t(a)];
                const Rgb& B = centers[std::size_t(b)];
                const float ux = B.r - A.r, uy = B.g - A.g, uz = B.b - A.b;
                const float len2 = ux * ux + uy * uy + uz * uz;
                if (len2 <= 0)
                    continue;
                const float t = std::clamp(((p.r - A.r) * ux + (p.g - A.g) * uy + (p.b - A.b) * uz) / len2, 0.0f, 1.0f);
                const Rgb q{A.r + t * ux, A.g + t * uy, A.b + t * uz};
                if (distance2(p, q) < 400.0f)
                    absorb[std::size_t(c)] = true;
            }
        }
    }
    if (std::none_of(absorb.begin(), absorb.end(), [](bool b) { return b; }))
        return;
    // Vollfarbe auf der Linie vom Hintergrund bg durch die Mischfarbe mix, mindestens so weit weg wie
    // mix (die eigentliche Farbe einer dünnen Form). Liefert sie und den Anteil des Wegs, den mix
    // vom Hintergrund zu ihr schon zurückgelegt hat; -1, wenn die Linie keine Farbe trifft.
    auto alongLine = [&](int bgLabel, int mixLabel) {
        const Rgb& bg = centers[std::size_t(bgLabel)];
        const Rgb& mix = centers[std::size_t(mixLabel)];
        const float ux = mix.r - bg.r, uy = mix.g - bg.g, uz = mix.b - bg.b;
        const float len2 = ux * ux + uy * uy + uz * uz;
        int target = -1;
        float bestD = std::numeric_limits<float>::max(), bestT = 1.0f;
        for (int o = 0; o < k && len2 > 0; ++o) {
            if (o == bgLabel || absorb[std::size_t(o)] || !solid[std::size_t(o)])
                continue;
            const Rgb& c = centers[std::size_t(o)];
            const float t = ((c.r - bg.r) * ux + (c.g - bg.g) * uy + (c.b - bg.b) * uz) / len2;
            if (t < 0.9f)
                continue;
            const float d = distance2(c, Rgb{bg.r + t * ux, bg.g + t * uy, bg.b + t * uz}) / (t * t);
            if (d < bestD) {
                bestD = d;
                bestT = t;
                target = o;
            }
        }
        if (target < 0 || bestD >= 0.25f * distance2(mix, bg))
            return std::pair{-1, 0.0f};
        return std::pair{target, 1.0f / bestT};
    };

    // Ein Saum liegt zwischen zwei Farben. Grenzt eine Mischfarben-Fläche fast nur an eine Farbe,
    // ist sie eine eigene Form (dünne Schrift auf einem Hintergrund): als Ganzes zur ähnlichsten
    // Vollfarbe, damit Buchstaben nicht stückweise im Hintergrund verschwinden
    {
        std::vector<char> seen(labels.size(), 0);
        std::vector<int> stack, members;
        for (int start = 0; start < w * h; ++start) {
            const int l = labels[std::size_t(start)];
            if (l < 0 || !absorb[std::size_t(l)] || seen[std::size_t(start)])
                continue;
            members.clear();
            stack.assign(1, start);
            seen[std::size_t(start)] = 1;
            std::map<int, int> touch;
            std::map<int, int> parts; // Mischfarben dieser Form (Ring und Kern eines dünnen Buchstabens)
            int contacts = 0;
            while (!stack.empty()) {
                const int i = stack.back();
                stack.pop_back();
                members.push_back(i);
                ++parts[labels[std::size_t(i)]];
                const int x = i % w, y = i / w;
                const int around[4] = {x > 0 ? i - 1 : -1, x + 1 < w ? i + 1 : -1, y > 0 ? i - w : -1, y + 1 < h ? i + w : -1};
                for (int j : around) {
                    if (j < 0)
                        continue;
                    const int o = labels[std::size_t(j)];
                    if (o >= 0 && absorb[std::size_t(o)]) {
                        if (!seen[std::size_t(j)]) {
                            seen[std::size_t(j)] = 1;
                            stack.push_back(j);
                        }
                    } else if (o >= 0 && !absorb[std::size_t(o)]) {
                        ++touch[o];
                        ++contacts;
                    }
                }
            }
            if (contacts == 0)
                continue;
            const auto main = std::max_element(touch.begin(), touch.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
            // Saum: Die Mischfarbe liegt zwischen zwei Farben, an die die Form grenzt (z.B. der Ring
            // zwischen Hintergrund und Buchstabenkern). Dann Pixel für Pixel unten aufteilen.
            bool seam = false;
            for (auto a = touch.begin(); a != touch.end() && !seam; ++a) {
                for (auto b = std::next(a); b != touch.end() && !seam; ++b) {
                    if (a->second * 10 < contacts || b->second * 10 < contacts)
                        continue;
                    const Rgb& A = centers[std::size_t(a->first)];
                    const Rgb& B = centers[std::size_t(b->first)];
                    for (const auto& [part, n] : parts) {
                        const Rgb& m = centers[std::size_t(part)];
                        const float ux = B.r - A.r, uy = B.g - A.g, uz = B.b - A.b;
                        const float len2 = ux * ux + uy * uy + uz * uz;
                        if (len2 <= 0)
                            continue;
                        const float t = ((m.r - A.r) * ux + (m.g - A.g) * uy + (m.b - A.b) * uz) / len2;
                        if (t > 0.1f && t < 0.9f && distance2(m, Rgb{A.r + t * ux, A.g + t * uy, A.b + t * uz}) < 400.0f) {
                            seam = true;
                            break;
                        }
                    }
                }
            }
            if (seam)
                continue;
            // Kräftigste Mischfarbe der Form: am weitesten vom Hintergrund weg
            const Rgb& bg = centers[std::size_t(main->first)];
            int strongest = l;
            for (const auto& [part, n] : parts) {
                if (distance2(centers[std::size_t(part)], bg) > distance2(centers[std::size_t(strongest)], bg))
                    strongest = part;
            }
            const int target = alongLine(main->first, strongest).first;
            // Nur wenn die Linie wirklich eine Farbe trifft; sonst Pixel für Pixel unten
            if (target >= 0) {
                for (int i : members)
                    labels[std::size_t(i)] = target;
            }
        }
    }
    const std::vector<int> before = labels;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int l = before[std::size_t(y * w + x)];
            if (l < 0 || !absorb[std::size_t(l)])
                continue;
            // Wie eine adaptive Schwelle: Hintergrund ist die häufigste Vollfarbe in der Umgebung,
            // Vordergrund die Vollfarbe auf der Linie vom Hintergrund durch die Mischfarbe. Das Pixel
            // gehört zum Vordergrund, wenn es mindestens halb dorthin reicht; so verliert dünne Schrift
            // keine Striche, auch wenn ringsum kein Pixel ganz in ihrer Farbe liegt
            int best = -1;
            float bestD = std::numeric_limits<float>::max();
            std::map<int, int> around;
            for (int yy = std::max(0, y - radius); yy <= std::min(h - 1, y + radius); ++yy) {
                for (int xx = std::max(0, x - radius); xx <= std::min(w - 1, x + radius); ++xx) {
                    const int o = before[std::size_t(yy * w + xx)];
                    if (o < 0 || absorb[std::size_t(o)])
                        continue;
                    ++around[o];
                    const float d = distance2(centers[std::size_t(l)], centers[std::size_t(o)]);
                    if (d < bestD) {
                        bestD = d;
                        best = o;
                    }
                }
            }
            if (!around.empty()) {
                const int bgLabel = std::max_element(around.begin(), around.end(), [](const auto& a, const auto& b) { return a.second < b.second; })->first;
                const auto [fg, reach] = alongLine(bgLabel, l);
                if (fg >= 0)
                    best = reach >= 0.5f ? fg : bgLabel;
            }
            if (best < 0) {
                for (int o = 0; o < k; ++o) {
                    if (absorb[std::size_t(o)] || count[std::size_t(o)] == 0)
                        continue;
                    const float d = distance2(centers[std::size_t(l)], centers[std::size_t(o)]);
                    if (d < bestD) {
                        bestD = d;
                        best = o;
                    }
                }
            }
            if (best >= 0)
                labels[std::size_t(y * w + x)] = best;
        }
    }
}

} // namespace

QList<Preset> presets()
{
    return {Preset::LineArt, Preset::Logo, Preset::DetailedLogo, Preset::Clipart, Preset::LowQuality, Preset::HighQuality};
}

QString presetName(Preset preset)
{
    switch (preset) {
    case Preset::LineArt: return QObject::tr("Strichgrafik");
    case Preset::Logo: return QObject::tr("Logo");
    case Preset::DetailedLogo: return QObject::tr("Detailliertes Logo");
    case Preset::Clipart: return QObject::tr("Clipart");
    case Preset::LowQuality: return QObject::tr("Bild niedriger Qualität");
    case Preset::HighQuality: return QObject::tr("Bild hoher Qualität");
    }
    return {};
}

Settings presetSettings(Preset preset)
{
    Settings s;
    switch (preset) {
    case Preset::LineArt:
        // Nur die dunkle Farbe wird zur Kurve, der helle Grund ist weg
        s.colors = 2;
        s.detail = 70;
        s.smoothing = 25;
        s.removeBackground = true;
        s.removeColorEverywhere = true;
        break;
    case Preset::Logo: break;
    case Preset::DetailedLogo:
        s.colors = 16;
        s.detail = 70;
        s.smoothing = 20;
        break;
    case Preset::Clipart:
        s.colors = 24;
        s.detail = 60;
        break;
    case Preset::LowQuality:
        s.colors = 12;
        s.detail = 30;
        s.smoothing = 50;
        break;
    case Preset::HighQuality:
        s.colors = 48;
        s.detail = 85;
        s.smoothing = 15;
        break;
    }
    return s;
}

// Kleinere Bilder werden für die Farbtrennung auf etwa diese Seitenlänge vergrössert
constexpr int kWorkingSide = 1600;

Result run(const QImage& source, const Settings& s)
{
    Result result;
    result.groupByColor = s.groupByColor;
    if (source.isNull())
        return result;
    result.size = source.size();
    QImage img = source.convertToFormat(QImage::Format_RGBA64);
    double scale = 1.0;
    const int side = std::max(img.width(), img.height());
    // Nachgezeichnet wird kleiner Bilder in doppelter Grösse (wie mkbitmap zu Potrace: Graustufen der
    // Kantenglättung werden Form statt Treppe), die Farben aber in bis zu vierfacher entschieden
    const int traceFactor = side * 2 <= s.maxSide ? 2 : 1;
    if (side > s.maxSide) {
        img = img.scaled(s.maxSide, s.maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scale = double(source.width()) / img.width();
    }
    // Farben je zur Hälfte am Bild in seiner Grösse (kräftige Farben dünner Linien) und am vergrösserten
    // Bild (Mischfarben der Kantenglättung, die der Saum-Schritt unten braucht) bestimmen
    std::vector<QRgba64> samplePixels;
    std::vector<bool> sampleOpaque;
    for (int y = 0; y < img.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgba64*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            samplePixels.push_back(line[x]);
            sampleOpaque.push_back(line[x].alpha() >= 32768);
        }
    }
    if (side <= s.maxSide && side * 2 <= kWorkingSide && (s.colors > 2 || traceFactor > 1)) {
        // Kleine Bilder weich vergrössern: dünne Striche (kleine Schrift) bekommen Pixel, die ganz
        // in ihrer Farbe liegen
        // Mit nur zwei Farben gibt es keine Mischfarben zu entscheiden: dann wie bisher doppelt
        const int factor = s.colors > 2 && side * 4 <= kWorkingSide ? 4 : 2;
        img = img.scaled(img.width() * factor, img.height() * factor, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        scale = 1.0 / factor;
    }
    double unit = std::max(1.0, 1.0 / scale); // Rechenpixel je Bildpixel
    int w = img.width(), h = img.height();
    std::vector<QRgba64> pixels(std::size_t(w) * h);
    std::vector<bool> opaque(pixels.size());
    for (int y = 0; y < h; ++y) {
        const auto* line = reinterpret_cast<const QRgba64*>(img.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            pixels[std::size_t(y * w + x)] = line[x];
            opaque[std::size_t(y * w + x)] = line[x].alpha() >= 32768;
        }
    }
    if (scale < 1.0) {
        const std::size_t original = samplePixels.size();
        const int copies = int(std::lround(unit * unit)) - 1;
        for (int c = 0; c < copies; ++c) {
            samplePixels.insert(samplePixels.end(), samplePixels.begin(), samplePixels.begin() + std::ptrdiff_t(original));
            sampleOpaque.insert(sampleOpaque.end(), sampleOpaque.begin(), sampleOpaque.begin() + std::ptrdiff_t(original));
        }
        samplePixels.insert(samplePixels.end(), pixels.begin(), pixels.end());
        sampleOpaque.insert(sampleOpaque.end(), opaque.begin(), opaque.end());
    }
    std::vector<Rgb> centers = quantize(samplePixels, sampleOpaque, std::clamp(s.colors, 2, 64));
    if (centers.empty())
        return result;

    // Jedes Pixel der nächsten Farbe zuordnen (gleiche Farben nur einmal rechnen)
    std::vector<int> labels(pixels.size(), -1);
    QHash<quint64, int> cache;
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        if (!opaque[i])
            continue;
        QRgba64 c = pixels[i];
        c.setAlpha(65535);
        auto it = cache.constFind(quint64(c));
        if (it == cache.constEnd()) {
            int best = 0;
            float bestD = std::numeric_limits<float>::max();
            for (int k = 0; k < int(centers.size()); ++k) {
                const float d = distance2(toRgb(c), centers[std::size_t(k)]);
                if (d < bestD) {
                    bestD = d;
                    best = k;
                }
            }
            it = cache.insert(quint64(c), best);
        }
        labels[i] = *it;
    }

    // Fast gleiche Farben (k-Means teilt eine Fläche gern in Nuancen) zusammenlegen
    {
        std::vector<int> count(centers.size(), 0);
        for (int l : labels) {
            if (l >= 0)
                ++count[std::size_t(l)];
        }
        std::vector<int> target(centers.size());
        for (std::size_t a = 0; a < centers.size(); ++a)
            target[a] = int(a);
        for (std::size_t a = 0; a < centers.size(); ++a) {
            if (target[a] != int(a) || count[a] == 0)
                continue;
            for (std::size_t b = a + 1; b < centers.size(); ++b) {
                if (target[b] != int(b) || count[b] == 0 || distance2(centers[a], centers[b]) >= 60.0f)
                    continue;
                const float wa = float(count[a]), wb = float(count[b]);
                centers[a] = Rgb{(centers[a].r * wa + centers[b].r * wb) / (wa + wb),
                                 (centers[a].g * wa + centers[b].g * wb) / (wa + wb),
                                 (centers[a].b * wa + centers[b].b * wb) / (wa + wb)};
                count[a] += count[b];
                count[b] = 0;
                target[b] = int(a);
            }
        }
        for (int& l : labels) {
            if (l >= 0)
                l = target[std::size_t(l)];
        }
    }
    absorbEdgeColors(labels, w, h, centers, 2 * int(std::ceil(unit)), int(std::ceil(unit)));

    // Kleinste Fleckgrösse aus "Detail": 100 = alles bleibt, 0 = grobe Flächen
    const double coarse = (100.0 - std::clamp(s.detail, 0, 100)) / 100.0;
    const int minArea = 1 + int(coarse * coarse * 0.002 * w * h);
    removeSpeckles(labels, w, h, minArea, std::max(int(6 * unit * unit), minArea / 4), centers);
    // Glättung über 25 rundet die Flächen selbst (bis 5 Bildpixel bei 100), darunter nur die Kurven
    smoothLabels(labels, w, h, int((std::clamp(s.smoothing, 0, 100) - 25) * unit / 15.0), int(centers.size()));

    // Ausgabefarben: ermittelte Farbe, im Register "Farben" bearbeitet oder zusammengeführt
    std::vector<QRgb> sourceRgb(centers.size());
    std::vector<QColor> outColor(centers.size());
    for (std::size_t k = 0; k < centers.size(); ++k) {
        const Rgb& c = centers[k];
        sourceRgb[k] = qRgb(int(std::lround(c.r)), int(std::lround(c.g)), int(std::lround(c.b)));
        outColor[k] = QColor::fromRgb(s.recolor.value(sourceRgb[k], sourceRgb[k]));
    }
    // Gleiche Ausgabefarbe = eine Farbe, damit Nachbarflächen verschmelzen
    {
        std::vector<int> same(centers.size());
        for (std::size_t k = 0; k < centers.size(); ++k) {
            same[k] = int(k);
            for (std::size_t j = 0; j < k; ++j) {
                if (outColor[j] == outColor[k]) {
                    same[k] = int(j);
                    break;
                }
            }
        }
        for (int& l : labels) {
            if (l >= 0)
                l = same[std::size_t(l)];
        }
    }

    if (s.removeBackground) {
        int background = -1;
        if (s.background) {
            std::vector<bool> used(centers.size(), false);
            for (int l : labels) {
                if (l >= 0)
                    used[std::size_t(l)] = true;
            }
            float bestD = std::numeric_limits<float>::max();
            const Rgb want = toRgb(s.background->rgba64());
            for (int k = 0; k < int(centers.size()); ++k) {
                const float d = distance2(want, centers[std::size_t(k)]);
                if (used[std::size_t(k)] && d < bestD) {
                    bestD = d;
                    background = k;
                }
            }
        } else {
            std::map<int, int> border;
            for (int x = 0; x < w; ++x) {
                ++border[labels[std::size_t(x)]];
                ++border[labels[std::size_t((h - 1) * w + x)]];
            }
            for (int y = 0; y < h; ++y) {
                ++border[labels[std::size_t(y * w)]];
                ++border[labels[std::size_t(y * w + w - 1)]];
            }
            // Ist ein Teil des Randes schon durchsichtig, ist das Bild freigestellt und hat keinen
            // Hintergrund mehr: sonst hielte man eine randberührende Form für den Hintergrund
            const int transparent = border[-1];
            border.erase(-1);
            const auto mostCommon = std::max_element(border.begin(), border.end(),
                                                     [](const auto& a, const auto& b) { return a.second < b.second; });
            if (mostCommon != border.end() && transparent * 10 < 2 * (w + h))
                background = mostCommon->first;
        }
        // Nie die einzige Farbe entfernen: dann bliebe nichts übrig
        if (background >= 0) {
            bool other = false;
            for (int l : labels) {
                if (l >= 0 && l != background) {
                    other = true;
                    break;
                }
            }
            if (!other)
                background = -1;
        }
        if (background >= 0 && s.removeColorEverywhere) {
            for (int& l : labels) {
                if (l == background)
                    l = -1;
            }
        } else if (background >= 0) {
            // Nur die Flächen dieser Farbe, die den Bildrand berühren
            std::vector<int> stack;
            auto seed = [&](int i) {
                if (labels[std::size_t(i)] == background) {
                    labels[std::size_t(i)] = -1;
                    stack.push_back(i);
                }
            };
            for (int x = 0; x < w; ++x) {
                seed(x);
                seed((h - 1) * w + x);
            }
            for (int y = 0; y < h; ++y) {
                seed(y * w);
                seed(y * w + w - 1);
            }
            while (!stack.empty()) {
                const int i = stack.back();
                stack.pop_back();
                const int x = i % w, y = i / w;
                if (x > 0)
                    seed(i - 1);
                if (x + 1 < w)
                    seed(i + 1);
                if (y > 0)
                    seed(i - w);
                if (y + 1 < h)
                    seed(i + w);
            }
        }
    }

    // Pixel links der ersten Kante eines Umrisses (bei Löchern: im Loch)
    // Zurück auf die Auflösung zum Nachzeichnen, sonst bekämen glatte Formen unnötig viele Knoten.
    // Je Block zählt die Mehrheit, bei Gleichstand die häufigere Farbe (sonst wird Schrift zu fett).
    if (const int r = int(std::lround(unit)) / traceFactor; scale < 1.0 && r >= 2) {
        std::map<int, int> total;
        for (int l : labels)
            ++total[l];
        const int w2 = w / r, h2 = h / r;
        std::vector<int> reduced(std::size_t(w2) * h2);
        std::map<int, int> votes;
        for (int y = 0; y < h2; ++y) {
            for (int x = 0; x < w2; ++x) {
                votes.clear();
                for (int dy = 0; dy < r; ++dy)
                    for (int dx = 0; dx < r; ++dx)
                        ++votes[labels[std::size_t((y * r + dy) * w + x * r + dx)]];
                int best = votes.begin()->first, bestCount = 0;
                for (const auto& [l, n] : votes) {
                    if (n > bestCount || (n == bestCount && total[l] > total[best])) {
                        best = l;
                        bestCount = n;
                    }
                }
                reduced[std::size_t(y * w2 + x)] = best;
            }
        }
        labels = std::move(reduced);
        w = w2;
        h = h2;
        scale *= r;
        unit /= r;
    }

    auto leftPixel = [&](const std::vector<QPoint>& loop) {
        const QPoint a = loop[0];
        const QPoint d = loop[1] - a;
        const QPoint p = d.x() > 0 ? QPoint(a.x(), a.y() - 1)
                         : d.y() > 0 ? QPoint(a.x(), a.y())
                         : d.x() < 0 ? QPoint(a.x() - 1, a.y())
                                     : QPoint(a.x() - 1, a.y() - 1);
        return p.x() >= 0 && p.y() >= 0 && p.x() < w && p.y() < h ? labels[std::size_t(p.y() * w + p.x())] : -1;
    };
    auto scaled = [scale](SubPath path) {
        if (scale != 1.0) {
            for (CurveNode& n : path.nodes) {
                n.point *= scale;
                n.c1 *= scale;
                n.c2 *= scale;
            }
        }
        return path;
    };

    // Je Farbe die Umrisse; Löcher gehören zur kleinsten umgebenden Fläche derselben Farbe
    std::vector<char> mask(labels.size());
    QList<Shape> shapes;
    for (int k = 0; k < int(centers.size()); ++k) {
        int count = 0;
        for (std::size_t i = 0; i < labels.size(); ++i) {
            mask[i] = labels[i] == k;
            count += mask[i];
        }
        if (count == 0)
            continue;
        result.palette.append(PaletteEntry{sourceRgb[std::size_t(k)], outColor[std::size_t(k)], count});
        const QColor color = outColor[std::size_t(k)];
        struct Outer {
            QPolygonF polygon;
            int shape;
        };
        std::vector<Outer> outers;
        std::vector<std::vector<QPoint>> holes;
        for (std::vector<QPoint>& loop : traceMask(mask, w, h)) {
            const double area = signedArea(loop);
            if (area < 0) {
                holes.push_back(std::move(loop));
                continue;
            }
            // Gestapelt: Flächen leicht vergrössern, sonst scheinen an den Nahtstellen
            // zweier Farben Haarlinien durch
            SubPath path = fitLoop(loop, s, s.removeOverlap ? 0.0 : 0.35 * unit, unit);
            if (path.nodes.size() < 2)
                continue;
            QPolygonF polygon;
            for (const QPoint& pt : loop)
                polygon << QPointF(pt);
            outers.push_back(Outer{polygon, int(shapes.size())});
            shapes.append(Shape{{scaled(path)}, color, area * scale * scale});
        }
        for (const std::vector<QPoint>& hole : holes) {
            // Gestapelt füllen kleinere Flächen darüber die Löcher; offen bleiben nur Löcher
            // zum entfernten Hintergrund
            if (!s.removeOverlap && leftPixel(hole) >= 0)
                continue;
            const QPointF probe = (QPointF(hole[0]) + QPointF(hole[1])) / 2.0;
            int owner = -1;
            double ownerArea = 0.0;
            for (const Outer& o : outers) {
                const double a = shapes[o.shape].area;
                if ((owner < 0 || a < ownerArea) && o.polygon.containsPoint(probe, Qt::OddEvenFill)) {
                    owner = o.shape;
                    ownerArea = a;
                }
            }
            SubPath path = fitLoop(hole, s, 0.0, unit);
            if (owner >= 0 && path.nodes.size() >= 2)
                shapes[owner].paths.append(scaled(path));
        }
    }
    // Grosse Flächen unten, kleine oben
    std::stable_sort(shapes.begin(), shapes.end(), [](const Shape& a, const Shape& b) { return a.area > b.area; });

    // Gleichfarbige Flächen zu einem Objekt, wenn keine andersfarbige dazwischen sie überdeckt.
    // Nur bei getrennten Umrissen: eine Insel in der anderen würde in einer Kurve (gerade-
    // ungerade Füllung) zum Loch.
    if (s.mergeAdjacent) {
        QList<Shape> merged;
        QList<QRectF> bounds;
        for (const Shape& shape : std::as_const(shapes)) {
            const QRectF b = pathBounds(shape.paths);
            int target = -1;
            for (int j = int(merged.size()) - 1; j >= 0; --j) {
                if (merged[j].color == shape.color) {
                    if (!bounds[j].intersects(b))
                        target = j;
                    break;
                }
                if (bounds[j].intersects(b))
                    break;
            }
            if (target >= 0) {
                merged[target].paths += shape.paths;
                merged[target].area += shape.area;
                bounds[target] = bounds[target].united(b);
            } else {
                merged.append(shape);
                bounds.append(b);
            }
        }
        shapes = merged;
    }
    for (const Shape& shape : std::as_const(shapes))
        result.nodes += countNodes(shape.paths);
    std::stable_sort(result.palette.begin(), result.palette.end(),
                     [](const PaletteEntry& a, const PaletteEntry& b) { return a.pixels > b.pixels; });
    result.colors = int(result.palette.size());
    result.shapes = shapes;
    return result;
}

} // namespace vek::trace

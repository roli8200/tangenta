#include "Imposition.h"

#include <QObject>

#include <algorithm>
#include <cmath>
#include <set>

namespace vek::impose {

namespace {

// Raster aus gleich grossen Zellen; Abstände zwischen Spalten bzw. Zeilen einzeln
struct Grid {
    int cols = 1;
    int rows = 1;
    QSizeF cell;
    QList<double> colGaps; // cols - 1
    QList<double> rowGaps; // rows - 1
    QList<int> spines;     // Falz ohne Schnitt: Spaltengrenze i (zwischen Spalte i und i+1)
    QList<int> headFolds;  // Zeilengrenzen, an denen gefalzt (und bei Abstand > 0 beschnitten) wird

    QSizeF block() const
    {
        double w = cols * cell.width(), h = rows * cell.height();
        for (double g : colGaps)
            w += g;
        for (double g : rowGaps)
            h += g;
        return {w, h};
    }
};

// strict: mit Papierrand und Anschnitt; sonst genügt das Endformat (A5-Heft auf A4)
bool fits(const QSizeF& sheet, const Grid& g, const Settings& s, bool strict = true)
{
    const QSizeF b = g.block();
    const double extra = strict ? 2 * (s.bleed + s.margin) : 0.0;
    return b.width() + extra <= sheet.width() + 1e-6 && b.height() + extra <= sheet.height() + 1e-6;
}

// Zellen auf dem Bogen (Zeile für Zeile, von oben links), Block mittig
QList<QRectF> cells(const QSizeF& sheet, const Grid& g)
{
    const QSizeF b = g.block();
    const QPointF origin((sheet.width() - b.width()) / 2, (sheet.height() - b.height()) / 2);
    QList<QRectF> out;
    double y = origin.y();
    for (int r = 0; r < g.rows; ++r) {
        double x = origin.x();
        for (int c = 0; c < g.cols; ++c) {
            out << QRectF(QPointF(x, y), g.cell);
            x += g.cell.width() + (c < g.colGaps.size() ? g.colGaps[c] : 0.0);
        }
        y += g.cell.height() + (r < g.rowGaps.size() ? g.rowGaps[r] : 0.0);
    }
    return out;
}

// Sichtbarer Anschnitt einer Zelle: aussen voll, an inneren Schnitten höchstens der halbe Abstand,
// im Bund (Falz ohne Abstand) keiner
QMarginsF bleedFor(const Grid& g, int index, double bleed)
{
    const int c = index % g.cols, r = index / g.cols;
    auto inner = [&](double gap) { return gap > 0 ? std::min(bleed, gap / 2) : 0.0; };
    const double left = c == 0 ? bleed : inner(g.colGaps.value(c - 1));
    const double right = c == g.cols - 1 ? bleed : inner(g.colGaps.value(c));
    const double top = r == 0 ? bleed : inner(g.rowGaps.value(r - 1));
    const double bottom = r == g.rows - 1 ? bleed : inner(g.rowGaps.value(r));
    return {left, top, right, bottom};
}

void addMarks(Side& side, const QSizeF& sheet, const Grid& g, const Settings& s)
{
    const QList<QRectF> rects = cells(sheet, g);
    if (rects.isEmpty())
        return;
    QRectF block = rects.first();
    for (const QRectF& r : rects)
        block = block.united(r);
    const double out = s.bleed + s.markOffset;
    // Schnittlinien: alle Zellkanten ausser Falzen ohne Abstand
    std::set<double> xs, ys;
    for (int c = 0; c < g.cols; ++c) {
        const QRectF& r = rects[c];
        const bool spineLeft = c > 0 && g.spines.contains(c - 1);
        const bool spineRight = g.spines.contains(c);
        if (!spineLeft)
            xs.insert(std::round(r.left() * 1000) / 1000);
        if (!spineRight)
            xs.insert(std::round(r.right() * 1000) / 1000);
    }
    for (int row = 0; row < g.rows; ++row) {
        const QRectF& r = rects[row * g.cols];
        const bool foldAbove = row > 0 && g.headFolds.contains(row - 1) && g.rowGaps.value(row - 1) <= 0;
        const bool foldBelow = g.headFolds.contains(row) && g.rowGaps.value(row) <= 0;
        if (!foldAbove)
            ys.insert(std::round(r.top() * 1000) / 1000);
        if (!foldBelow)
            ys.insert(std::round(r.bottom() * 1000) / 1000);
    }
    if (s.cropMarks) {
        for (double x : xs) {
            side.cropMarks << QLineF(x, block.top() - out, x, block.top() - out - s.markLength);
            side.cropMarks << QLineF(x, block.bottom() + out, x, block.bottom() + out + s.markLength);
        }
        for (double y : ys) {
            side.cropMarks << QLineF(block.left() - out, y, block.left() - out - s.markLength, y);
            side.cropMarks << QLineF(block.right() + out, y, block.right() + out + s.markLength, y);
        }
    }
    if (s.foldMarks) {
        for (int i : g.spines) {
            const double x = rects[i].right() + g.colGaps.value(i) / 2;
            side.foldMarks << QLineF(x, block.top() - out, x, block.top() - out - s.markLength);
            side.foldMarks << QLineF(x, block.bottom() + out, x, block.bottom() + out + s.markLength);
        }
        for (int i : g.headFolds) {
            const double y = rects[i * g.cols].bottom() + g.rowGaps.value(i) / 2;
            side.foldMarks << QLineF(block.left() - out, y, block.left() - out - s.markLength, y);
            side.foldMarks << QLineF(block.right() + out, y, block.right() + out + s.markLength, y);
        }
    }
}

// Rückseite für den Drucker: Die Anordnung ist so gerechnet, als würde das Blatt um die
// senkrechte Achse gewendet (wie eine Buchseite). Wendet der Drucker um die waagrechte Achse,
// steht die Rückseite auf dem Kopf -> um 180 Grad drehen.
void turnForPrinter(Side& side, const QSizeF& sheet, const Settings& s)
{
    const bool landscape = sheet.width() > sheet.height();
    const bool aroundHorizontal = landscape ? s.flip == Flip::LongEdge : s.flip == Flip::ShortEdge;
    if (!aroundHorizontal)
        return;
    auto turn = [&](const QPointF& p) { return QPointF(sheet.width() - p.x(), sheet.height() - p.y()); };
    for (Placement& p : side.pages) {
        p.trim = QRectF(turn(p.trim.bottomRight()), p.trim.size());
        p.rotation = (p.rotation + 180) % 360;
        p.bleed = QMarginsF(p.bleed.right(), p.bleed.bottom(), p.bleed.left(), p.bleed.top());
        p.shift = -p.shift;
    }
    for (QList<QLineF>* lines : {&side.cropMarks, &side.foldMarks})
        for (QLineF& l : *lines)
            l = QLineF(turn(l.p1()), turn(l.p2()));
}

// Papier hoch oder quer: wo der Block passt; notfalls randlos (tight)
QSizeF orient(const Settings& s, const Grid& g, bool* ok, bool* tight)
{
    const QSizeF portrait(std::min(s.paper.width(), s.paper.height()), std::max(s.paper.width(), s.paper.height()));
    const QSizeF landscape = portrait.transposed();
    *ok = true;
    *tight = false;
    for (bool strict : {true, false}) {
        if (fits(portrait, g, s, strict))
            return portrait;
        if (fits(landscape, g, s, strict))
            return landscape;
        *tight = true;
    }
    *ok = false;
    return portrait;
}

int maxFit(double available, double cell, double gap)
{
    return std::max(0, int(std::floor((available + gap + 1e-6) / (cell + gap))));
}

} // namespace

Result arrange(const Settings& s, int pageCount, const QSizeF& page)
{
    Result res;
    if (pageCount <= 0 || page.isEmpty()) {
        res.error = QObject::tr("Keine Seiten.");
        return res;
    }
    Grid g;
    g.cell = page;
    int perSide = 1;
    switch (s.layout) {
    case Layout::Single:
        break;
    case Layout::NUp:
    case Layout::StepRepeat: {
        // Spalten/Zeilen fest oder so viele wie passen (Hoch- und Querformat probieren)
        int bestCols = 1, bestRows = 1;
        QSizeF bestSheet;
        for (const QSizeF& sheet : {QSizeF(std::min(s.paper.width(), s.paper.height()), std::max(s.paper.width(), s.paper.height())),
                                    QSizeF(std::max(s.paper.width(), s.paper.height()), std::min(s.paper.width(), s.paper.height()))}) {
            const double w = sheet.width() - 2 * s.margin - 2 * s.bleed, h = sheet.height() - 2 * s.margin - 2 * s.bleed;
            const int c = s.columns > 0 ? s.columns : maxFit(w, page.width(), s.gap);
            const int r = s.rows > 0 ? s.rows : maxFit(h, page.height(), s.gap);
            if (c * r > bestCols * bestRows || bestSheet.isEmpty()) {
                bestCols = std::max(1, c);
                bestRows = std::max(1, r);
                bestSheet = sheet;
            }
        }
        g.cols = bestCols;
        g.rows = bestRows;
        g.colGaps = QList<double>(g.cols - 1, s.gap);
        g.rowGaps = QList<double>(g.rows - 1, s.gap);
        perSide = g.cols * g.rows;
        break;
    }
    case Layout::Booklet:
        g.cols = 2;
        g.colGaps = {0.0};
        g.spines = {0};
        break;
    case Layout::Signature8:
        g.cols = 2;
        g.rows = 2;
        g.colGaps = {0.0};
        g.rowGaps = {s.gap};
        g.spines = {0};
        g.headFolds = {0};
        break;
    }
    bool ok = false, tight = false;
    res.sheet = orient(s, g, &ok, &tight);
    if (ok && tight)
        res.warning = QObject::tr("Die Seiten füllen das Papier ganz aus: Ränder, Anschnitt und Marken haben keinen Platz, "
                                  "und der Drucker druckt die äussersten Millimeter evtl. nicht.");
    if (!ok) {
        res.error = QObject::tr("Die Seiten (%1 x %2 mm) passen in dieser Anordnung nicht auf das Papier (%3 x %4 mm).")
                        .arg(page.width(), 0, 'f', 0)
                        .arg(page.height(), 0, 'f', 0)
                        .arg(s.paper.width(), 0, 'f', 0)
                        .arg(s.paper.height(), 0, 'f', 0);
        return res;
    }
    const QList<QRectF> rects = cells(res.sheet, g);
    auto place = [&](int page, int cell, int rotation, double shift = 0.0) {
        Placement p;
        p.page = page;
        p.trim = rects[cell];
        p.rotation = rotation;
        p.bleed = bleedFor(g, cell, s.bleed);
        p.shift = shift;
        return p;
    };
    auto newSide = [&]() {
        Side side;
        addMarks(side, res.sheet, g, s);
        return side;
    };

    switch (s.layout) {
    case Layout::Single:
    case Layout::NUp: {
        res.pageCount = pageCount;
        for (int first = 0; first < pageCount; first += perSide) {
            Sheet sheet;
            sheet.front = newSide();
            for (int i = 0; i < perSide && first + i < pageCount; ++i)
                sheet.front.pages << place(first + i, i, 0);
            res.sheets << sheet;
        }
        break;
    }
    case Layout::StepRepeat: {
        res.pageCount = pageCount;
        for (int p = 0; p < pageCount; ++p) {
            Sheet sheet;
            sheet.front = newSide();
            for (int i = 0; i < perSide; ++i)
                sheet.front.pages << place(p, i, 0);
            res.sheets << sheet;
        }
        break;
    }
    case Layout::Booklet: {
        // Auf ein Vielfaches von 4 ergänzen; Bogen k: vorn [n-1-2k | 2k], hinten [2k+1 | n-2-2k]
        const int n = (pageCount + 3) / 4 * 4;
        res.pageCount = n;
        res.blankPages = n - pageCount;
        res.duplex = true;
        const int sheets = n / 4;
        auto real = [&](int index) { return index < pageCount ? index : -1; };
        for (int k = 0; k < sheets; ++k) {
            // Bundzuwachs: innere Bogen zum Bund hin
            const double shift = sheets > 1 ? s.creep * k / (sheets - 1) : 0.0;
            Sheet sheet;
            sheet.hasBack = true;
            sheet.front = newSide();
            sheet.front.pages << place(real(n - 1 - 2 * k), 0, 0, shift) << place(real(2 * k), 1, 0, -shift);
            sheet.back = newSide();
            sheet.back.pages << place(real(2 * k + 1), 0, 0, shift) << place(real(n - 2 - 2 * k), 1, 0, -shift);
            turnForPrinter(sheet.back, res.sheet, s);
            res.sheets << sheet;
        }
        break;
    }
    case Layout::Signature8: {
        // Kreuzbruch: vorn 5↓ 4↓ / 8 1, hinten 3↓ 6↓ / 2 7 (Seiten 1..8 des Falzbogens)
        const int n = (pageCount + 7) / 8 * 8;
        res.pageCount = n;
        res.blankPages = n - pageCount;
        res.duplex = true;
        const int signatures = n / 8;
        auto real = [&](int index) { return index < pageCount ? index : -1; };
        for (int sig = 0; sig < signatures; ++sig) {
            QList<int> local;
            if (s.nested) {
                for (int i = 0; i < 4; ++i)
                    local << 4 * sig + i;
                for (int i = 0; i < 4; ++i)
                    local << n - 4 * sig - 4 + i;
            } else {
                for (int i = 0; i < 8; ++i)
                    local << 8 * sig + i;
            }
            const double shift = s.nested && signatures > 1 ? s.creep * sig / (signatures - 1) : 0.0;
            Sheet sheet;
            sheet.hasBack = true;
            sheet.front = newSide();
            sheet.front.pages << place(real(local[4]), 0, 180, shift) << place(real(local[3]), 1, 180, -shift)
                              << place(real(local[7]), 2, 0, shift) << place(real(local[0]), 3, 0, -shift);
            sheet.back = newSide();
            sheet.back.pages << place(real(local[2]), 0, 180, shift) << place(real(local[5]), 1, 180, -shift)
                             << place(real(local[1]), 2, 0, shift) << place(real(local[6]), 3, 0, -shift);
            turnForPrinter(sheet.back, res.sheet, s);
            res.sheets << sheet;
        }
        break;
    }
    }
    return res;
}

QString layoutName(Layout layout)
{
    switch (layout) {
    case Layout::Single: return QObject::tr("Einzelseiten (mit Anschnitt und Marken)");
    case Layout::NUp: return QObject::tr("Mehrfachnutzen (Seiten der Reihe nach)");
    case Layout::StepRepeat: return QObject::tr("Nutzen wiederholen (z.B. Visitenkarten)");
    case Layout::Booklet: return QObject::tr("Heft – Rückendrahtheftung (2 Seiten je Bogenseite)");
    case Layout::Signature8: return QObject::tr("Falzbogen 8 Seiten – Kreuzbruch (4 Seiten je Bogenseite)");
    }
    return {};
}

QString describe(const Result& r)
{
    if (!r.ok())
        return r.error;
    QString text = r.duplex ? QObject::tr("%n Bogen beidseitig", nullptr, int(r.sheets.size())) : QObject::tr("%n Bogen einseitig", nullptr, int(r.sheets.size()));
    text += QObject::tr(", Bogen %1 x %2 mm").arg(r.sheet.width(), 0, 'f', 0).arg(r.sheet.height(), 0, 'f', 0);
    if (r.blankPages > 0)
        text += QObject::tr(", %n Leerseite(n) ergänzt", nullptr, r.blankPages);
    return text;
}

} // namespace vek::impose

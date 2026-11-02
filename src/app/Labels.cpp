#include "Labels.h"

namespace vek {

QList<LabelStock> labelStocks()
{
    auto make = [](const char* maker, const char* name, double w, double h, int cols, int rows, double left, double top,
                   double hGap, double vGap) {
        LabelStock s;
        s.maker = QString::fromUtf8(maker);
        s.name = QString::fromUtf8(name);
        s.label = QSizeF(w, h);
        s.columns = cols;
        s.rows = rows;
        s.left = left;
        s.top = top;
        s.hGap = hGap;
        s.vGap = vGap;
        return s;
    };
    return {
        make("Avery Zweckform", "3474 (70 × 37)", 70, 37, 3, 8, 0, 0.5, 0, 0),
        make("Avery Zweckform", "3475 (70 × 36)", 70, 36, 3, 8, 0, 4.5, 0, 0),
        make("Avery Zweckform", "3422 (70 × 35)", 70, 35, 3, 8, 0, 8.5, 0, 0),
        make("Avery Zweckform", "3652 (70 × 42,3)", 70, 42.3, 3, 7, 0, 0.45, 0, 0),
        make("Avery Zweckform", "3659 (97 × 42,3)", 97, 42.3, 2, 7, 8, 0.45, 0, 0),
        make("Avery Zweckform", "3425 (105 × 57)", 105, 57, 2, 5, 0, 6, 0, 0),
        make("Avery Zweckform", "3483 (105 × 148)", 105, 148, 2, 2, 0, 0.5, 0, 0),
        make("Avery Zweckform", "L7159 (63,5 × 33,9)", 63.5, 33.9, 3, 8, 7.2, 12.9, 2.5, 0),
        make("Avery Zweckform", "L7160 (63,5 × 38,1)", 63.5, 38.1, 3, 7, 7.2, 15.1, 2.5, 0),
        make("Avery Zweckform", "L7163 (99,1 × 38,1)", 99.1, 38.1, 2, 7, 4.7, 15.1, 2.5, 0),
        make("Avery Zweckform", "L7165 (99,1 × 67,7)", 99.1, 67.7, 2, 4, 4.7, 13.1, 2.5, 0),
        make("Avery Zweckform", "L7173 (99,1 × 57)", 99.1, 57, 2, 5, 4.7, 6, 2.5, 0),
        make("Avery Zweckform", "L7651 (38,1 × 21,2)", 38.1, 21.2, 5, 13, 4.7, 10.7, 2.5, 0),
        make("Herma", "4360 (70 × 36)", 70, 36, 3, 8, 0, 4.5, 0, 0),
        make("Herma", "4453 (105 × 48)", 105, 48, 2, 6, 0, 4.5, 0, 0),
    };
}

} // namespace vek

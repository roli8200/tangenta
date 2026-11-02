#pragma once

#include <QImage>

#include <functional>

namespace vek {

// Gaussscher Weichzeichner, angenähert durch drei Kastenfilter je Richtung (wie im
// SVG-Filter feGaussianBlur üblich). image muss ARGB32_Premultiplied sein; sigma in Pixeln.
void gaussianBlur(QImage& image, double sigma);

// Verlaufende Unschärfe: sigmaAt(x, y) liefert je Pixelmitte das Sigma in Pixeln (zwischen
// minSigma und maxSigma). Gerechnet werden einige Stufen, dazwischen wird gemischt.
void rampBlur(QImage& image, const std::function<double(double, double)>& sigmaAt, double minSigma, double maxSigma);
// Deckkraft je Pixelmitte (0..1) anwenden; image ARGB32_Premultiplied
void applyOpacity(QImage& image, const std::function<double(double, double)>& opacityAt);

} // namespace vek

#include "Splash.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QSvgRenderer>
#include <QVBoxLayout>

#include <cmath>
#include <numbers>
#include <tuple>

namespace vek {

namespace splash {

namespace {

const QColor kBackground(0x0e, 0x11, 0x15);
const QColor kRed(0xd6, 0x26, 0x24);
const QColor kOrange(0xff, 0x8c, 0x42);
const QColor kWhite(0xf2, 0xf4, 0xf6);
const QColor kMuted(0x9a, 0xa4, 0xb2);

QColor mixColor(const QColor& a, const QColor& b, double t)
{
    return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * t), float(a.greenF() + (b.greenF() - a.greenF()) * t),
                            float(a.blueF() + (b.blueF() - a.blueF()) * t));
}

QFont font(double pixelSize, int weight, double spacing = 0.0)
{
    QFont f = QApplication::font();
    f.setPixelSize(int(std::lround(pixelSize)));
    f.setWeight(QFont::Weight(weight));
    if (spacing != 0.0)
        f.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
    return f;
}

// Rechts: Fächer aus Geraden, deren Hülle eine Kurve bildet (wie im Zeichen), darüber eine
// Bézierkurve mit Knoten und Anfassern
void drawArt(QPainter& p)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const int n = 34;
    for (int i = 0; i <= n; ++i) {
        const double t = double(i) / n;
        QColor c = mixColor(kRed, kOrange, t);
        c.setAlphaF(float(0.18 + 0.32 * std::sin(t * std::numbers::pi)));
        p.setPen(QPen(c, 1.1));
        p.drawLine(QPointF(372 + 300 * t, 420), QPointF(680, 410 - 330 * t));
    }
    QPainterPath curve;
    const QPointF a(392, 318), c1(452, 150), c2(560, 330), b(640, 132);
    curve.moveTo(a);
    curve.cubicTo(c1, c2, b);
    QLinearGradient g(a, b);
    g.setColorAt(0, kRed);
    g.setColorAt(1, kOrange);
    p.setPen(QPen(QBrush(g), 3.2, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    p.drawPath(curve);
    QPen handle(QColor(255, 255, 255, 110), 1.0, Qt::DashLine);
    p.setPen(handle);
    p.drawLine(a, c1);
    p.drawLine(b, c2);
    p.setPen(QPen(QColor(255, 255, 255, 200), 1.2));
    p.setBrush(kBackground);
    for (const QPointF& h : {c1, c2})
        p.drawEllipse(h, 4.0, 4.0);
    p.setBrush(kWhite);
    p.setPen(QPen(kBackground, 1.2));
    for (const QPointF& node : {a, b})
        p.drawRect(QRectF(node.x() - 4.5, node.y() - 4.5, 9, 9));
    p.restore();
}

} // namespace

QSize size()
{
    return {680, 420};
}

QPixmap render(qreal dpr, const QString& status)
{
    const QSize s = size();
    QPixmap pixmap(s * dpr);
    pixmap.setDevicePixelRatio(dpr);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.fillRect(QRect(QPoint(0, 0), s), kBackground);
    for (const auto& [center, radius, color] :
         {std::tuple{QPointF(520, 210), 430.0, QColor(214, 38, 36, 70)}, std::tuple{QPointF(640, 60), 260.0, QColor(255, 140, 66, 45)}}) {
        QRadialGradient glow(center, radius);
        glow.setColorAt(0, color);
        glow.setColorAt(1, QColor(0, 0, 0, 0));
        p.fillRect(QRect(QPoint(0, 0), s), glow);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(154, 164, 178, 22));
    for (int x = 12; x < s.width(); x += 24)
        for (int y = 12; y < s.height(); y += 24)
            p.drawRect(QRectF(x, y, 1.6, 1.6));
    drawArt(p);

    // Zeichen und Name
    QSvgRenderer mark(QStringLiteral(":/brand/tangenta-mark.svg"));
    mark.render(&p, QRectF(48, 52, 88, 88));
    p.setPen(kWhite);
    p.setFont(font(50, QFont::ExtraBold, 6));
    p.drawText(QPointF(156, 108), QStringLiteral("TANGENTA"));
    p.setPen(kOrange);
    p.setFont(font(13.5, QFont::DemiBold, 4.5));
    p.drawText(QPointF(159, 134), QObject::tr("VEKTORGRAFIK · LAYOUT · BILD").toUpper());

    QLinearGradient bar(48, 0, 228, 0);
    bar.setColorAt(0, kRed);
    bar.setColorAt(1, kOrange);
    p.fillRect(QRectF(48, 182, 180, 4), bar);
    p.setPen(kWhite);
    p.setFont(font(19, QFont::DemiBold));
    const QString version = QApplication::applicationVersion();
    p.drawText(QPointF(48, 222), version.isEmpty() ? QStringLiteral("TANGENTA") : QObject::tr("Version %1").arg(version));
    p.setPen(kMuted);
    p.setFont(font(14.5, QFont::Normal));
    p.drawText(QPointF(48, 248), QObject::tr("Frei und kostenlos für Windows, Linux und macOS"));

    // Fusszeile
    p.setPen(QColor(255, 255, 255, 28));
    p.drawLine(QPointF(48, 372), QPointF(s.width() - 48, 372));
    p.setFont(font(12.5, QFont::Normal));
    p.setPen(kMuted);
    const QRectF foot(48, 380, s.width() - 96, 24);
    p.drawText(foot, Qt::AlignLeft | Qt::AlignVCenter, status);
    p.drawText(foot, Qt::AlignRight | Qt::AlignVCenter, QObject::tr("© Intersoft Networks · GPLv3"));
    p.setPen(QColor(255, 255, 255, 20));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(0.5, 0.5, s.width() - 1, s.height() - 1));
    return pixmap;
}

} // namespace splash

SplashScreen::SplashScreen()
{
    const QScreen* screen = QGuiApplication::primaryScreen();
    setPixmap(splash::render(screen ? screen->devicePixelRatio() : 1.0));
}

void SplashScreen::setStatus(const QString& status)
{
    setPixmap(splash::render(devicePixelRatioF(), status));
    repaint();
    QApplication::processEvents();
}

void showAbout(QWidget* parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Über TANGENTA"));
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(0, 0, 0, 12);
    auto* image = new QLabel(&dialog);
    image->setObjectName(QStringLiteral("splash"));
    image->setPixmap(splash::render(dialog.devicePixelRatioF(), QStringLiteral("tangenta.ch")));
    layout->addWidget(image);
    auto* text = new QLabel(&dialog);
    text->setObjectName(QStringLiteral("aboutText"));
    text->setTextFormat(Qt::RichText);
    text->setOpenExternalLinks(true);
    text->setWordWrap(true);
    text->setContentsMargins(20, 8, 20, 0);
    text->setText(QObject::tr("<p><b>TANGENTA %1</b> – Vektorgrafik, Layout und Bildbearbeitung.<br>"
                              "Qt %2 · %3</p>"
                              "<p>Freie Software unter der GNU General Public License, Version 3.<br>"
                              "© Intersoft Networks, Frauenfeld</p>"
                              "<p><a href=\"https://tangenta.ch\">tangenta.ch</a> · "
                              "<a href=\"https://github.com/roli8200/tangenta\">Quellcode auf GitHub</a></p>")
                      .arg(QApplication::applicationVersion(), QString::fromLatin1(qVersion()), QSysInfo::prettyProductName()));
    layout->addWidget(text);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    buttons->setContentsMargins(20, 0, 20, 0);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    dialog.exec();
}

} // namespace vek

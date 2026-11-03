#include "Language.h"
#include "MainWindow.h"
#include "UiScale.h"

#include <QApplication>
#include <QIcon>
#include <QTimer>

int main(int argc, char* argv[])
{
    QApplication::setOrganizationName(QStringLiteral("Tangenta"));
    QApplication::setApplicationName(QStringLiteral("Tangenta"));
    vek::applyInterfaceScale();
    QApplication app(argc, argv);
    QApplication::setApplicationDisplayName(QStringLiteral("TANGENTA"));
    QApplication::setDesktopFileName(QStringLiteral("tangenta"));
    QApplication::setApplicationVersion(QStringLiteral(VEK_VERSION));

    // Oberfläche in der gewählten Sprache (Ansicht > Sprache), sonst in der des Systems
    vek::installTranslations();

    // Kleine Grössen haben eigene, kräftigere Schnitte; das SVG deckt alles Grössere ab
    QIcon icon(QStringLiteral(":/brand/tangenta.svg"));
    for (int size : {16, 24, 32, 48, 64, 128, 256})
        icon.addFile(QStringLiteral(":/brand/tangenta-%1.png").arg(size), QSize(size, size));
    QApplication::setWindowIcon(icon);

    vek::MainWindow window;
    const QStringList args = QApplication::arguments();
    window.show();
    if (args.size() > 1)
        window.openFile(args.at(1));
    else
        QTimer::singleShot(0, &window, [&window] { window.applyStartup(); });
    return QApplication::exec();
}

#include "Language.h"
#include "MainWindow.h"
#include "Splash.h"
#include "TangentaStyle.h"
#include "UiScale.h"

#include <QApplication>
#include <QElapsedTimer>

#include <algorithm>
#include <QFileInfo>
#include <QFileOpenEvent>
#include <QIcon>
#include <QTimer>

int main(int argc, char* argv[])
{
    QApplication::setOrganizationName(QStringLiteral("Tangenta"));
    QApplication::setApplicationName(QStringLiteral("Tangenta"));
    vek::applyInterfaceScale();
    QApplication app(argc, argv);
    QApplication::setStyle(new vek::TangentaStyle);
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

    // Startbild, solange das Hauptfenster aufgebaut wird (mindestens kurz, damit es nicht nur aufblitzt)
    QElapsedTimer started;
    started.start();
    vek::SplashScreen splash;
    splash.show();
    splash.setStatus(QObject::tr("Wird gestartet …"));
    vek::MainWindow window;
    const QStringList args = QApplication::arguments();
    if (args.size() > 1)
        splash.setStatus(QObject::tr("Öffne %1 …").arg(QFileInfo(args.at(1)).fileName()));
    window.show();
    QTimer::singleShot(std::max<qint64>(0, 1200 - started.elapsed()), &splash, [&splash, &window] { splash.finish(&window); });
#ifdef Q_OS_MACOS
    // Finder übergibt Dateien (Doppelklick, aufs Dock-Symbol ziehen) als Ereignis statt als Argument
    struct FileOpenFilter : QObject {
        vek::MainWindow* window = nullptr;
        bool opened = false;
        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (event->type() != QEvent::FileOpen)
                return QObject::eventFilter(watched, event);
            opened = true;
            window->openFile(static_cast<QFileOpenEvent*>(event)->file());
            return true;
        }
    } fileOpen;
    fileOpen.window = &window;
    app.installEventFilter(&fileOpen);
#endif
    if (args.size() > 1)
        window.openFile(args.at(1));
    else
        QTimer::singleShot(0, &window, [&] {
#ifdef Q_OS_MACOS
            if (fileOpen.opened)
                return;
#endif
            window.applyStartup();
        });
    return QApplication::exec();
}

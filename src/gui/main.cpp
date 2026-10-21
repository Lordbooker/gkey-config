// gkey-config - configuration tool for the gaming keys of Logitech G keyboards.

#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QLibraryInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLocale>
#include <QTimer>
#include <QTranslator>

namespace {

// Single instance: a second start asks the running instance to show its window.
QString instanceSocketName()
{
    return QStringLiteral("gkey-config-") + qEnvironmentVariable("USER");
}

bool notifyRunningInstance()
{
    QLocalSocket socket;
    socket.connectToServer(instanceSocketName());
    if (!socket.waitForConnected(300))
        return false;
    socket.write("show\n");
    socket.waitForBytesWritten(300);
    return true;
}

void installTranslations(QApplication &app)
{
    // Qt's own texts (standard buttons) ...
    auto *qtTranslator = new QTranslator(&app);
    if (qtTranslator->load(QLocale(), QStringLiteral("qtbase"), QStringLiteral("_"),
                           QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(qtTranslator);
    // ... and ours, built into the binary (English is the source language).
    auto *translator = new QTranslator(&app);
    if (translator->load(QLocale(), QStringLiteral("gkey-config"), QStringLiteral("_"), QStringLiteral(":/i18n")))
        QApplication::installTranslator(translator);
}

}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("gkey-config"));
    QApplication::setOrganizationName(QStringLiteral("gkey-config"));
    QApplication::setApplicationVersion(QStringLiteral(GKEY_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("io.github.lordbooker.GKeyConfig"));
    installTranslations(app);

    QCommandLineParser parser;
    parser.setApplicationDescription(QApplication::translate("main", "Configuration of the gaming keys of Logitech G keyboards."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption trayOption(QStringLiteral("tray"),
                                        QApplication::translate("main", "Start in the system tray without a window."));
    const QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
                                              QApplication::translate("main", "Save a picture of the window and exit."),
                                              QStringLiteral("file"));
    const QCommandLineOption keyOption(QStringLiteral("show-key"),
                                       QApplication::translate("main", "With --screenshot: select this key."),
                                       QStringLiteral("key"));
    const QCommandLineOption tabOption(QStringLiteral("show-tab"),
                                       QApplication::translate("main", "With --screenshot: show this tab (0-4)."),
                                       QStringLiteral("tab"));
    parser.addOption(trayOption);
    parser.addOption(screenshotOption);
    parser.addOption(keyOption);
    parser.addOption(tabOption);
    parser.process(app);

    if (parser.isSet(screenshotOption)) {
        MainWindow window;
        window.show();
        window.showKey(parser.value(keyOption), parser.value(tabOption).toInt());
        QTimer::singleShot(300, &app, [&] {
            window.saveScreenshot(parser.value(screenshotOption));
            app.quit();
        });
        return app.exec();
    }

    if (notifyRunningInstance())
        return 0;
    QLocalServer::removeServer(instanceSocketName());
    QLocalServer server;
    server.listen(instanceSocketName());

    QApplication::setQuitOnLastWindowClosed(false);
    MainWindow window;
    QObject::connect(&server, &QLocalServer::newConnection, &window, [&] {
        while (QLocalSocket *socket = server.nextPendingConnection()) {
            socket->deleteLater();
            window.showWindow();
        }
    });

    if (parser.isSet(trayOption)) {
        // Desktops may start the tray (StatusNotifier host) after the autostart;
        // wait up to 30 seconds for it, then give up quietly.
        auto *timer = new QTimer(&app);
        int attempts = 0;
        QObject::connect(timer, &QTimer::timeout, &app, [&, timer] {
            if (window.setupTray())
                timer->stop();
            else if (++attempts >= 30)
                app.quit();
        });
        if (!window.setupTray())
            timer->start(1000);
    } else {
        window.setupTray();
        window.showWindow();
    }
    return app.exec();
}

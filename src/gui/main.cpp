// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

// gkey-config - configuration tool for the gaming keys of Logitech G keyboards.

#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QLibraryInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPalette>
#include <QStyleFactory>
#include <QLocale>
#include <QStandardPaths>
#include <QTimer>
#include <QTranslator>

#include <unistd.h>

namespace {

// Single instance: a second start asks the running instance to show its window.
// The socket lives in the user's runtime directory ($XDG_RUNTIME_DIR, only
// accessible by the user), not in the shared /tmp where another user could
// take the name first.
QString instanceSocketName()
{
    const QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (!runtimeDir.isEmpty())
        return runtimeDir + QStringLiteral("/gkey-config.socket");
    return QStringLiteral("gkey-config-%1").arg(::getuid());   // fallback, restricted below
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

// Dark palette (colours of KDE Breeze Dark) for screenshots of the dark mode,
// independent of the desktop the screenshot is taken on.
void applyDarkPalette()
{
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    const QColor window(0x20, 0x23, 0x26);
    const QColor base(0x14, 0x16, 0x18);
    const QColor button(0x29, 0x2c, 0x30);
    const QColor text(0xfc, 0xfc, 0xfc);
    const QColor disabled(0x6e, 0x71, 0x75);
    const QColor highlight(0x3d, 0xae, 0xe9);
    QPalette palette;
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, window);
    palette.setColor(QPalette::ToolTipBase, window);
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::PlaceholderText, disabled);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, button);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::Light, QColor(0x3a, 0x3e, 0x43));
    palette.setColor(QPalette::Midlight, QColor(0x31, 0x35, 0x39));
    palette.setColor(QPalette::Mid, QColor(0x1e, 0x20, 0x23));
    palette.setColor(QPalette::Dark, QColor(0x10, 0x11, 0x13));
    palette.setColor(QPalette::Shadow, Qt::black);
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::HighlightedText, text);
    palette.setColor(QPalette::Link, highlight);
    for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, disabled);
    QApplication::setPalette(palette);
}

}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("gkey-config"));
    QApplication::setOrganizationName(QStringLiteral("gkey-config"));
    QApplication::setApplicationVersion(QStringLiteral(GKEY_VERSION));
    // The desktop file name is also the Wayland app ID; Qt registers it with
    // the XDG desktop portal, which fails (with a warning) when the desktop
    // file is not installed, e.g. when started from the build directory.
    const QString desktopFile = QStringLiteral("io.github.lordbooker.GKeyConfig");
    if (!QStandardPaths::locate(QStandardPaths::ApplicationsLocation, desktopFile + QStringLiteral(".desktop")).isEmpty())
        QApplication::setDesktopFileName(desktopFile);
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
    const QCommandLineOption modelOption(QStringLiteral("show-model"),
                                         QApplication::translate("main", "With --screenshot: show this device model."),
                                         QStringLiteral("model"));
    const QCommandLineOption darkOption(QStringLiteral("dark"),
                                        QApplication::translate("main", "With --screenshot: use a dark colour scheme."));
    parser.addOption(trayOption);
    parser.addOption(darkOption);
    parser.addOption(modelOption);
    parser.addOption(screenshotOption);
    parser.addOption(keyOption);
    parser.addOption(tabOption);
    parser.process(app);

    if (parser.isSet(screenshotOption)) {
        if (parser.isSet(darkOption))
            applyDarkPalette();
        MainWindow window;
        window.show();
        if (parser.isSet(modelOption))
            window.showModel(parser.value(modelOption));
        window.showKey(parser.value(keyOption), parser.value(tabOption).toInt());
        QTimer::singleShot(300, &app, [&] {
            window.saveScreenshot(parser.value(screenshotOption));
            QCoreApplication::quit();
        });
        return QCoreApplication::exec();
    }

    if (notifyRunningInstance())
        return 0;
    QLocalServer::removeServer(instanceSocketName());
    QLocalServer server;
    server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!server.listen(instanceSocketName()))
        qWarning("single instance disabled: %s", qPrintable(server.errorString()));

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
                QCoreApplication::quit();
        });
        if (!window.setupTray())
            timer->start(1000);
    } else {
        window.setupTray();
        window.showWindow();
    }
    return QCoreApplication::exec();
}

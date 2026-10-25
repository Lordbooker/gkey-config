// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

// gkeyd - user session daemon for Logitech G keyboards driven by hid-lg-g15.

#include "DBusService.h"
#include "Daemon.h"
#include "ModelRegistry.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QSocketNotifier>
#include <QTextStream>

#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

namespace {

int signalPipe[2] = {-1, -1};

void onSignal(int)
{
    const char byte = 1;
    [[maybe_unused]] const ssize_t written = ::write(signalPipe[1], &byte, 1);
}

// SIGTERM/SIGINT end the event loop normally, so all grabs are released and
// held keys are let go before the process exits.
void installSignalHandlers(QCoreApplication &app)
{
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, signalPipe) != 0)
        return;
    auto *notifier = new QSocketNotifier(signalPipe[0], QSocketNotifier::Read, &app);
    QObject::connect(notifier, &QSocketNotifier::activated, &app, [] {
        char byte;
        [[maybe_unused]] const ssize_t got = ::read(signalPipe[0], &byte, 1);
        QCoreApplication::quit();
    });
    struct sigaction action = {};
    action.sa_handler = onSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &action, nullptr);
    sigaction(SIGINT, &action, nullptr);
}

int listModels()
{
    ModelRegistry registry;
    registry.load();
    QTextStream out(stdout);
    for (const DeviceModel &model : registry.models()) {
        QStringList ids;
        for (const DeviceModel::UsbId &usb : model.usbIds())
            ids.append(QStringLiteral("%1:%2").arg(usb.vendor, 4, 16, QChar(u'0'))
                                              .arg(usb.product, 4, 16, QChar(u'0')));
        out << model.id() << '\t' << model.name() << '\t' << ids.join(u',')
            << (model.tested() ? "" : "\t(untested)") << '\n';
    }
    for (const QString &error : registry.errors())
        out << "invalid model file: " << error << '\n';
    return registry.errors().isEmpty() ? 0 : 1;
}

}

int main(int argc, char *argv[])
{
    qSetMessagePattern(QStringLiteral("%{if-category}%{category}: %{endif}%{message}"));

    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("gkeyd"));
    QCoreApplication::setApplicationVersion(QStringLiteral(GKEY_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Daemon for the gaming keys of Logitech G keyboards (hid-lg-g15)."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption listOption(QStringLiteral("list-models"),
                                        QStringLiteral("List the supported models and exit."));
    parser.addOption(listOption);
    parser.process(app);

    if (parser.isSet(listOption))
        return listModels();

    installSignalHandlers(app);

    Daemon daemon;
    QString error;
    // Claim the bus name first: a second daemon stops here, before it touches any device.
    auto *dbus = new DBusService(&daemon);
    if (!dbus->registerOnSessionBus(&error)) {
        qCritical("%s", qPrintable(error));
        return 1;
    }
    if (!daemon.start(&error)) {
        qCritical("%s", qPrintable(error));
        return 1;
    }
    return QCoreApplication::exec();
}

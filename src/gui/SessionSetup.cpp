// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "SessionSetup.h"

#include "Autostart.h"

#include <QCoreApplication>
#include <QProcess>
#include <QStandardPaths>

namespace {

const QString Unit = QStringLiteral("gkeyd.service");

QString trayExec()
{
    // Installed: rely on PATH like the system-wide entry; otherwise the full path.
    const QString installed = QStandardPaths::findExecutable(QStringLiteral("gkey-config"));
    const QString self = QCoreApplication::applicationFilePath();
    const QString program = installed == self ? QStringLiteral("gkey-config") : self;
    return program + QStringLiteral(" --tray");
}

}

SessionSetup::SessionSetup(QObject *parent) : QObject(parent) {}

void SessionSetup::runSystemctl(const QStringList &arguments, const std::function<void(int, const QString &)> &done)
{
    // Deleted by deleteLater() when it has finished or failed to start, and
    // owned by this object until then; the analyser does not follow that.
    // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [process, done](int exitCode, QProcess::ExitStatus status) {
        const QString output = QString::fromLocal8Bit(process->readAllStandardOutput()).trimmed();
        const QString errors = QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
        process->deleteLater();
        done(status == QProcess::NormalExit ? exitCode : -1, output.isEmpty() ? errors : output);
    });
    connect(process, &QProcess::errorOccurred, this, [process, done](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        process->deleteLater();
        done(-1, process->errorString());
    });
    process->start(QStringLiteral("systemctl"), QStringList{QStringLiteral("--user")} + arguments);
    // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

void SessionSetup::runDaemonCommand(const QString &verb, const std::function<void(int, const QString &)> &done)
{
    runSystemctl({verb, Unit}, done);
}

void SessionSetup::refreshDaemonAutostart()
{
    runSystemctl({QStringLiteral("is-enabled"), Unit}, [this](int, const QString &output) {
        // is-enabled prints the state; exit code 1 for disabled, masked and unknown units.
        const QString state = output.section(u'\n', 0, 0).trimmed();
        DaemonAutostart value = DaemonAutostart::NotInstalled;
        // "not-found" or no output (no user manager): NotInstalled.
        if (state.startsWith(u"enabled") || state.startsWith(u"linked") || state == u"alias")
            value = DaemonAutostart::Enabled;
        else if (state == u"disabled" || state == u"indirect")
            value = DaemonAutostart::Disabled;
        else if (state.startsWith(u"masked"))
            value = DaemonAutostart::Masked;
        m_daemonAutostart = value;
        emit daemonAutostartChanged(m_daemonAutostart);
    });
}

void SessionSetup::setDaemonAutostart(bool enabled)
{
    auto finish = [this](int exitCode, const QString &output) {
        if (exitCode != 0)
            emit failed(output);
        refreshDaemonAutostart();
    };
    if (!enabled) {
        runSystemctl({QStringLiteral("disable"), Unit}, finish);
        return;
    }
    auto enable = [this, finish] { runSystemctl({QStringLiteral("enable"), QStringLiteral("--now"), Unit}, finish); };
    if (m_daemonAutostart == DaemonAutostart::Masked) {
        runSystemctl({QStringLiteral("unmask"), Unit}, [this, enable](int exitCode, const QString &output) {
            if (exitCode != 0) {
                emit failed(output);
                refreshDaemonAutostart();
                return;
            }
            enable();
        });
        return;
    }
    enable();
}

bool SessionSetup::trayAutostart() const
{
    return Autostart::isEnabled(QString::fromLatin1(TrayDesktopFile));
}

bool SessionSetup::setTrayAutostart(bool enabled, QString *error)
{
    return Autostart::setEnabled(QString::fromLatin1(TrayDesktopFile), enabled, tr("G-Key Configuration (tray icon)"),
                                 trayExec(), Autostart::defaultPaths(), error);
}

GroupAccess::State SessionSetup::ledGroupState() const
{
    return GroupAccess::state(QString::fromLatin1(LedGroup));
}

void SessionSetup::requestLedAccess()
{
    const QString pkexec = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
    const QString group = QString::fromLatin1(LedGroup);
    const QString user = GroupAccess::userName();
    const QString manual = QStringLiteral("sudo usermod -aG %1 %2").arg(group, user);
    if (pkexec.isEmpty()) {
        emit ledAccessFinished(false, tr("pkexec (polkit) is not installed. Run in a terminal: %1").arg(manual));
        return;
    }

    QStringList arguments;
    if (GroupAccess::state(group) == GroupAccess::State::Missing) {
        // Normally created by the package (sysusers.d); not for a manual installation.
        arguments = {
            QStringLiteral("/bin/sh"),
            QStringLiteral("-c"),
            QStringLiteral("getent group \"$1\" >/dev/null || groupadd -r \"$1\" && usermod -aG \"$1\" \"$2\""),
            QStringLiteral("gkey-config"),
            group,
            user};
    } else {
        const QString usermod = QStandardPaths::findExecutable(
            QStringLiteral("usermod"),
            {QStringLiteral("/usr/sbin"), QStringLiteral("/usr/bin"), QStringLiteral("/sbin")});
        arguments = {usermod.isEmpty() ? QStringLiteral("/usr/sbin/usermod") : usermod, QStringLiteral("-aG"), group,
                     user};
    }

    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::finished, this, [this, process, manual](int exitCode, QProcess::ExitStatus status) {
        const QString output = QString::fromLocal8Bit(process->readAll()).trimmed();
        process->deleteLater();
        if (status == QProcess::NormalExit && exitCode == 0) {
            emit ledAccessFinished(true, QString());
        } else if (exitCode == 126 || exitCode == 127) {
            // pkexec: authorization dismissed or failed (no agent, wrong password).
            emit ledAccessFinished(false, tr("Not authorized. You can also run in a terminal: %1").arg(manual));
        } else {
            emit ledAccessFinished(false, tr("Adding to the group failed: %1").arg(output));
        }
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, manual](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        process->deleteLater();
        emit ledAccessFinished(false, tr("pkexec could not be started. Run in a terminal: %1").arg(manual));
    });
    process->start(pkexec, arguments);
}

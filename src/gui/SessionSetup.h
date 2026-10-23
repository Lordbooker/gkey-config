// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "GroupAccess.h"

#include <QObject>
#include <QStringList>

#include <functional>

// Per-user setup of the session: daemon autostart (systemd user unit), tray
// icon autostart (XDG autostart) and LED access (group "gkey" via pkexec).
class SessionSetup : public QObject {
    Q_OBJECT
public:
    enum class DaemonAutostart { Unknown, Enabled, Disabled, Masked, NotInstalled };

    static constexpr const char *LedGroup = "gkey";
    static constexpr const char *TrayDesktopFile = "gkey-config-tray.desktop";

    explicit SessionSetup(QObject *parent = nullptr);

    DaemonAutostart daemonAutostart() const { return m_daemonAutostart; }
    // Asks systemd (asynchronously); emits daemonAutostartChanged.
    void refreshDaemonAutostart();
    // On: unmask if needed, "enable --now". Off: "disable" (a running daemon keeps running).
    void setDaemonAutostart(bool enabled);

    bool trayAutostart() const;
    bool setTrayAutostart(bool enabled, QString *error);

    GroupAccess::State ledGroupState() const;
    // Adds the user to the group "gkey" through pkexec (asks for the password);
    // emits ledAccessFinished.
    void requestLedAccess();

    // Runs "systemctl --user <verb> gkeyd.service" (start, stop, restart);
    // done gets the exit code (-1 if systemctl could not run) and its output.
    void runDaemonCommand(const QString &verb, std::function<void(int, const QString &)> done);

signals:
    void daemonAutostartChanged(SessionSetup::DaemonAutostart state);
    void failed(const QString &message);
    void ledAccessFinished(bool success, const QString &message);

private:
    void runSystemctl(const QStringList &arguments, const std::function<void(int, const QString &)> &done);

    DaemonAutostart m_daemonAutostart = DaemonAutostart::Unknown;
};

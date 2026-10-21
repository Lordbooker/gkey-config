// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <QString>
#include <QStringList>

// Per-user switch for an XDG autostart entry (Desktop Application Autostart
// Specification): a file in the user's autostart directory overrides the
// system-wide file of the same name; "Hidden=true" there turns it off.
namespace Autostart {

struct Paths {
    QString userDir;          // $XDG_CONFIG_HOME/autostart
    QStringList systemDirs;   // $XDG_CONFIG_DIRS/autostart, default /etc/xdg/autostart
};

Paths defaultPaths();

// Whether the entry starts at login for this user.
bool isEnabled(const QString &fileName, const Paths &paths = defaultPaths());

// Turns the entry on or off for this user only. Without a system-wide entry,
// enabling writes a complete one (name, exec) to the user's directory.
bool setEnabled(const QString &fileName, bool enabled, const QString &name, const QString &exec,
                const Paths &paths = defaultPaths(), QString *error = nullptr);

}

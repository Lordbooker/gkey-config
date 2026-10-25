// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "Autostart.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

// Entry of an autostart file that is active: not hidden and not disabled for GNOME.
bool entryActive(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    bool inEntry = false;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith(u'[')) {
            inEntry = line == u"[Desktop Entry]";
            continue;
        }
        if (!inEntry)
            continue;
        const qsizetype equals = line.indexOf(u'=');
        if (equals <= 0)
            continue;
        const QString key = line.left(equals).trimmed();
        const QString value = line.mid(equals + 1).trimmed();
        if (key == u"Hidden" && value == u"true")
            return false;
        if (key == u"X-GNOME-Autostart-enabled" && value == u"false")
            return false;
    }
    return true;
}

QString systemFile(const QString &fileName, const Autostart::Paths &paths)
{
    for (const QString &dir : paths.systemDirs) {
        const QString path = QDir(dir).filePath(fileName);
        if (QFileInfo::exists(path))
            return path;
    }
    return {};
}

bool writeFile(const QString &path, const QString &content, QString *error)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text) || file.write(content.toUtf8()) < 0 || !file.commit()) {
        if (error)
            *error = QStringLiteral("%1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

}

namespace Autostart {

Paths defaultPaths()
{
    Paths paths;
    paths.userDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/autostart");
    QString configDirs = qEnvironmentVariable("XDG_CONFIG_DIRS");
    if (configDirs.isEmpty())
        configDirs = QStringLiteral("/etc/xdg");
    for (const QString &dir : configDirs.split(u':', Qt::SkipEmptyParts))
        paths.systemDirs.append(dir + QStringLiteral("/autostart"));
    return paths;
}

bool isEnabled(const QString &fileName, const Paths &paths)
{
    const QString userFile = QDir(paths.userDir).filePath(fileName);
    if (QFileInfo::exists(userFile))
        return entryActive(userFile);
    const QString system = systemFile(fileName, paths);
    return !system.isEmpty() && entryActive(system);
}

bool setEnabled(const QString &fileName, bool enabled, const QString &name, const QString &exec, const Paths &paths,
                QString *error)
{
    const QString userFile = QDir(paths.userDir).filePath(fileName);
    const QString system = systemFile(fileName, paths);

    if (QFileInfo::exists(userFile) && !QFile::remove(userFile)) {
        if (error)
            *error = QStringLiteral("%1: cannot remove the file").arg(userFile);
        return false;
    }
    const bool systemActive = !system.isEmpty() && entryActive(system);
    if (enabled == systemActive)
        return true; // the system-wide entry already does what is wanted

    QString content = QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\nExec=%2\nIcon=gkey-config\n"
                                     "Terminal=false\nNoDisplay=true\n")
                          .arg(name, exec);
    content += enabled ? QStringLiteral("X-GNOME-Autostart-enabled=true\n") : QStringLiteral("Hidden=true\n");
    return writeFile(userFile, content, error);
}

}

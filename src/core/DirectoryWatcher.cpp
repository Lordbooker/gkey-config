// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "DirectoryWatcher.h"

#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QRegularExpression>
#include <QTimer>

DirectoryWatcher::DirectoryWatcher(const QString &directory, QObject *parent) : QObject(parent), m_directory(directory)
{
    m_watcher = new QFileSystemWatcher(this);
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(150);

    connect(m_watcher, &QFileSystemWatcher::directoryChanged, m_debounce, qOverload<>(&QTimer::start));
    connect(m_watcher, &QFileSystemWatcher::fileChanged, m_debounce, qOverload<>(&QTimer::start));
    connect(m_debounce, &QTimer::timeout, this, &DirectoryWatcher::changed);
}

void DirectoryWatcher::setFiles(const QStringList &files)
{
    const QStringList watched = m_watcher->files() + m_watcher->directories();
    if (!watched.isEmpty())
        m_watcher->removePaths(watched);
    if (QDir(m_directory).exists())
        m_watcher->addPath(m_directory);
    if (!files.isEmpty())
        m_watcher->addPaths(files);
}

QString DirectoryWatcher::uniqueFileName(const QString &directory, const QString &name)
{
    static const QRegularExpression invalid(QStringLiteral("[^a-z0-9]+"));
    static const QRegularExpression edges(QStringLiteral("^-+|-+$"));
    QString base = name.toLower().replace(invalid, QStringLiteral("-"));
    base.remove(edges);
    if (base.isEmpty())
        base = QStringLiteral("item");

    const QDir dir(directory);
    QString candidate = dir.filePath(base + QStringLiteral(".json"));
    for (int n = 2; QFile::exists(candidate); ++n)
        candidate = dir.filePath(QStringLiteral("%1-%2.json").arg(base).arg(n));
    return candidate;
}

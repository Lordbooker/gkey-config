// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <QObject>
#include <QStringList>

class QFileSystemWatcher;
class QTimer;

// Watches a directory and the files in it and reports changes once they have
// settled (editors and QSaveFile write in several steps).
class DirectoryWatcher : public QObject {
    Q_OBJECT
public:
    explicit DirectoryWatcher(const QString &directory, QObject *parent = nullptr);

    // Files to watch in addition to the directory itself (needed for editors
    // that write in place, which only changes the file, not the directory).
    void setFiles(const QStringList &files);

    // New, unused file name "<slug>.json" in the directory for an item called
    // name (e.g. "Gaming / WoW" -> "gaming-wow.json").
    static QString uniqueFileName(const QString &directory, const QString &name);

signals:
    void changed();

private:
    QString m_directory;
    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_debounce = nullptr;
};

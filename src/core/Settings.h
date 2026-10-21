// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <QObject>
#include <QStringList>

class DirectoryWatcher;

// Global settings that apply to all profiles, in
// $XDG_CONFIG_HOME/gkey-config/settings.json:
//
//   { "lcd": { "pages": ["status", "clock", "text"] } }
//
// Reloaded automatically when the file changes.
class Settings : public QObject {
    Q_OBJECT
public:
    explicit Settings(const QString &filePath, QObject *parent = nullptr);

    void reload();
    bool save(QString *error = nullptr);

    // Enabled LCD pages in display order (known names only; at least one).
    const QStringList &lcdPages() const { return m_lcdPages; }
    void setLcdPages(const QStringList &pages);

    const QString &filePath() const { return m_filePath; }

signals:
    void changed();

private:
    QString m_filePath;
    QStringList m_lcdPages;
    DirectoryWatcher *m_watcher = nullptr;
};

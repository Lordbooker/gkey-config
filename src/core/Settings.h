// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <QObject>
#include <QStringList>

class DirectoryWatcher;

// Global settings that apply to all profiles, in
// $XDG_CONFIG_HOME/gkey-config/settings.json:
//
//   { "lcd": { "pages": ["intro", "status", "clock", "text"], "afterIntro": "clock" } }
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

    // Page shown after the start animation; empty: its final image stays. A
    // page of the rotation (not the start animation or the profile name); it
    // only takes effect while that page is enabled.
    const QString &lcdAfterIntro() const { return m_lcdAfterIntro; }
    void setLcdAfterIntro(const QString &page);

    const QString &filePath() const { return m_filePath; }

signals:
    void changed();

private:
    QString m_filePath;
    QStringList m_lcdPages;
    QString m_lcdAfterIntro;
    DirectoryWatcher *m_watcher = nullptr;
};

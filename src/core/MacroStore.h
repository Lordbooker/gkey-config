// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "Macro.h"

#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

class DirectoryWatcher;

// The shared macro library: one JSON file per macro in
// $XDG_CONFIG_HOME/gkey-config/macros/. Reloaded automatically on changes.
class MacroStore : public QObject {
    Q_OBJECT
public:
    explicit MacroStore(const QString &directory, QObject *parent = nullptr);

    void reload();

    // Macro names, sorted case-insensitively.
    QStringList names() const;

    // Macro with the given name; nullptr if unknown.
    const Macro *macro(const QString &name) const;

    // Writes the macro atomically. A macro with the same name keeps its file.
    bool save(const Macro &macro, QString *error = nullptr);

    // Deletes the macro's file.
    bool remove(const QString &name, QString *error = nullptr);

    const QStringList &messages() const { return m_messages; }
    const QString &directory() const { return m_directory; }

signals:
    void changed();

private:
    QString m_directory;
    QMap<QString, Macro> m_macros;    // name -> macro
    QMap<QString, QString> m_files;   // name -> file path
    QStringList m_messages;
    DirectoryWatcher *m_watcher = nullptr;
};

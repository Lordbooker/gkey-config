// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "JsonFileStore.h"
#include "Macro.h"

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
    QStringList names() const { return m_store.names(); }

    // Macro with the given name; nullptr if unknown.
    const Macro *macro(const QString &name) const { return m_store.item(name); }

    // Writes the macro atomically. A macro with the same name keeps its file.
    bool save(const Macro &macro, QString *error = nullptr) { return m_store.save(macro, error); }

    // Deletes the macro's file.
    bool remove(const QString &name, QString *error = nullptr) { return m_store.remove(name, error); }

    const QStringList &messages() const { return m_store.messages(); }
    const QString &directory() const { return m_store.directory(); }

signals:
    void changed();

private:
    JsonFileStore<Macro> m_store;
    DirectoryWatcher *m_watcher = nullptr;
};

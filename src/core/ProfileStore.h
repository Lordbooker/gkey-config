// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "DeviceModel.h"
#include "JsonFileStore.h"
#include "Profile.h"

#include <QObject>
#include <QString>
#include <QStringList>

class DirectoryWatcher;

// The profiles of one device model: one JSON file per profile in
// $XDG_CONFIG_HOME/gkey-config/profiles/<model>/. Changes made by other
// programs (GUI, text editor) are picked up automatically.
class ProfileStore : public QObject {
    Q_OBJECT
public:
    // directory: profile directory of the model (see ConfigPaths::profileDir).
    ProfileStore(const DeviceModel &model, const QString &directory, QObject *parent = nullptr);

    // Reads all profile files. Called by the constructor and on every change.
    void reload();

    // Profile names, sorted case-insensitively.
    QStringList names() const { return m_store.names(); }

    // Profile with the given name; nullptr if unknown.
    const Profile *profile(const QString &name) const { return m_store.item(name); }

    // Writes the profile atomically. A profile with the same name keeps its file.
    bool save(const Profile &profile, QString *error = nullptr) { return m_store.save(profile, error); }

    // Deletes the profile's file.
    bool remove(const QString &name, QString *error = nullptr) { return m_store.remove(name, error); }

    // Renames a profile; its file gets a name derived from the new name.
    bool rename(const QString &oldName, const QString &newName, QString *error = nullptr)
    {
        return m_store.rename(oldName, newName, error);
    }

    // Creates an empty profile "Default" if there is no profile at all.
    void ensureDefault();

    // Problems found by the last reload (invalid files, ignored bindings).
    QStringList messages() const;

    const QString &directory() const { return m_store.directory(); }

signals:
    // Emitted after a reload triggered by a change on disk.
    void changed();

private:
    DeviceModel m_model;   // copy: the store must not depend on the registry's lifetime
    JsonFileStore<Profile> m_store;
    QStringList m_extraMessages;   // e.g. the default profile could not be written
    DirectoryWatcher *m_watcher = nullptr;
};

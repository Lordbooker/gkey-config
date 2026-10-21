#pragma once

#include "DeviceModel.h"
#include "Profile.h"

#include <QList>
#include <QMap>
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
    QStringList names() const;

    // Profile with the given name; nullptr if unknown.
    const Profile *profile(const QString &name) const;

    // Writes the profile atomically. A profile with the same name keeps its file.
    bool save(const Profile &profile, QString *error = nullptr);

    // Deletes the profile's file.
    bool remove(const QString &name, QString *error = nullptr);

    // Renames a profile; its file gets a name derived from the new name.
    bool rename(const QString &oldName, const QString &newName, QString *error = nullptr);

    // Creates an empty profile "Default" if there is no profile at all.
    void ensureDefault();

    // Problems found by the last reload (invalid files, ignored bindings).
    const QStringList &messages() const { return m_messages; }

    const QString &directory() const { return m_directory; }

signals:
    // Emitted after a reload triggered by a change on disk.
    void changed();

private:

    DeviceModel m_model;   // copy: the store must not depend on the registry's lifetime
    QString m_directory;
    QMap<QString, Profile> m_profiles;     // name -> profile
    QMap<QString, QString> m_files;        // name -> file path
    QStringList m_messages;
    DirectoryWatcher *m_watcher = nullptr;
};

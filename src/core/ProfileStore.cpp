#include "ProfileStore.h"

#include "DeviceModel.h"
#include "DirectoryWatcher.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>

#include <algorithm>

ProfileStore::ProfileStore(const DeviceModel &model, const QString &directory, QObject *parent)
    : QObject(parent), m_model(model), m_directory(directory)
{
    QDir().mkpath(m_directory);
    m_watcher = new DirectoryWatcher(m_directory, this);
    connect(m_watcher, &DirectoryWatcher::changed, this, [this] {
        reload();
        emit changed();
    });

    reload();
}

void ProfileStore::reload()
{
    m_profiles.clear();
    m_files.clear();
    m_messages.clear();

    QDir dir(m_directory);
    if (!dir.exists())
        QDir().mkpath(m_directory);

    const QStringList files = dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QString &fileName : files) {
        const QString path = dir.filePath(fileName);
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            m_messages.append(QStringLiteral("%1: %2").arg(path, file.errorString()));
            continue;
        }
        auto profile = Profile::fromJson(file.readAll());
        if (!profile) {
            m_messages.append(QStringLiteral("%1: %2").arg(path, profile.error()));
            continue;
        }
        profile->validateFor(m_model);
        for (const QString &warning : profile->warnings())
            m_messages.append(QStringLiteral("%1: %2").arg(path, warning));
        if (m_profiles.contains(profile->name())) {
            m_messages.append(QStringLiteral("%1: profile name \"%2\" is already used by %3; file ignored")
                                  .arg(path, profile->name(), m_files.value(profile->name())));
            continue;
        }
        m_files.insert(profile->name(), path);
        m_profiles.insert(profile->name(), *profile);
    }
    m_watcher->setFiles(m_files.values());
}

QStringList ProfileStore::names() const
{
    QStringList list = m_profiles.keys();
    std::sort(list.begin(), list.end(), [](const QString &a, const QString &b) {
        return QString::compare(a, b, Qt::CaseInsensitive) < 0;
    });
    return list;
}

const Profile *ProfileStore::profile(const QString &name) const
{
    const auto it = m_profiles.constFind(name);
    return it == m_profiles.constEnd() ? nullptr : &it.value();
}

bool ProfileStore::save(const Profile &profile, QString *error)
{
    QDir().mkpath(m_directory);
    const QString path = m_files.contains(profile.name()) ? m_files.value(profile.name())
                                                          : DirectoryWatcher::uniqueFileName(m_directory, profile.name());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(profile.toJson()) < 0 || !file.commit()) {
        if (error)
            *error = QStringLiteral("%1: %2").arg(path, file.errorString());
        return false;
    }
    m_files.insert(profile.name(), path);
    m_profiles.insert(profile.name(), profile);
    return true;
}

bool ProfileStore::remove(const QString &name, QString *error)
{
    const QString path = m_files.value(name);
    if (path.isEmpty()) {
        if (error)
            *error = QStringLiteral("profile \"%1\" does not exist").arg(name);
        return false;
    }
    QFile file(path);
    if (!file.remove()) {
        if (error)
            *error = QStringLiteral("%1: %2").arg(path, file.errorString());
        return false;
    }
    m_files.remove(name);
    m_profiles.remove(name);
    return true;
}

bool ProfileStore::rename(const QString &oldName, const QString &newName, QString *error)
{
    const Profile *existing = profile(oldName);
    if (!existing) {
        if (error)
            *error = QStringLiteral("profile \"%1\" does not exist").arg(oldName);
        return false;
    }
    if (oldName == newName)
        return true;
    if (profile(newName)) {
        if (error)
            *error = QStringLiteral("a profile \"%1\" already exists").arg(newName);
        return false;
    }
    Profile renamed = *existing;
    renamed.setName(newName);
    const QString oldPath = m_files.value(oldName);
    // Write the new file first: if that fails, nothing is lost.
    if (!save(renamed, error))
        return false;
    m_files.remove(oldName);
    m_profiles.remove(oldName);
    QFile::remove(oldPath);
    return true;
}

void ProfileStore::ensureDefault()
{
    if (!m_profiles.isEmpty())
        return;
    Profile profile;
    profile.setName(QStringLiteral("Default"));
    QString error;
    if (!save(profile, &error))
        m_messages.append(error);
}


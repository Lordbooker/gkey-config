#include "ProfileStore.h"

#include "DeviceModel.h"

#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>

#include <algorithm>

ProfileStore::ProfileStore(const DeviceModel &model, const QString &directory, QObject *parent)
    : QObject(parent), m_model(model), m_directory(directory)
{
    m_watcher = new QFileSystemWatcher(this);
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(150);   // editors and QSaveFile write in several steps

    connect(m_watcher, &QFileSystemWatcher::directoryChanged, m_debounce, qOverload<>(&QTimer::start));
    connect(m_watcher, &QFileSystemWatcher::fileChanged, m_debounce, qOverload<>(&QTimer::start));
    connect(m_debounce, &QTimer::timeout, this, [this] {
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
    watch();
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
                                                          : fileNameFor(profile.name());
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

void ProfileStore::watch()
{
    const QStringList watched = m_watcher->files() + m_watcher->directories();
    if (!watched.isEmpty())
        m_watcher->removePaths(watched);
    if (QDir(m_directory).exists())
        m_watcher->addPath(m_directory);
    // Editors that write in place only trigger fileChanged.
    const QStringList files = m_files.values();
    if (!files.isEmpty())
        m_watcher->addPaths(files);
}

QString ProfileStore::fileNameFor(const QString &profileName) const
{
    static const QRegularExpression invalid(QStringLiteral("[^a-z0-9]+"));
    QString base = profileName.toLower().replace(invalid, QStringLiteral("-"));
    base.remove(QRegularExpression(QStringLiteral("^-+|-+$")));
    if (base.isEmpty())
        base = QStringLiteral("profile");

    const QDir dir(m_directory);
    QString candidate = dir.filePath(base + QStringLiteral(".json"));
    for (int n = 2; QFile::exists(candidate); ++n)
        candidate = dir.filePath(QStringLiteral("%1-%2.json").arg(base).arg(n));
    return candidate;
}

// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "ProfileStore.h"

#include "DirectoryWatcher.h"

ProfileStore::ProfileStore(const DeviceModel &model, const QString &directory, QObject *parent)
    : QObject(parent), m_model(model),
      m_store(directory, [this](Profile &profile, const QString &path, QStringList &messages) {
          profile.validateFor(m_model);
          for (const QString &warning : profile.warnings())
              messages.append(QStringLiteral("%1: %2").arg(path, warning));
      })
{
    m_watcher = new DirectoryWatcher(m_store.directory(), this);
    connect(m_watcher, &DirectoryWatcher::changed, this, [this] {
        reload();
        emit changed();
    });
    reload();
}

void ProfileStore::reload()
{
    m_extraMessages.clear();
    m_store.load();
    m_watcher->setFiles(m_store.files());
}

void ProfileStore::ensureDefault()
{
    if (!m_store.isEmpty())
        return;
    Profile profile;
    profile.setName(QStringLiteral("Default"));
    QString error;
    if (!save(profile, &error))
        m_extraMessages.append(error);
}

QStringList ProfileStore::messages() const
{
    return m_store.messages() + m_extraMessages;
}

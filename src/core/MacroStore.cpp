// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "MacroStore.h"

#include "DirectoryWatcher.h"

MacroStore::MacroStore(const QString &directory, QObject *parent) : QObject(parent), m_store(directory)
{
    m_watcher = new DirectoryWatcher(m_store.directory(), this);
    connect(m_watcher, &DirectoryWatcher::changed, this, [this] {
        reload();
        emit changed();
    });
    reload();
}

void MacroStore::reload()
{
    m_store.load();
    m_watcher->setFiles(m_store.files());
}

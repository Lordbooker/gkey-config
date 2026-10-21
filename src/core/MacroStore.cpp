#include "MacroStore.h"

#include "DirectoryWatcher.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>

#include <algorithm>

MacroStore::MacroStore(const QString &directory, QObject *parent) : QObject(parent), m_directory(directory)
{
    QDir().mkpath(m_directory);
    m_watcher = new DirectoryWatcher(m_directory, this);
    connect(m_watcher, &DirectoryWatcher::changed, this, [this] {
        reload();
        emit changed();
    });
    reload();
}

void MacroStore::reload()
{
    m_macros.clear();
    m_files.clear();
    m_messages.clear();

    const QDir dir(m_directory);
    const QStringList files = dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QString &fileName : files) {
        const QString path = dir.filePath(fileName);
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            m_messages.append(QStringLiteral("%1: %2").arg(path, file.errorString()));
            continue;
        }
        auto macro = Macro::fromJson(file.readAll());
        if (!macro) {
            m_messages.append(QStringLiteral("%1: %2").arg(path, macro.error()));
            continue;
        }
        if (m_macros.contains(macro->name())) {
            m_messages.append(QStringLiteral("%1: macro name \"%2\" is already used by %3; file ignored")
                                  .arg(path, macro->name(), m_files.value(macro->name())));
            continue;
        }
        m_files.insert(macro->name(), path);
        m_macros.insert(macro->name(), *macro);
    }
    m_watcher->setFiles(m_files.values());
}

QStringList MacroStore::names() const
{
    QStringList list = m_macros.keys();
    std::sort(list.begin(), list.end(), [](const QString &a, const QString &b) {
        return QString::compare(a, b, Qt::CaseInsensitive) < 0;
    });
    return list;
}

const Macro *MacroStore::macro(const QString &name) const
{
    const auto it = m_macros.constFind(name);
    return it == m_macros.constEnd() ? nullptr : &it.value();
}

bool MacroStore::save(const Macro &macro, QString *error)
{
    QDir().mkpath(m_directory);
    const QString path = m_files.contains(macro.name()) ? m_files.value(macro.name())
                                                        : DirectoryWatcher::uniqueFileName(m_directory, macro.name());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(macro.toJson()) < 0 || !file.commit()) {
        if (error)
            *error = QStringLiteral("%1: %2").arg(path, file.errorString());
        return false;
    }
    m_files.insert(macro.name(), path);
    m_macros.insert(macro.name(), macro);
    return true;
}

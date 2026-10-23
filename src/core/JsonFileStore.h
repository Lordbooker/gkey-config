// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "DirectoryWatcher.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QSaveFile>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <functional>

// A directory of JSON files, one item per file, identified by the item's name
// (profiles, macros). T provides:
//   static std::expected<T, QString> fromJson(const QByteArray &)
//   QByteArray toJson() const
//   const QString &name() const, void setName(const QString &)   (setName only for rename())
//
// File names are derived from the item name when an item is created
// (DirectoryWatcher::uniqueFileName) and kept afterwards. Writes are atomic.
template<typename T>
class JsonFileStore {
public:
    // Called for every item read; may adjust it and add messages (e.g. warnings).
    using ReadHook = std::function<void(T &item, const QString &path, QStringList &messages)>;

    explicit JsonFileStore(const QString &directory, ReadHook hook = {})
        : m_directory(directory), m_hook(std::move(hook))
    {
        QDir().mkpath(m_directory);
    }

    const QString &directory() const { return m_directory; }
    const QStringList &messages() const { return m_messages; }
    QStringList files() const { return m_files.values(); }

    // Reads all *.json files. Invalid files and duplicate names are reported in messages().
    void load()
    {
        m_items.clear();
        m_files.clear();
        m_messages.clear();
        QDir().mkpath(m_directory);

        const QDir dir(m_directory);
        for (const QString &fileName : dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name)) {
            const QString path = dir.filePath(fileName);
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                m_messages.append(QStringLiteral("%1: %2").arg(path, file.errorString()));
                continue;
            }
            auto item = T::fromJson(file.readAll());
            if (!item) {
                m_messages.append(QStringLiteral("%1: %2").arg(path, item.error()));
                continue;
            }
            if (m_hook)
                m_hook(*item, path, m_messages);
            if (m_items.contains(item->name())) {
                m_messages.append(QStringLiteral("%1: name \"%2\" is already used by %3; file ignored")
                                      .arg(path, item->name(), m_files.value(item->name())));
                continue;
            }
            m_files.insert(item->name(), path);
            m_items.insert(item->name(), *item);
        }
    }

    bool isEmpty() const { return m_items.isEmpty(); }

    // Names sorted case-insensitively.
    QStringList names() const
    {
        QStringList list = m_items.keys();
        std::sort(list.begin(), list.end(), [](const QString &a, const QString &b) {
            return QString::compare(a, b, Qt::CaseInsensitive) < 0;
        });
        return list;
    }

    // nullptr if unknown.
    const T *item(const QString &name) const
    {
        const auto it = m_items.constFind(name);
        return it == m_items.constEnd() ? nullptr : &it.value();
    }

    // Writes the item atomically; an item with the same name keeps its file.
    bool save(const T &item, QString *error)
    {
        QDir().mkpath(m_directory);
        const QString path = m_files.contains(item.name())
                                 ? m_files.value(item.name())
                                 : DirectoryWatcher::uniqueFileName(m_directory, item.name());
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(item.toJson()) < 0 || !file.commit()) {
            if (error)
                *error = QStringLiteral("%1: %2").arg(path, file.errorString());
            return false;
        }
        m_files.insert(item.name(), path);
        m_items.insert(item.name(), item);
        return true;
    }

    // Deletes the item's file.
    bool remove(const QString &name, QString *error)
    {
        const QString path = m_files.value(name);
        if (path.isEmpty()) {
            if (error)
                *error = QStringLiteral("\"%1\" does not exist").arg(name);
            return false;
        }
        QFile file(path);
        if (!file.remove()) {
            if (error)
                *error = QStringLiteral("%1: %2").arg(path, file.errorString());
            return false;
        }
        m_files.remove(name);
        m_items.remove(name);
        return true;
    }

    // The renamed item gets a file name derived from the new name. The new file
    // is written first: if that fails, nothing is lost.
    bool rename(const QString &oldName, const QString &newName, QString *error)
    {
        const T *existing = item(oldName);
        if (!existing) {
            if (error)
                *error = QStringLiteral("\"%1\" does not exist").arg(oldName);
            return false;
        }
        if (oldName == newName)
            return true;
        if (item(newName)) {
            if (error)
                *error = QStringLiteral("\"%1\" already exists").arg(newName);
            return false;
        }
        T renamed = *existing;
        renamed.setName(newName);
        const QString oldPath = m_files.value(oldName);
        if (!save(renamed, error))
            return false;
        m_files.remove(oldName);
        m_items.remove(oldName);
        QFile::remove(oldPath);
        return true;
    }

private:
    QString m_directory;
    ReadHook m_hook;
    QMap<QString, T> m_items;         // name -> item
    QMap<QString, QString> m_files;   // name -> file path
    QStringList m_messages;
};

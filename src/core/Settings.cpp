// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "Settings.h"

#include "DirectoryWatcher.h"
#include "LcdPages.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

QStringList validPages(const QStringList &pages)
{
    QStringList result;
    for (const QString &page : pages) {
        if (LcdPages::allPages().contains(page) && !result.contains(page))
            result.append(page);
    }
    return result.isEmpty() ? LcdPages::defaultPages() : result;
}

}

Settings::Settings(const QString &filePath, QObject *parent) : QObject(parent), m_filePath(filePath)
{
    const QString dir = QFileInfo(filePath).absolutePath();
    QDir().mkpath(dir);
    m_watcher = new DirectoryWatcher(dir, this);
    connect(m_watcher, &DirectoryWatcher::changed, this, [this] {
        const QStringList before = m_lcdPages;
        reload();
        if (m_lcdPages != before)
            emit changed();
    });
    reload();
}

void Settings::reload()
{
    QFile file(m_filePath);
    QJsonObject root;
    if (file.open(QIODevice::ReadOnly))
        root = QJsonDocument::fromJson(file.readAll()).object();
    QStringList pages;
    for (const auto &page : root.value(u"lcd").toObject().value(u"pages").toArray())
        pages.append(page.toString());
    m_lcdPages = validPages(pages);
    m_watcher->setFiles(QFile::exists(m_filePath) ? QStringList{m_filePath} : QStringList{});
}

void Settings::setLcdPages(const QStringList &pages)
{
    m_lcdPages = validPages(pages);
}

bool Settings::save(QString *error)
{
    QJsonObject root;
    QFile existing(m_filePath);
    if (existing.open(QIODevice::ReadOnly))
        root = QJsonDocument::fromJson(existing.readAll()).object();   // keep unknown members
    existing.close();
    QJsonObject lcd = root.value(u"lcd").toObject();
    lcd.insert(QStringLiteral("pages"), QJsonArray::fromStringList(m_lcdPages));
    root.insert(QStringLiteral("lcd"), lcd);

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0 ||
        !file.commit()) {
        if (error)
            *error = QStringLiteral("%1: %2").arg(m_filePath, file.errorString());
        return false;
    }
    m_watcher->setFiles({m_filePath});
    return true;
}

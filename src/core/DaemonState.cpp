// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "DaemonState.h"

#include "Profile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

DaemonState::DaemonState(const QString &filePath) : m_filePath(filePath) {}

void DaemonState::load()
{
    m_entries.clear();
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return;   // first start
    const QJsonObject models = QJsonDocument::fromJson(file.readAll()).object().value(u"models").toObject();
    for (auto it = models.begin(); it != models.end(); ++it) {
        const QJsonObject object = it.value().toObject();
        Entry entry;
        entry.profile = object.value(u"profile").toString();
        entry.mode = object.value(u"mode").toInt(1);
        if (entry.mode < 1 || entry.mode > Profile::ModeCount)
            entry.mode = 1;
        const QJsonArray center = object.value(u"stickCenter").toArray();
        if (center.size() == 2)
            entry.stickCenter = QPoint(center.at(0).toInt(), center.at(1).toInt());
        m_entries.insert(it.key(), entry);
    }
}

bool DaemonState::save() const
{
    QJsonObject models;
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        QJsonObject object;
        object.insert(QStringLiteral("profile"), it.value().profile);
        object.insert(QStringLiteral("mode"), it.value().mode);
        if (it.value().stickCenter)
            object.insert(QStringLiteral("stickCenter"),
                          QJsonArray{it.value().stickCenter->x(), it.value().stickCenter->y()});
        models.insert(it.key(), object);
    }
    QJsonObject root;
    root.insert(QStringLiteral("models"), models);

    QDir().mkpath(QFileInfo(m_filePath).absolutePath());
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}

QString DaemonState::profile(const QString &modelId) const
{
    return m_entries.value(modelId).profile;
}

void DaemonState::setProfile(const QString &modelId, const QString &profile)
{
    m_entries[modelId].profile = profile;
}

int DaemonState::mode(const QString &modelId) const
{
    return m_entries.contains(modelId) ? m_entries.value(modelId).mode : 1;
}

void DaemonState::setMode(const QString &modelId, int mode)
{
    m_entries[modelId].mode = mode;
}

std::optional<QPoint> DaemonState::stickCenter(const QString &modelId) const
{
    return m_entries.value(modelId).stickCenter;
}

void DaemonState::setStickCenter(const QString &modelId, const QPoint &center)
{
    m_entries[modelId].stickCenter = center;
}

#include "DaemonState.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
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
        if (entry.mode < 1 || entry.mode > 3)
            entry.mode = 1;
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

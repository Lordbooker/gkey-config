// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "ModelRegistry.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

QStringList ModelRegistry::defaultSearchPaths()
{
    QStringList paths;
    const QString fromEnvironment = qEnvironmentVariable("GKEY_MODEL_DIR");
    if (!fromEnvironment.isEmpty())
        paths.append(fromEnvironment.split(u':', Qt::SkipEmptyParts));

    const QString userData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (!userData.isEmpty())
        paths.append(userData + QStringLiteral("/gkey-config/models"));

    paths.append(QStringLiteral(GKEY_DATA_DIR "/models"));
    return paths;
}

void ModelRegistry::load(const QStringList &searchPaths)
{
    m_models.clear();
    m_errors.clear();

    for (const QString &path : searchPaths) {
        const QDir dir(path);
        if (!dir.exists())
            continue;
        const QStringList files = dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        for (const QString &fileName : files) {
            const QString filePath = dir.filePath(fileName);
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly)) {
                m_errors.append(QStringLiteral("%1: %2").arg(filePath, file.errorString()));
                continue;
            }
            auto model = DeviceModel::fromJson(file.readAll(), filePath);
            if (!model) {
                m_errors.append(model.error());
                continue;
            }
            if (findById(model->id()))
                continue;   // an earlier search path overrides this file
            m_models.append(std::move(*model));
        }
    }
}

const DeviceModel *ModelRegistry::findByUsb(quint16 vendor, quint16 product) const
{
    for (const DeviceModel &model : m_models) {
        if (model.matchesUsb(vendor, product))
            return &model;
    }
    return nullptr;
}

const DeviceModel *ModelRegistry::findById(const QString &id) const
{
    for (const DeviceModel &model : m_models) {
        if (model.id() == id)
            return &model;
    }
    return nullptr;
}

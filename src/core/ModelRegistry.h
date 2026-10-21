#pragma once

#include "DeviceModel.h"

#include <QList>
#include <QString>
#include <QStringList>

// Loads all model files and finds the model for a USB device.
//
// Search order (the first file with a given model id wins):
//   1. directories in $GKEY_MODEL_DIR (colon separated), if set - for development and tests
//   2. $XDG_DATA_HOME/gkey-config/models (user overrides)
//   3. <install prefix>/share/gkey-config/models
class ModelRegistry {
public:
    // Directories searched by load(), in order.
    static QStringList defaultSearchPaths();

    // Loads all *.json files from the given directories. Invalid files are
    // skipped and reported in errors().
    void load(const QStringList &searchPaths = defaultSearchPaths());

    const QList<DeviceModel> &models() const { return m_models; }
    const QStringList &errors() const { return m_errors; }

    // Model for the given USB IDs; nullptr if unsupported.
    const DeviceModel *findByUsb(quint16 vendor, quint16 product) const;

    // Model with the given id; nullptr if unknown.
    const DeviceModel *findById(const QString &id) const;

private:
    QList<DeviceModel> m_models;
    QStringList m_errors;
};

#pragma once

#include "DaemonState.h"
#include "DeviceMonitor.h"
#include "KeyOutput.h"
#include "ModelRegistry.h"
#include "Profile.h"
#include "VirtualInput.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

#include <memory>

class Device;
class ProfileStore;

// Ties everything together: device discovery, one profile store per model,
// the active profile and mode, and the shared virtual output device.
class Daemon : public QObject {
    Q_OBJECT
public:
    explicit Daemon(QObject *parent = nullptr);
    ~Daemon() override;

    // Returns false and sets an error text if the daemon cannot run at all.
    bool start(QString *error);

private:
    // Everything that is shared by all connected devices of one model.
    struct ModelContext {
        DeviceModel model;
        ProfileStore *store = nullptr;
        Profile profile;
        int mode = 1;
    };

    void onInputAdded(const DeviceMonitor::InputInfo &info);
    void onInputRemoved(const QString &devNode);

    ModelContext &context(const DeviceModel &model);
    void selectProfile(ModelContext &ctx, const QString &requested);
    void applyProfile(ModelContext &ctx);
    void applyMode(ModelContext &ctx);
    void onProfilesChanged(const QString &modelId);
    void onModeRequested(const QString &modelId, int mode);
    void onProfileRequested(const QString &modelId, const QString &target);
    void logStoreMessages(const ModelContext &ctx);
    void saveState();

    ModelRegistry m_registry;
    VirtualInput m_virtualInput;
    std::unique_ptr<KeyOutput> m_output;
    DaemonState m_state;
    DeviceMonitor *m_monitor = nullptr;
    QHash<QString, ModelContext> m_contexts;   // model id -> context
    QHash<QString, Device *> m_devices;        // HID syspath -> device
    QSet<QString> m_ignoredRoles;              // logged once: "<model>/<role>"
};

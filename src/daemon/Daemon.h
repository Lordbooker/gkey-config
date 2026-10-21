#pragma once

#include "DaemonState.h"
#include "DeviceMonitor.h"
#include "KeyOutput.h"
#include "ModelRegistry.h"
#include "Profile.h"
#include "VirtualInput.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

#include <memory>
#include <optional>

class Device;
class MacroRun;
class MacroStore;
class ProfileStore;
class Settings;

// Ties everything together: device discovery, one profile store per model,
// the macro library, the active profile and mode, LEDs, macro recording state
// and the shared virtual output device. The D-Bus service (DBusService) is a
// thin layer on top of the public functions and signals.
class Daemon : public QObject {
    Q_OBJECT
public:
    explicit Daemon(QObject *parent = nullptr);
    ~Daemon() override;

    // Returns false and sets an error text if the daemon cannot run at all.
    bool start(QString *error);

    // --- Used by the D-Bus service --------------------------------------
    // Status as JSON (devices, models with profiles, macros, recording).
    QByteArray statusJson();
    bool setProfile(const QString &modelId, const QString &name);
    bool setMode(const QString &modelId, int mode);
    // Temporary colour until the next profile change; nullopt restores the profile's colour.
    bool setBacklight(const QString &modelId, const std::optional<Profile::Rgb> &rgb);
    void reload();
    bool runMacro(const QString &name);
    void stopMacros();
    void setRecording(bool recording);
    bool recording() const { return m_recording; }
    void lcdText(const QString &text, int seconds);
    void lcdPage(const QString &page);
    bool calibrateStick(const QString &modelId);

signals:
    void deviceAdded(const QString &modelId, const QString &syspath);
    void deviceRemoved(const QString &modelId, const QString &syspath);
    void profileChanged(const QString &modelId, const QString &name);
    void modeChanged(const QString &modelId, int mode);
    void keyEvent(const QString &modelId, const QString &keyId, bool pressed);
    void recordingChanged(bool recording);

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
    Device *createDevice(const DeviceModel &model, const QString &hidSyspath);

    ModelContext *context(const QString &modelId);
    void selectProfile(ModelContext &ctx, const QString &requested);
    void applyProfile(ModelContext &ctx);
    void applyMode(ModelContext &ctx);
    void onProfilesChanged(const QString &modelId);
    void onProfileRequested(const QString &modelId, const QString &target);
    void logStoreMessages(const ModelContext &ctx);
    void saveState();
    QList<Device *> devicesOf(const QString &modelId) const;
    void updateLcdStatus(Device *device);

    ModelRegistry m_registry;
    VirtualInput m_virtualInput;
    std::unique_ptr<KeyOutput> m_output;
    DaemonState m_state;
    DeviceMonitor *m_monitor = nullptr;
    MacroStore *m_macros = nullptr;
    Settings *m_settings = nullptr;
    QHash<QString, ModelContext> m_contexts;   // model id -> context
    QHash<QString, Device *> m_devices;        // HID syspath -> device
    QList<MacroRun *> m_freeRuns;              // macros started via D-Bus
    QSet<QString> m_ignoredRoles;              // logged once: "<model>/<role>"
    bool m_recording = false;
};

// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "Daemon.h"

#include "BindingEngine.h"
#include "ConfigPaths.h"
#include "Device.h"
#include "LcdController.h"
#include "LedController.h"
#include "MacroRun.h"
#include "MacroStore.h"
#include "ProfileStore.h"
#include "Settings.h"
#include "StickProcessor.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QThread>

namespace {

Q_LOGGING_CATEGORY(lcDaemon, "gkey.daemon")

// Input roles handled by the daemon.
bool roleHandled(const QString &role)
{
    return role == u"keys" || role == u"stick";
}

}

Daemon::Daemon(QObject *parent) : QObject(parent), m_state(ConfigPaths::stateDir() + QStringLiteral("/state.json")) {}

Daemon::~Daemon()
{
    // Release held keys while the virtual device still exists.
    stopMacros();
    qDeleteAll(m_devices);
    m_devices.clear();
}

bool Daemon::start(QString *error, const DaemonStartOptions &options)
{
    m_registry.load();
    for (const QString &message : m_registry.errors())
        qCWarning(lcDaemon) << "invalid model file:" << message;
    if (m_registry.models().isEmpty()) {
        *error = QStringLiteral("no model files found in %1")
                     .arg(ModelRegistry::defaultSearchPaths().join(QStringLiteral(", ")));
        return false;
    }

    if (options.output) {
        m_output = std::make_unique<KeyOutput>(*options.output);
    } else {
        // Right after login the daemon may start before logind has given the
        // session user access to /dev/uinput (udev "uaccess"): wait for it a
        // little instead of failing (and being restarted by systemd).
        constexpr int Attempts = 20;
        constexpr unsigned long WaitMs = 500;
        bool created = false;
        for (int attempt = 1; attempt <= Attempts; ++attempt) {
            created = m_virtualInput.create(error);
            if (created || !m_virtualInput.permissionDenied() || attempt == Attempts)
                break;
            if (attempt == 1)
                qCInfo(lcDaemon) << "no access to /dev/uinput yet, waiting for the login session";
            QThread::msleep(WaitMs);
        }
        if (!created)
            return false;
        m_output = std::make_unique<KeyOutput>(m_virtualInput);
    }
    m_state.load();

    m_macros = new MacroStore(ConfigPaths::macroDir(), this);
    for (const QString &message : m_macros->messages())
        qCWarning(lcDaemon).noquote() << message;
    connect(m_macros, &MacroStore::changed, this, [this] {
        for (const QString &message : m_macros->messages())
            qCWarning(lcDaemon).noquote() << message;
        qCInfo(lcDaemon) << "macros reloaded:" << m_macros->names().size();
    });

    m_settings = new Settings(ConfigPaths::settingsFile(), this);
    connect(m_settings, &Settings::changed, this, [this] {
        qCInfo(lcDaemon) << "settings reloaded, LCD pages:" << m_settings->lcdPages();
        for (Device *device : std::as_const(m_devices)) {
            if (device->lcd())
                device->lcd()->setPages(m_settings->lcdPages());
        }
    });

    if (options.monitorDevices) {
        m_monitor = new DeviceMonitor(this);
        connect(m_monitor, &DeviceMonitor::inputAdded, this, &Daemon::onInputAdded);
        connect(m_monitor, &DeviceMonitor::inputRemoved, this, &Daemon::onInputRemoved);
        if (!m_monitor->start(error))
            return false;
    }

    qCInfo(lcDaemon) << "running," << m_registry.models().size() << "model(s) known," << m_macros->names().size()
                     << "macro(s)";
    return true;
}

// --- Devices -------------------------------------------------------------------

void Daemon::onInputAdded(const DeviceMonitor::InputInfo &info)
{
    const DeviceModel *model = m_registry.findByUsb(info.vendor, info.product);
    if (!model)
        return;

    QString role;
    for (auto it = model->inputs().begin(); it != model->inputs().end(); ++it) {
        if (it.value() == info.inputName)
            role = it.key();
    }
    if (role.isEmpty()) {
        qCDebug(lcDaemon) << info.devNode << "unknown input" << info.inputName << "of" << model->name();
        return;
    }
    if (info.hidDriver != u"lg-g15") {
        qCWarning(lcDaemon).noquote() << QStringLiteral("%1 (%2) is bound to driver \"%3\", expected \"lg-g15\"")
                                             .arg(model->name(), info.devNode, info.hidDriver);
        return;
    }
    if (!roleHandled(role)) {
        if (!m_ignoredRoles.contains(model->id() + u'/' + role)) {
            m_ignoredRoles.insert(model->id() + u'/' + role);
            qCInfo(lcDaemon).noquote() << QStringLiteral("%1: input \"%2\" is left to the system in this version")
                                              .arg(model->name(), info.inputName);
        }
        return;
    }

    Device *device = m_devices.value(info.hidSyspath);
    const bool newDevice = !device;
    if (newDevice)
        device = createDevice(*model, info.hidSyspath);

    QString error;
    if (!device->addInput(role, info.devNode, &error)) {
        qCWarning(lcDaemon).noquote() << error;
        if (newDevice)
            delete device;
        return;
    }
    if (newDevice) {
        m_devices.insert(info.hidSyspath, device);
        const ModelContext *ctx = context(model->id());
        qCInfo(lcDaemon).noquote() << QStringLiteral("%1 connected (%2), profile \"%3\", mode M%4")
                                          .arg(model->name(), info.devNode, ctx->profile.name())
                                          .arg(ctx->mode);
        emit deviceAdded(model->id(), info.hidSyspath);
    }
}

Device *Daemon::createDevice(const DeviceModel &model, const QString &hidSyspath)
{
    ModelContext *ctx = context(model.id());
    auto *device = new Device(model, hidSyspath, *m_output, this);
    const QString &modelId = model.id();
    BindingEngine *engine = device->engine();
    engine->setMacroStore(m_macros);
    engine->setProfile(ctx->profile);
    engine->setMode(ctx->mode);

    connect(engine, &BindingEngine::modeRequested, this, [this, modelId](int mode) { setMode(modelId, mode); });
    connect(engine, &BindingEngine::profileRequested, this,
            [this, modelId](const QString &target) { onProfileRequested(modelId, target); });
    connect(engine, &BindingEngine::recordRequested, this, [this] { setRecording(!m_recording); });
    connect(engine, &BindingEngine::lcdPageRequested, this, [device](const QString &page) {
        if (device->lcd())
            device->lcd()->showPage(page);
    });
    connect(engine, &BindingEngine::keyEvent, this,
            [this, modelId](const QString &keyId, bool pressed) { emit keyEvent(modelId, keyId, pressed); });
    connect(device, &Device::disconnected, this, [this, device] {
        qCInfo(lcDaemon).noquote() << device->model().name() << "disconnected";
        m_devices.remove(device->hidSyspath());
        emit deviceRemoved(device->model().id(), device->hidSyspath());
        device->deleteLater();
    });

    device->setStickCenter(m_state.stickCenter(modelId));
    device->setStickSettings(ctx->profile.stickSettings());

    // The G13 forgets its LED state at power loss: always apply on connect.
    device->leds().applyProfile(ctx->profile);
    device->leds().setMode(ctx->mode);
    device->leds().setRecording(m_recording);
    if (device->lcd()) {
        device->lcd()->setPages(m_settings ? m_settings->lcdPages() : QStringList());
        device->lcd()->start();   // start animation (if enabled) on every connect
    }
    updateLcdStatus(device);
    return device;
}

void Daemon::onInputRemoved(const QString &devNode)
{
    for (Device *device : std::as_const(m_devices)) {
        if (device->removeInput(devNode))
            return;
    }
}

QList<Device *> Daemon::devicesOf(const QString &modelId) const
{
    QList<Device *> list;
    for (Device *device : m_devices) {
        if (device->model().id() == modelId)
            list.append(device);
    }
    return list;
}

// --- Profiles and modes ----------------------------------------------------------

Daemon::ModelContext *Daemon::context(const QString &modelId)
{
    if (const auto it = m_contexts.find(modelId); it != m_contexts.end())
        return &it->second;
    const DeviceModel *model = m_registry.findById(modelId);
    if (!model)
        return nullptr;

    ModelContext ctx;
    ctx.model = *model;
    ctx.store = new ProfileStore(*model, ConfigPaths::profileDir(modelId), this);
    ctx.store->ensureDefault();
    ctx.mode = m_state.mode(modelId);
    connect(ctx.store, &ProfileStore::changed, this, [this, modelId] { onProfilesChanged(modelId); });

    ModelContext &inserted = m_contexts.emplace(modelId, std::move(ctx)).first->second;
    logStoreMessages(inserted);
    selectProfile(inserted, m_state.profile(modelId));
    return &inserted;
}

void Daemon::selectProfile(ModelContext &ctx, const QString &requested)
{
    // The requested profile, else the first one; the pointer is only used after the check.
    const Profile *profile = ctx.store->profile(requested);
    if (!profile) {
        const QStringList names = ctx.store->names();
        if (!names.isEmpty())
            profile = ctx.store->profile(names.first());
    }

    ctx.profile = profile ? *profile : Profile();
    m_state.setProfile(ctx.model.id(), ctx.profile.name());
    saveState();
}

void Daemon::applyProfile(ModelContext &ctx)
{
    for (Device *device : devicesOf(ctx.model.id())) {
        device->engine()->setProfile(ctx.profile);
        device->setStickSettings(ctx.profile.stickSettings());
        device->leds().applyProfile(ctx.profile);
        updateLcdStatus(device);
    }
    emit profileChanged(ctx.model.id(), ctx.profile.name());
}

void Daemon::applyMode(ModelContext &ctx)
{
    for (Device *device : devicesOf(ctx.model.id())) {
        device->engine()->setMode(ctx.mode);
        if (device->stick())
            device->stick()->reset();
        device->leds().setMode(ctx.mode);
        updateLcdStatus(device);
    }
    emit modeChanged(ctx.model.id(), ctx.mode);
}

void Daemon::onProfilesChanged(const QString &modelId)
{
    ModelContext *ctx = context(modelId);
    if (!ctx)
        return;
    logStoreMessages(*ctx);
    const QString previous = ctx->profile.name();
    const QByteArray previousContent = ctx->profile.toJson();
    selectProfile(*ctx, previous);
    if (ctx->profile.name() == previous && ctx->profile.toJson() == previousContent)
        return;   // e.g. our own write of the default profile, or another profile changed
    applyProfile(*ctx);
    if (ctx->profile.name() == previous)
        qCInfo(lcDaemon).noquote() << QStringLiteral("%1: profile \"%2\" reloaded").arg(ctx->model.name(), previous);
    else
        qCInfo(lcDaemon).noquote() << QStringLiteral("%1: profile \"%2\" is gone, switched to \"%3\"")
                                          .arg(ctx->model.name(), previous, ctx->profile.name());
}

bool Daemon::setMode(const QString &modelId, int mode)
{
    ModelContext *ctx = context(modelId);
    if (!ctx || mode < 1 || mode > Profile::ModeCount)
        return false;
    if (ctx->mode == mode)
        return true;
    ctx->mode = mode;
    applyMode(*ctx);
    m_state.setMode(modelId, mode);
    saveState();
    qCInfo(lcDaemon).noquote() << QStringLiteral("%1: mode M%2").arg(ctx->model.name()).arg(mode);
    return true;
}

bool Daemon::setProfile(const QString &modelId, const QString &name)
{
    ModelContext *ctx = context(modelId);
    if (!ctx || !ctx->store->profile(name))
        return false;
    if (name != ctx->profile.name()) {
        selectProfile(*ctx, name);
        applyProfile(*ctx);
        qCInfo(lcDaemon).noquote() << QStringLiteral("%1: profile \"%2\"").arg(ctx->model.name(), ctx->profile.name());
    }
    return true;
}

void Daemon::onProfileRequested(const QString &modelId, const QString &target)
{
    ModelContext *ctx = context(modelId);
    if (!ctx)
        return;
    const QStringList names = ctx->store->names();
    if (names.isEmpty())
        return;

    QString name;
    const qsizetype current = names.indexOf(ctx->profile.name());
    if (target == u"next") {
        name = names.at((current + 1) % names.size());
    } else if (target == u"previous") {
        name = names.at((current - 1 + names.size()) % names.size());
    } else {
        for (const QString &candidate : names) {
            if (candidate.compare(target, Qt::CaseInsensitive) == 0)
                name = candidate;
        }
    }
    if (name.isEmpty()) {
        qCWarning(lcDaemon).noquote()
            << QStringLiteral("%1: profile \"%2\" does not exist").arg(ctx->model.name(), target);
        return;
    }
    setProfile(modelId, name);
}

bool Daemon::setBacklight(const QString &modelId, const std::optional<Profile::Rgb> &rgb)
{
    ModelContext *ctx = context(modelId);
    if (!ctx)
        return false;
    for (Device *device : devicesOf(modelId)) {
        if (rgb)
            device->leds().setBacklight(rgb);
        else
            device->leds().applyProfile(ctx->profile);   // ResetBacklight
    }
    return true;
}

void Daemon::reload()
{
    if (m_macros)
        m_macros->reload();
    for (auto &[modelId, ctx] : m_contexts) {
        ctx.store->reload();
        onProfilesChanged(modelId);
    }
}

// --- Macros and recording ---------------------------------------------------------

bool Daemon::runMacro(const QString &name)
{
    const Macro *macro = m_macros ? m_macros->macro(name) : nullptr;
    if (!macro)
        return false;
    auto *run = new MacroRun(*macro, Action::Repeat::Once, 1, *m_output, this);
    m_freeRuns.append(run);
    connect(run, &MacroRun::finished, this, [this, run] {
        m_freeRuns.removeOne(run);
        run->deleteLater();
    });
    run->start();
    return true;
}

void Daemon::stopMacros()
{
    const auto runs = m_freeRuns;
    for (MacroRun *run : runs)
        run->stop();
    for (Device *device : std::as_const(m_devices))
        device->engine()->stopMacros();
}

void Daemon::setRecording(bool recording)
{
    if (m_recording == recording)
        return;
    m_recording = recording;
    for (Device *device : std::as_const(m_devices)) {
        device->leds().setRecording(recording);
        updateLcdStatus(device);
    }
    qCInfo(lcDaemon) << (recording ? "macro recording started" : "macro recording stopped");
    emit recordingChanged(recording);
}

// --- LCD and stick --------------------------------------------------------------------------

void Daemon::updateLcdStatus(Device *device)
{
    if (!device->lcd())
        return;
    const auto it = m_contexts.find(device->model().id());
    LcdPages::Status status;
    status.deviceName = device->model().name();
    if (it != m_contexts.end()) {
        status.profile = it->second.profile.name();
        status.mode = it->second.mode;
    }
    status.recording = m_recording;
    device->lcd()->setStatus(status);
}

void Daemon::lcdText(const QString &text, int seconds)
{
    for (Device *device : std::as_const(m_devices)) {
        if (device->lcd())
            device->lcd()->setText(text, seconds);
    }
}

void Daemon::lcdPage(const QString &page)
{
    for (Device *device : std::as_const(m_devices)) {
        if (device->lcd())
            device->lcd()->showPage(page);
    }
}

bool Daemon::calibrateStick(const QString &modelId)
{
    bool done = false;
    for (Device *device : devicesOf(modelId)) {
        if (const auto center = device->calibrateStick()) {
            m_state.setStickCenter(modelId, *center);
            qCInfo(lcDaemon).noquote() << QStringLiteral("%1: stick rest position set to %2/%3")
                                              .arg(device->model().name())
                                              .arg(center->x())
                                              .arg(center->y());
            done = true;
        }
    }
    if (done)
        saveState();
    return done;
}

// --- Status ---------------------------------------------------------------------------

QByteArray Daemon::statusJson()
{
    QJsonObject root;
    root.insert(QStringLiteral("version"), QStringLiteral(GKEY_VERSION));
    root.insert(QStringLiteral("recording"), m_recording);

    QJsonArray devices;
    for (Device *device : std::as_const(m_devices)) {
        QJsonObject object;
        object.insert(QStringLiteral("model"), device->model().id());
        object.insert(QStringLiteral("syspath"), device->hidSyspath());
        object.insert(QStringLiteral("ledPermission"), !device->leds().permissionDenied());
        if (device->lcd())
            object.insert(QStringLiteral("lcdPage"), device->lcd()->currentPage());
        if (StickProcessor *stick = device->stick())
            object.insert(QStringLiteral("stickCenter"), QJsonArray{stick->centerX(), stick->centerY()});
        devices.append(object);
    }
    root.insert(QStringLiteral("devices"), devices);

    QJsonObject models;
    for (const DeviceModel &model : m_registry.models()) {
        QJsonObject object;
        object.insert(QStringLiteral("name"), model.name());
        object.insert(QStringLiteral("tested"), model.tested());
        if (const auto it = m_contexts.find(model.id()); it != m_contexts.end()) {
            const ModelContext &ctx = it->second;
            object.insert(QStringLiteral("profile"), ctx.profile.name());
            object.insert(QStringLiteral("mode"), ctx.mode);
            object.insert(QStringLiteral("profiles"), QJsonArray::fromStringList(ctx.store->names()));
            object.insert(QStringLiteral("profileDir"), ctx.store->directory());
        }
        models.insert(model.id(), object);
    }
    root.insert(QStringLiteral("models"), models);
    if (m_macros) {
        root.insert(QStringLiteral("macros"), QJsonArray::fromStringList(m_macros->names()));
        root.insert(QStringLiteral("macroDir"), m_macros->directory());
    }
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

void Daemon::logStoreMessages(const ModelContext &ctx)
{
    for (const QString &message : ctx.store->messages())
        qCWarning(lcDaemon).noquote() << message;
}

void Daemon::saveState()
{
    if (!m_state.save())
        qCWarning(lcDaemon) << "cannot write the state file in" << ConfigPaths::stateDir();
}

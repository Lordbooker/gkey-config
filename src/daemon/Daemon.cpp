#include "Daemon.h"

#include "BindingEngine.h"
#include "ConfigPaths.h"
#include "Device.h"
#include "ProfileStore.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcDaemon, "gkey.daemon")

namespace {

// Input roles handled in this version. The stick ("stick") stays with the
// system until the stick modes exist (milestone 5), so games keep seeing it.
bool roleHandled(const QString &role)
{
    return role == u"keys";
}

}

Daemon::Daemon(QObject *parent)
    : QObject(parent), m_state(ConfigPaths::stateDir() + QStringLiteral("/state.json"))
{
}

Daemon::~Daemon()
{
    // Devices release their held keys while the virtual device still exists.
    qDeleteAll(m_devices);
    m_devices.clear();
}

bool Daemon::start(QString *error)
{
    m_registry.load();
    for (const QString &message : m_registry.errors())
        qCWarning(lcDaemon) << "invalid model file:" << message;
    if (m_registry.models().isEmpty()) {
        *error = QStringLiteral("no model files found in %1")
                     .arg(ModelRegistry::defaultSearchPaths().join(QStringLiteral(", ")));
        return false;
    }

    if (!m_virtualInput.create(error))
        return false;
    m_output = std::make_unique<KeyOutput>(m_virtualInput);
    m_state.load();

    m_monitor = new DeviceMonitor(this);
    connect(m_monitor, &DeviceMonitor::inputAdded, this, &Daemon::onInputAdded);
    connect(m_monitor, &DeviceMonitor::inputRemoved, this, &Daemon::onInputRemoved);
    if (!m_monitor->start(error))
        return false;

    qCInfo(lcDaemon) << "running," << m_registry.models().size() << "model(s) known";
    return true;
}

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

    ModelContext &ctx = context(*model);
    Device *device = m_devices.value(info.hidSyspath);
    const bool newDevice = !device;
    if (newDevice) {
        device = new Device(*model, info.hidSyspath, *m_output, this);
        const QString modelId = model->id();
        connect(device->engine(), &BindingEngine::modeRequested, this,
                [this, modelId](int mode) { onModeRequested(modelId, mode); });
        connect(device->engine(), &BindingEngine::profileRequested, this,
                [this, modelId](const QString &target) { onProfileRequested(modelId, target); });
        connect(device->engine(), &BindingEngine::recordRequested, this,
                [] { qCInfo(lcDaemon) << "macro recording is not implemented yet"; });
        connect(device, &Device::disconnected, this, [this, device] {
            qCInfo(lcDaemon).noquote() << device->model().name() << "disconnected";
            m_devices.remove(device->hidSyspath());
            device->deleteLater();
        });
        device->engine()->setProfile(ctx.profile);
        device->engine()->setMode(ctx.mode);
    }

    QString error;
    if (!device->addInput(role, info.devNode, &error)) {
        qCWarning(lcDaemon).noquote() << error;
        if (newDevice)
            delete device;
        return;
    }
    if (newDevice) {
        m_devices.insert(info.hidSyspath, device);
        qCInfo(lcDaemon).noquote() << QStringLiteral("%1 connected (%2), profile \"%3\", mode M%4")
                                          .arg(model->name(), info.devNode, ctx.profile.name())
                                          .arg(ctx.mode);
    }
}

void Daemon::onInputRemoved(const QString &devNode)
{
    for (Device *device : std::as_const(m_devices)) {
        if (device->removeInput(devNode))
            return;
    }
}

Daemon::ModelContext &Daemon::context(const DeviceModel &model)
{
    auto it = m_contexts.find(model.id());
    if (it != m_contexts.end())
        return it.value();

    ModelContext ctx;
    ctx.model = model;
    ctx.store = new ProfileStore(model, ConfigPaths::profileDir(model.id()), this);
    ctx.store->ensureDefault();
    ctx.mode = m_state.mode(model.id());
    const QString modelId = model.id();
    connect(ctx.store, &ProfileStore::changed, this, [this, modelId] { onProfilesChanged(modelId); });

    it = m_contexts.insert(model.id(), ctx);
    logStoreMessages(it.value());
    selectProfile(it.value(), m_state.profile(model.id()));
    return it.value();
}

void Daemon::selectProfile(ModelContext &ctx, const QString &requested)
{
    const QStringList names = ctx.store->names();
    QString name;
    if (ctx.store->profile(requested))
        name = requested;
    else if (!names.isEmpty())
        name = names.first();

    ctx.profile = name.isEmpty() ? Profile() : *ctx.store->profile(name);
    m_state.setProfile(ctx.model.id(), ctx.profile.name());
    saveState();
}

void Daemon::applyProfile(ModelContext &ctx)
{
    for (Device *device : std::as_const(m_devices)) {
        if (device->model().id() == ctx.model.id())
            device->engine()->setProfile(ctx.profile);
    }
}

void Daemon::applyMode(ModelContext &ctx)
{
    for (Device *device : std::as_const(m_devices)) {
        if (device->model().id() == ctx.model.id())
            device->engine()->setMode(ctx.mode);
    }
}

void Daemon::onProfilesChanged(const QString &modelId)
{
    auto it = m_contexts.find(modelId);
    if (it == m_contexts.end())
        return;
    ModelContext &ctx = it.value();
    logStoreMessages(ctx);
    const QString previous = ctx.profile.name();
    selectProfile(ctx, previous);
    applyProfile(ctx);
    if (ctx.profile.name() == previous)
        qCInfo(lcDaemon).noquote() << QStringLiteral("%1: profile \"%2\" reloaded").arg(ctx.model.name(), previous);
    else
        qCInfo(lcDaemon).noquote() << QStringLiteral("%1: profile \"%2\" is gone, switched to \"%3\"")
                                          .arg(ctx.model.name(), previous, ctx.profile.name());
}

void Daemon::onModeRequested(const QString &modelId, int mode)
{
    auto it = m_contexts.find(modelId);
    if (it == m_contexts.end() || it->mode == mode)
        return;
    it->mode = mode;
    applyMode(it.value());
    m_state.setMode(modelId, mode);
    saveState();
    qCInfo(lcDaemon).noquote() << QStringLiteral("%1: mode M%2").arg(it->model.name()).arg(mode);
}

void Daemon::onProfileRequested(const QString &modelId, const QString &target)
{
    auto it = m_contexts.find(modelId);
    if (it == m_contexts.end())
        return;
    ModelContext &ctx = it.value();
    const QStringList names = ctx.store->names();
    if (names.isEmpty())
        return;

    QString name;
    const qsizetype current = names.indexOf(ctx.profile.name());
    if (target == u"next") {
        name = names.at((current + 1) % names.size());
    } else if (target == u"previous") {
        name = names.at((current - 1 + names.size()) % names.size());
    } else {
        for (const QString &candidate : names) {
            if (candidate.compare(target, Qt::CaseInsensitive) == 0)
                name = candidate;
        }
        if (name.isEmpty()) {
            qCWarning(lcDaemon).noquote() << QStringLiteral("%1: profile \"%2\" does not exist")
                                                 .arg(ctx.model.name(), target);
            return;
        }
    }
    if (name == ctx.profile.name())
        return;
    selectProfile(ctx, name);
    applyProfile(ctx);
    qCInfo(lcDaemon).noquote() << QStringLiteral("%1: profile \"%2\"").arg(ctx.model.name(), ctx.profile.name());
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

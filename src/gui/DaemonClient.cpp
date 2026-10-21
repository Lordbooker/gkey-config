#include "DaemonClient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QJsonArray>
#include <QJsonDocument>

namespace {

const QString Service = QStringLiteral("io.github.lordbooker.GKeyConfig");
const QString Path = QStringLiteral("/io/github/lordbooker/GKeyConfig");
const QString Interface = QStringLiteral("io.github.lordbooker.GKeyConfig1");

}

DaemonClient::DaemonClient(QObject *parent) : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    m_interface = new QDBusInterface(Service, Path, Interface, bus, this);
    m_interface->setTimeout(3000);

    m_watcher = new QDBusServiceWatcher(Service, bus,
                                        QDBusServiceWatcher::WatchForRegistration
                                            | QDBusServiceWatcher::WatchForUnregistration,
                                        this);
    connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this, [this] {
        setAvailable(true);
        refreshStatus();
    });
    connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this] { setAvailable(false); });

    bus.connect(Service, Path, Interface, QStringLiteral("DeviceAdded"), this, SLOT(onDeviceAdded(QString, QString)));
    bus.connect(Service, Path, Interface, QStringLiteral("DeviceRemoved"), this,
                SLOT(onDeviceRemoved(QString, QString)));
    bus.connect(Service, Path, Interface, QStringLiteral("ProfileChanged"), this,
                SLOT(onProfileChanged(QString, QString)));
    bus.connect(Service, Path, Interface, QStringLiteral("ModeChanged"), this, SLOT(onModeChanged(QString, int)));
    bus.connect(Service, Path, Interface, QStringLiteral("KeyEvent"), this, SLOT(onKeyEvent(QString, QString, bool)));
    bus.connect(Service, Path, Interface, QStringLiteral("RecordingChanged"), this, SLOT(onRecordingChanged(bool)));

    m_available = bus.isConnected() && bus.interface() && bus.interface()->isServiceRegistered(Service).value();
    if (m_available)
        refreshStatus();
}

void DaemonClient::setAvailable(bool available)
{
    if (m_available == available)
        return;
    m_available = available;
    if (!available) {
        m_status = {};
        emit statusChanged();
    }
    emit availableChanged(available);
}

void DaemonClient::refreshStatus()
{
    if (!m_available)
        return;
    const QDBusReply<QString> reply = m_interface->call(QStringLiteral("Status"));
    if (!reply.isValid())
        return;
    m_status = QJsonDocument::fromJson(reply.value().toUtf8()).object();
    emit statusChanged();
}

QStringList DaemonClient::connectedModels() const
{
    QStringList models;
    for (const QJsonValue &device : m_status.value(u"devices").toArray()) {
        const QString model = device.toObject().value(u"model").toString();
        if (!models.contains(model))
            models.append(model);
    }
    return models;
}

QString DaemonClient::activeProfile(const QString &modelId) const
{
    return m_status.value(u"models").toObject().value(modelId).toObject().value(u"profile").toString();
}

int DaemonClient::activeMode(const QString &modelId) const
{
    return m_status.value(u"models").toObject().value(modelId).toObject().value(u"mode").toInt(1);
}

bool DaemonClient::ledPermission(const QString &modelId) const
{
    for (const QJsonValue &device : m_status.value(u"devices").toArray()) {
        const QJsonObject object = device.toObject();
        if (object.value(u"model").toString() == modelId && !object.value(u"ledPermission").toBool(true))
            return false;
    }
    return true;
}

bool DaemonClient::recording() const
{
    return m_status.value(u"recording").toBool();
}

bool DaemonClient::callBool(const QString &method, const QList<QVariant> &arguments)
{
    if (!m_available)
        return false;
    const QDBusReply<bool> reply = m_interface->callWithArgumentList(QDBus::Block, method, arguments);
    return reply.isValid() && reply.value();
}

bool DaemonClient::setProfile(const QString &modelId, const QString &name)
{
    return callBool(QStringLiteral("SetProfile"), {modelId, name});
}

bool DaemonClient::setMode(const QString &modelId, int mode)
{
    return callBool(QStringLiteral("SetMode"), {modelId, mode});
}

bool DaemonClient::setBacklight(const QString &modelId, const QColor &color)
{
    return callBool(QStringLiteral("SetBacklight"), {modelId, color.red(), color.green(), color.blue()});
}

bool DaemonClient::resetBacklight(const QString &modelId)
{
    return callBool(QStringLiteral("ResetBacklight"), {modelId});
}

bool DaemonClient::runMacro(const QString &name)
{
    return callBool(QStringLiteral("RunMacro"), {name});
}

void DaemonClient::stopMacros()
{
    if (m_available)
        m_interface->call(QDBus::NoBlock, QStringLiteral("StopMacros"));
}

void DaemonClient::setRecording(bool recording)
{
    if (m_available)
        m_interface->call(QDBus::NoBlock, QStringLiteral("SetRecording"), recording);
}

void DaemonClient::onDeviceAdded(const QString &model, const QString &)
{
    refreshStatus();
    emit deviceAdded(model);
}

void DaemonClient::onDeviceRemoved(const QString &model, const QString &)
{
    refreshStatus();
    emit deviceRemoved(model);
}

void DaemonClient::onProfileChanged(const QString &model, const QString &name)
{
    refreshStatus();
    emit profileChanged(model, name);
}

void DaemonClient::onModeChanged(const QString &model, int mode)
{
    refreshStatus();
    emit modeChanged(model, mode);
}

void DaemonClient::onKeyEvent(const QString &model, const QString &key, bool pressed)
{
    emit keyEvent(model, key, pressed);
}

void DaemonClient::onRecordingChanged(bool recording)
{
    refreshStatus();
    emit recordingChanged(recording);
}

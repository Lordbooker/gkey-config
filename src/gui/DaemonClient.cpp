// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "DaemonClient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>

namespace {

const QString Service = QStringLiteral("io.github.lordbooker.GKeyConfig");
const QString Path = QStringLiteral("/io/github/lordbooker/GKeyConfig");
const QString Interface = QStringLiteral("io.github.lordbooker.GKeyConfig1");
constexpr int CallTimeoutMs = 3000;

}

DaemonClient::DaemonClient(QObject *parent) : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();

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
    const QDBusReply<QString> reply = call(QStringLiteral("Status"));
    if (!reply.isValid())
        return;
    m_status = QJsonDocument::fromJson(reply.value().toUtf8()).object();
    emit statusChanged();
}

QStringList DaemonClient::connectedModels() const
{
    QStringList models;
    for (const auto &device : m_status.value(u"devices").toArray()) {
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
    return std::ranges::none_of(m_status.value(u"devices").toArray(), [&modelId](const QJsonValue &device) {
        const QJsonObject object = device.toObject();
        return object.value(u"model").toString() == modelId && !object.value(u"ledPermission").toBool(true);
    });
}

QString DaemonClient::stickCenter(const QString &modelId) const
{
    for (const auto &device : m_status.value(u"devices").toArray()) {
        const QJsonObject object = device.toObject();
        const QJsonArray center = object.value(u"stickCenter").toArray();
        if (object.value(u"model").toString() == modelId && center.size() == 2)
            return QStringLiteral("%1 / %2").arg(center.at(0).toInt()).arg(center.at(1).toInt());
    }
    return {};
}

bool DaemonClient::recording() const
{
    return m_status.value(u"recording").toBool();
}

// Calls never start the daemon through D-Bus activation: the GUI only talks to
// a daemon that is already running, so "Stop daemon" and a disabled autostart
// are respected.
QDBusMessage DaemonClient::call(const QString &method, const QList<QVariant> &arguments) const
{
    QDBusMessage message = QDBusMessage::createMethodCall(Service, Path, Interface, method);
    message.setArguments(arguments);
    message.setAutoStartService(false);
    return QDBusConnection::sessionBus().call(message, QDBus::Block, CallTimeoutMs);
}

void DaemonClient::send(const QString &method, const QList<QVariant> &arguments) const
{
    QDBusMessage message = QDBusMessage::createMethodCall(Service, Path, Interface, method);
    message.setArguments(arguments);
    message.setAutoStartService(false);
    QDBusConnection::sessionBus().send(message);
}

bool DaemonClient::callBool(const QString &method, const QList<QVariant> &arguments)
{
    if (!m_available)
        return false;
    const QDBusReply<bool> reply = call(method, arguments);
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
        send(QStringLiteral("StopMacros"));
}

void DaemonClient::setRecording(bool recording)
{
    if (m_available)
        send(QStringLiteral("SetRecording"), {recording});
}

bool DaemonClient::calibrateStick(const QString &modelId)
{
    const bool done = callBool(QStringLiteral("CalibrateStick"), {modelId});
    refreshStatus();
    return done;
}

void DaemonClient::lcdPage(const QString &page)
{
    if (m_available)
        send(QStringLiteral("LcdPage"), {page});
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

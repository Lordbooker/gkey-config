// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "DaemonClient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
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

    m_watcher = new QDBusServiceWatcher(
        Service, bus, QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration, this);
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

void DaemonClient::refreshStatus(QObject *context, const std::function<void()> &done)
{
    if (!m_available) {
        if (done)
            done();
        return;
    }
    call(QStringLiteral("Status"), {}, context, [this, done](const QDBusMessage &reply) {
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
            m_status = QJsonDocument::fromJson(reply.arguments().constFirst().toString().toUtf8()).object();
            emit statusChanged();
        }
        if (done)
            done();
    });
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
void DaemonClient::call(const QString &method, const QList<QVariant> &arguments, QObject *context,
                        const std::function<void(const QDBusMessage &)> &onReply)
{
    QDBusMessage message = QDBusMessage::createMethodCall(Service, Path, Interface, method);
    message.setArguments(arguments);
    message.setAutoStartService(false);
    const QDBusPendingCall pending = QDBusConnection::sessionBus().asyncCall(message, CallTimeoutMs);
    // The watcher deletes itself when the answer has arrived (or the call has
    // failed); the analyzer does not see that through the connection.
    // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, watcher, &QObject::deleteLater);
    // The answer is dropped if the context (e.g. a window) is gone by then.
    if (onReply)
        connect(watcher, &QDBusPendingCallWatcher::finished, context ? context : this,
                [watcher, onReply] { onReply(watcher->reply()); });
    // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

void DaemonClient::send(const QString &method, const QList<QVariant> &arguments) const
{
    QDBusMessage message = QDBusMessage::createMethodCall(Service, Path, Interface, method);
    message.setArguments(arguments);
    message.setAutoStartService(false);
    QDBusConnection::sessionBus().send(message);
}

void DaemonClient::callBool(const QString &method, const QList<QVariant> &arguments, QObject *context, const Done &done)
{
    if (!m_available) {
        if (done)
            done(false);
        return;
    }
    call(method, arguments, context, [done](const QDBusMessage &reply) {
        const bool result = reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty() &&
                            reply.arguments().constFirst().toBool();
        if (done)
            done(result);
    });
}

void DaemonClient::setProfile(const QString &modelId, const QString &name)
{
    callBool(QStringLiteral("SetProfile"), {modelId, name});
}

void DaemonClient::setMode(const QString &modelId, int mode)
{
    callBool(QStringLiteral("SetMode"), {modelId, mode});
}

void DaemonClient::setBacklight(const QString &modelId, const QColor &color)
{
    callBool(QStringLiteral("SetBacklight"), {modelId, color.red(), color.green(), color.blue()});
}

void DaemonClient::resetBacklight(const QString &modelId)
{
    callBool(QStringLiteral("ResetBacklight"), {modelId});
}

void DaemonClient::runMacro(const QString &name, QObject *context, const Done &done)
{
    callBool(QStringLiteral("RunMacro"), {name}, context, done);
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

void DaemonClient::calibrateStick(const QString &modelId, QObject *context, const Done &done)
{
    callBool(QStringLiteral("CalibrateStick"), {modelId}, context, [this, context, done](bool calibrated) {
        refreshStatus(context, [done, calibrated] {
            if (done)
                done(calibrated);
        });
    });
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

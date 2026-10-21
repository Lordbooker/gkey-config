#include "DBusService.h"

#include "Daemon.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>

DBusService::DBusService(Daemon *daemon) : QObject(daemon), m_daemon(daemon)
{
    connect(daemon, &Daemon::deviceAdded, this, &DBusService::DeviceAdded);
    connect(daemon, &Daemon::deviceRemoved, this, &DBusService::DeviceRemoved);
    connect(daemon, &Daemon::profileChanged, this, &DBusService::ProfileChanged);
    connect(daemon, &Daemon::modeChanged, this, &DBusService::ModeChanged);
    connect(daemon, &Daemon::keyEvent, this, &DBusService::KeyEvent);
    connect(daemon, &Daemon::recordingChanged, this, &DBusService::RecordingChanged);
}

bool DBusService::registerOnSessionBus(QString *error)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        *error = QStringLiteral("no D-Bus session bus: %1").arg(bus.lastError().message());
        return false;
    }
    if (!bus.registerObject(QLatin1String(ObjectPath), this,
                            QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals
                                | QDBusConnection::ExportAllProperties)) {
        *error = QStringLiteral("cannot register the D-Bus object: %1").arg(bus.lastError().message());
        return false;
    }
    // DoNotQueue: a second daemon must fail instead of waiting for the name.
    const auto reply = bus.interface()->registerService(QLatin1String(ServiceName),
                                                        QDBusConnectionInterface::DontQueueService,
                                                        QDBusConnectionInterface::DontAllowReplacement);
    if (!reply.isValid() || reply.value() != QDBusConnectionInterface::ServiceRegistered) {
        *error = QStringLiteral("the D-Bus name %1 is already taken - is gkeyd already running?")
                     .arg(QLatin1String(ServiceName));
        return false;
    }
    return true;
}

bool DBusService::recording() const
{
    return m_daemon->recording();
}

QString DBusService::Status()
{
    return QString::fromUtf8(m_daemon->statusJson());
}

bool DBusService::SetProfile(const QString &model, const QString &name)
{
    return m_daemon->setProfile(model, name);
}

bool DBusService::SetMode(const QString &model, int mode)
{
    return m_daemon->setMode(model, mode);
}

bool DBusService::SetBacklight(const QString &model, int red, int green, int blue)
{
    const auto valid = [](int v) { return v >= 0 && v <= 255; };
    if (!valid(red) || !valid(green) || !valid(blue))
        return false;
    return m_daemon->setBacklight(model, Profile::Rgb{red, green, blue});
}

bool DBusService::ResetBacklight(const QString &model)
{
    return m_daemon->setBacklight(model, std::nullopt);
}

void DBusService::Reload()
{
    m_daemon->reload();
}

bool DBusService::RunMacro(const QString &name)
{
    return m_daemon->runMacro(name);
}

void DBusService::StopMacros()
{
    m_daemon->stopMacros();
}

void DBusService::SetRecording(bool recording)
{
    m_daemon->setRecording(recording);
}

void DBusService::LcdText(const QString &text, int seconds)
{
    m_daemon->lcdText(text, qMax(0, seconds));
}

void DBusService::LcdPage(const QString &page)
{
    m_daemon->lcdPage(page);
}

bool DBusService::CalibrateStick(const QString &model)
{
    return m_daemon->calibrateStick(model);
}

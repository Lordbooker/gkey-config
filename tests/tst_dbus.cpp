// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "DBusService.h"
#include "Daemon.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

// Talks to the D-Bus service from a separate process (gdbus), so the calls go
// through a real session bus. Run inside dbus-run-session (see CMakeLists.txt).
class TestDBus : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void introspection();
    void status();
    void recording();
    void unknownModel();
    void secondInstanceIsRejected();

private:
    // Runs gdbus asynchronously so this process can answer the call.
    QString gdbus(const QStringList &arguments);

    Daemon *m_daemon = nullptr;
    DBusService *m_service = nullptr;
};

QString TestDBus::gdbus(const QStringList &arguments)
{
    QProcess process;
    QSignalSpy finished(&process, &QProcess::finished);
    process.start(QStringLiteral("gdbus"), arguments);
    if (!finished.wait(5000))
        return QStringLiteral("<timeout>");
    return QString::fromUtf8(process.readAllStandardOutput() + process.readAllStandardError()).trimmed();
}

void TestDBus::initTestCase()
{
    if (!QDBusConnection::sessionBus().isConnected())
        QSKIP("no D-Bus session bus");
    if (QStandardPaths::findExecutable(QStringLiteral("gdbus")).isEmpty())
        QSKIP("gdbus not installed");
    // The daemon is not started (no uinput in test environments); the D-Bus
    // layer works without devices.
    m_daemon = new Daemon(this);
    m_service = new DBusService(m_daemon);
    QString error;
    QVERIFY2(m_service->registerOnSessionBus(&error), qPrintable(error));
}

QStringList call(const QString &method, const QStringList &args = {})
{
    return QStringList{QStringLiteral("call"), QStringLiteral("--session"), QStringLiteral("--dest"),
                       QLatin1String(DBusService::ServiceName), QStringLiteral("--object-path"),
                       QLatin1String(DBusService::ObjectPath), QStringLiteral("--method"),
                       QStringLiteral("io.github.lordbooker.GKeyConfig1.") + method}
           + args;
}

void TestDBus::introspection()
{
    const QString xml = gdbus({QStringLiteral("introspect"), QStringLiteral("--session"), QStringLiteral("--dest"),
                               QLatin1String(DBusService::ServiceName), QStringLiteral("--object-path"),
                               QLatin1String(DBusService::ObjectPath)});
    for (const char *member : {"Status", "SetProfile", "SetMode", "SetBacklight", "ResetBacklight", "Reload",
                               "RunMacro", "StopMacros", "SetRecording", "LcdText", "LcdPage", "CalibrateStick",
                               "DeviceAdded", "DeviceRemoved",
                               "ProfileChanged", "ModeChanged", "KeyEvent", "RecordingChanged", "Recording"}) {
        QVERIFY2(xml.contains(QLatin1String(member)), member);
    }
}

void TestDBus::status()
{
    const QString reply = gdbus(call(QStringLiteral("Status")));
    // gdbus prints ('<json>',)
    const qsizetype start = reply.indexOf(u'{');
    const qsizetype end = reply.lastIndexOf(u'}');
    QVERIFY2(start >= 0 && end > start, qPrintable(reply));
    const QJsonObject status = QJsonDocument::fromJson(reply.mid(start, end - start + 1).toUtf8()).object();
    QCOMPARE(status.value(u"version").toString(), QStringLiteral(GKEY_VERSION));
    QCOMPARE(status.value(u"recording").toBool(true), false);
}

void TestDBus::recording()
{
    QSignalSpy spy(m_daemon, &Daemon::recordingChanged);
    QCOMPARE(gdbus(call(QStringLiteral("SetRecording"), {QStringLiteral("true")})), QStringLiteral("()"));
    QCOMPARE(spy.size(), 1);
    QVERIFY(m_daemon->recording());
    const QString property = gdbus({QStringLiteral("call"), QStringLiteral("--session"), QStringLiteral("--dest"),
                                    QLatin1String(DBusService::ServiceName), QStringLiteral("--object-path"),
                                    QLatin1String(DBusService::ObjectPath), QStringLiteral("--method"),
                                    QStringLiteral("org.freedesktop.DBus.Properties.Get"),
                                    QStringLiteral("io.github.lordbooker.GKeyConfig1"), QStringLiteral("Recording")});
    QVERIFY2(property.contains(QStringLiteral("true")), qPrintable(property));
    gdbus(call(QStringLiteral("SetRecording"), {QStringLiteral("false")}));
    QVERIFY(!m_daemon->recording());
}

void TestDBus::unknownModel()
{
    QCOMPARE(gdbus(call(QStringLiteral("SetMode"), {QStringLiteral("nope"), QStringLiteral("2")})),
             QStringLiteral("(false,)"));
    QCOMPARE(gdbus(call(QStringLiteral("SetBacklight"),
                        {QStringLiteral("nope"), QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3")})),
             QStringLiteral("(false,)"));
    QCOMPARE(gdbus(call(QStringLiteral("RunMacro"), {QStringLiteral("nope")})), QStringLiteral("(false,)"));
}

void TestDBus::secondInstanceIsRejected()
{
    // A second connection (as a second daemon would have) cannot take the name.
    QDBusConnection other = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("second"));
    QVERIFY(other.isConnected());
    const auto reply = other.interface()->registerService(QLatin1String(DBusService::ServiceName),
                                                          QDBusConnectionInterface::DontQueueService,
                                                          QDBusConnectionInterface::DontAllowReplacement);
    QVERIFY(reply.isValid());
    QCOMPARE(reply.value(), QDBusConnectionInterface::ServiceNotRegistered);
    QDBusConnection::disconnectFromBus(QStringLiteral("second"));
}

QTEST_GUILESS_MAIN(TestDBus)
#include "tst_dbus.moc"

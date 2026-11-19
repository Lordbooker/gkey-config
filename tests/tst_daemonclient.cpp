// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "DaemonClient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

#include <memory>

namespace {

const QString Service = QStringLiteral("io.github.lordbooker.GKeyConfig");
const QString Path = QStringLiteral("/io/github/lordbooker/GKeyConfig");
const QString BusName = QStringLiteral("fake-daemon");

}

// Stands in for gkeyd on its own bus connection and answers after a delay
// (or never), like a daemon that is busy or hangs.
class FakeDaemon : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.lordbooker.GKeyConfig1")

public:
    int delayMs = 0;
    bool answer = true;   // false: the daemon hangs
    QStringList calls;
    bool calibrated = false;

public slots:
    QString Status(const QDBusMessage &message)
    {
        QJsonObject status{{QStringLiteral("recording"), false}};
        if (calibrated)
            status.insert(QStringLiteral("devices"),
                          QJsonArray{QJsonObject{{QStringLiteral("model"), QStringLiteral("g13")},
                                                 {QStringLiteral("stickCenter"), QJsonArray{127, 129}}}});
        reply(message, QString::fromUtf8(QJsonDocument(status).toJson(QJsonDocument::Compact)));
        return {};
    }
    bool SetProfile(const QString &model, const QString &name, const QDBusMessage &message)
    {
        reply(message, model == u"g13" && !name.isEmpty());
        return false;
    }
    bool RunMacro(const QString &name, const QDBusMessage &message)
    {
        reply(message, name == u"hello");
        return false;
    }
    bool CalibrateStick(const QString &model, const QDBusMessage &message)
    {
        calibrated = model == u"g13";
        reply(message, calibrated);
        return false;
    }

private:
    void reply(const QDBusMessage &message, const QVariant &value)
    {
        calls.append(message.member());
        message.setDelayedReply(true);
        if (!answer)
            return;
        const QDBusMessage answerMessage = message.createReply(value);
        QTimer::singleShot(delayMs, this, [answerMessage] { QDBusConnection(BusName).send(answerMessage); });
    }
};

class TestDaemonClient : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();
    void callsDoNotWait();
    void callsKeepTheirOrder();
    void resultReachesTheCallback();
    void calibrationRefreshesTheStatus();
    void hangingDaemon();
    void callbackDroppedWithItsContext();

private:
    std::unique_ptr<FakeDaemon> m_fake;
};

void TestDaemonClient::initTestCase()
{
    if (!QDBusConnection::sessionBus().isConnected())
        QSKIP("no D-Bus session bus");
    QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, BusName);
    QVERIFY(bus.isConnected());
    m_fake = std::make_unique<FakeDaemon>();
    QVERIFY(bus.registerObject(Path, m_fake.get(), QDBusConnection::ExportAllSlots));
    QVERIFY(bus.registerService(Service));
}

void TestDaemonClient::init()
{
    m_fake->delayMs = 0;
    m_fake->answer = true;
    m_fake->calls.clear();
    m_fake->calibrated = false;
}

void TestDaemonClient::cleanupTestCase()
{
    QDBusConnection(BusName).unregisterService(Service);
    QDBusConnection::disconnectFromBus(BusName);
}

// The window must not wait for a slow daemon.
void TestDaemonClient::callsDoNotWait()
{
    DaemonClient client;
    QTRY_VERIFY(client.available());
    QTRY_VERIFY(m_fake->calls.contains(u"Status"));   // first status at start

    m_fake->delayMs = 1000;
    QElapsedTimer timer;
    timer.start();
    client.setProfile(QStringLiteral("g13"), QStringLiteral("Alpha"));
    client.refreshStatus();
    QVERIFY2(timer.elapsed() < 200, qPrintable(QString::number(timer.elapsed())));
    QSignalSpy status(&client, &DaemonClient::statusChanged);
    QVERIFY(status.wait(3000));
}

void TestDaemonClient::callsKeepTheirOrder()
{
    DaemonClient client;
    QTRY_VERIFY(client.available());
    QTRY_VERIFY(!m_fake->calls.isEmpty());
    m_fake->calls.clear();
    client.setProfile(QStringLiteral("g13"), QStringLiteral("Alpha"));
    client.runMacro(QStringLiteral("hello"));
    client.refreshStatus();
    QTRY_COMPARE(m_fake->calls,
                 (QStringList{QStringLiteral("SetProfile"), QStringLiteral("RunMacro"), QStringLiteral("Status")}));
}

void TestDaemonClient::resultReachesTheCallback()
{
    DaemonClient client;
    QTRY_VERIFY(client.available());
    m_fake->delayMs = 100;
    std::optional<bool> known;
    std::optional<bool> unknown;
    client.runMacro(QStringLiteral("hello"), this, [&known](bool started) { known = started; });
    client.runMacro(QStringLiteral("nope"), this, [&unknown](bool started) { unknown = started; });
    QVERIFY(!known);   // not before the answer
    QTRY_VERIFY(known && unknown);
    QVERIFY(*known);
    QVERIFY(!*unknown);
}

// The callback of the calibration may read the new rest position at once.
void TestDaemonClient::calibrationRefreshesTheStatus()
{
    DaemonClient client;
    QTRY_VERIFY(client.available());
    m_fake->delayMs = 50;
    QString center;
    bool called = false;
    client.calibrateStick(QStringLiteral("g13"), this, [&](bool calibrated) {
        called = true;
        QVERIFY(calibrated);
        center = client.stickCenter(QStringLiteral("g13"));
    });
    QTRY_VERIFY(called);
    QCOMPARE(center, QStringLiteral("127 / 129"));
}

// A daemon that never answers: the call fails after the timeout instead of
// freezing the window.
void TestDaemonClient::hangingDaemon()
{
    DaemonClient client;
    QTRY_VERIFY(client.available());
    m_fake->answer = false;
    QElapsedTimer timer;
    timer.start();
    std::optional<bool> result;
    client.runMacro(QStringLiteral("hello"), this, [&result](bool started) { result = started; });
    QVERIFY(timer.elapsed() < 200);
    QTRY_VERIFY_WITH_TIMEOUT(result.has_value(), 6000);
    QVERIFY(!*result);
}

// An answer that arrives after its window was closed is dropped.
void TestDaemonClient::callbackDroppedWithItsContext()
{
    DaemonClient client;
    QTRY_VERIFY(client.available());
    m_fake->delayMs = 200;
    bool called = false;
    {
        QObject context;
        client.runMacro(QStringLiteral("hello"), &context, [&called](bool) { called = true; });
    }
    QTest::qWait(500);
    QVERIFY(!called);
}

QTEST_GUILESS_MAIN(TestDaemonClient)
#include "tst_daemonclient.moc"

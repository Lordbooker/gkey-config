// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "Daemon.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <linux/input-event-codes.h>

#include <memory>

namespace {

class RecordingSink : public OutputSink {
public:
    void writeKey(int code, bool pressed) override
    {
        events.append((pressed ? QStringLiteral("+%1") : QStringLiteral("-%1")).arg(code));
    }
    void sync() override {}
    QStringList events;
};

void writeFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

}

// The daemon's logic without devices: profiles and modes, their persistence,
// reloading and macros. Output goes to a recording sink instead of uinput.
class TestDaemon : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void startsWithoutDevices();
    void profileSwitching();
    void modeSwitching();
    void stateIsRemembered();
    void reloadAfterExternalChanges();
    void macroThroughDBusFunction();

private:
    QJsonObject status() const;

    std::unique_ptr<QTemporaryDir> m_dir;   // configuration and state of one test
    RecordingSink m_sink;
    std::unique_ptr<Daemon> m_daemon;
};

void TestDaemon::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    qputenv("XDG_CONFIG_HOME", m_dir->filePath(QStringLiteral("config")).toLocal8Bit());
    qputenv("XDG_STATE_HOME", m_dir->filePath(QStringLiteral("state")).toLocal8Bit());
    qputenv("GKEY_MODEL_DIR", GKEY_SOURCE_MODEL_DIR);

    writeFile(m_dir->filePath(QStringLiteral("config/gkey-config/profiles/g13/a.json")), R"({"name":"Alpha"})");
    writeFile(m_dir->filePath(QStringLiteral("config/gkey-config/profiles/g13/b.json")), R"({"name":"Beta"})");
    m_sink.events.clear();

    m_daemon = std::make_unique<Daemon>();
    QString error;
    QVERIFY2(m_daemon->start(&error, {&m_sink, false}), qPrintable(error));
}

void TestDaemon::cleanup()
{
    m_daemon.reset();
}

QJsonObject TestDaemon::status() const
{
    return QJsonDocument::fromJson(m_daemon->statusJson()).object();
}

void TestDaemon::startsWithoutDevices()
{
    const QJsonObject root = status();
    QVERIFY(root.value(u"devices").toArray().isEmpty());
    QVERIFY(root.value(u"models").toObject().contains(u"g13"));
    QVERIFY(root.value(u"models").toObject().contains(u"g15"));
}

void TestDaemon::profileSwitching()
{
    QSignalSpy changed(m_daemon.get(), &Daemon::profileChanged);
    QVERIFY(m_daemon->setProfile(QStringLiteral("g13"), QStringLiteral("Beta")));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.at(0).at(1).toString(), QStringLiteral("Beta"));
    const QJsonObject g13 = status().value(u"models").toObject().value(u"g13").toObject();
    QCOMPARE(g13.value(u"profile").toString(), QStringLiteral("Beta"));
    QCOMPARE(g13.value(u"profiles").toArray().size(), 2);

    QVERIFY(m_daemon->setProfile(QStringLiteral("g13"), QStringLiteral("Beta")));   // already active
    QCOMPARE(changed.count(), 1);
    QVERIFY(!m_daemon->setProfile(QStringLiteral("g13"), QStringLiteral("Gamma")));
    QVERIFY(!m_daemon->setProfile(QStringLiteral("nope"), QStringLiteral("Alpha")));
}

void TestDaemon::modeSwitching()
{
    QSignalSpy changed(m_daemon.get(), &Daemon::modeChanged);
    QVERIFY(m_daemon->setMode(QStringLiteral("g13"), 3));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(status().value(u"models").toObject().value(u"g13").toObject().value(u"mode").toInt(), 3);
    QVERIFY(!m_daemon->setMode(QStringLiteral("g13"), 0));
    QVERIFY(!m_daemon->setMode(QStringLiteral("g13"), Profile::ModeCount + 1));
    QCOMPARE(changed.count(), 1);
}

void TestDaemon::stateIsRemembered()
{
    QVERIFY(m_daemon->setProfile(QStringLiteral("g13"), QStringLiteral("Beta")));
    QVERIFY(m_daemon->setMode(QStringLiteral("g13"), 2));
    m_daemon.reset();

    m_daemon = std::make_unique<Daemon>();
    QString error;
    QVERIFY2(m_daemon->start(&error, {&m_sink, false}), qPrintable(error));
    QSignalSpy modeChanged(m_daemon.get(), &Daemon::modeChanged);
    // Creates the model context from the state file: M2 is already active.
    QVERIFY(m_daemon->setMode(QStringLiteral("g13"), 2));
    QCOMPARE(modeChanged.count(), 0);
    const QJsonObject g13 = status().value(u"models").toObject().value(u"g13").toObject();
    QCOMPARE(g13.value(u"profile").toString(), QStringLiteral("Beta"));
    QCOMPARE(g13.value(u"mode").toInt(), 2);
}

void TestDaemon::reloadAfterExternalChanges()
{
    QVERIFY(m_daemon->setProfile(QStringLiteral("g13"), QStringLiteral("Beta")));
    QSignalSpy changed(m_daemon.get(), &Daemon::profileChanged);

    // A new profile appears, the active one is deleted (e.g. in a file manager).
    writeFile(m_dir->filePath(QStringLiteral("config/gkey-config/profiles/g13/c.json")), R"({"name":"Gamma"})");
    QVERIFY(QFile::remove(m_dir->filePath(QStringLiteral("config/gkey-config/profiles/g13/b.json"))));
    m_daemon->reload();

    const QJsonObject g13 = status().value(u"models").toObject().value(u"g13").toObject();
    QCOMPARE(g13.value(u"profiles").toArray().size(), 2);
    QCOMPARE(g13.value(u"profile").toString(), QStringLiteral("Alpha"));   // first remaining one
    QCOMPARE(changed.count(), 1);
}

void TestDaemon::macroThroughDBusFunction()
{
    writeFile(m_dir->filePath(QStringLiteral("config/gkey-config/macros/hello.json")),
              R"({"name":"hello","steps":[{"tap":"KEY_H","hold":0}]})");
    m_daemon->reload();
    QVERIFY(!m_daemon->runMacro(QStringLiteral("unknown")));
    QVERIFY(m_daemon->runMacro(QStringLiteral("hello")));
    QTRY_COMPARE(m_sink.events, (QStringList{QStringLiteral("+%1").arg(KEY_H), QStringLiteral("-%1").arg(KEY_H)}));
}

QTEST_GUILESS_MAIN(TestDaemon)
#include "tst_daemon.moc"

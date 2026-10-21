#include "BindingEngine.h"
#include "KeyOutput.h"
#include "Macro.h"
#include "MacroRun.h"
#include "MacroStore.h"
#include "ModelRegistry.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <linux/input-event-codes.h>

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

QString down(int code) { return QStringLiteral("+%1").arg(code); }
QString up(int code) { return QStringLiteral("-%1").arg(code); }

Macro parse(const QByteArray &json)
{
    auto macro = Macro::fromJson(json);
    if (!macro)
        qFatal("invalid test macro: %s", qPrintable(macro.error()));
    return *macro;
}

}

class TestMacros : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void parsing_data();
    void parsing();
    void roundTrip();
    void playOnce();
    void playCount();
    void stopReleasesHeldKeys();
    void store();
    void engineWhileHeld();
    void engineToggleStopsOnSecondPress();
    void engineModeChangeStopsMacro();
    void engineMissingMacro();

private:
    ModelRegistry m_registry;
};

void TestMacros::initTestCase()
{
    m_registry.load({QStringLiteral(GKEY_SOURCE_MODEL_DIR)});
    QVERIFY(m_registry.findById(QStringLiteral("g13")));
}

void TestMacros::parsing_data()
{
    QTest::addColumn<QByteArray>("json");
    QTest::addColumn<QString>("expectedError");

    QTest::newRow("valid") << QByteArray(R"({"name":"m","steps":[{"tap":"KEY_A"},{"delay":5},
        {"press":"BTN_LEFT"},{"release":"BTN_LEFT"},{"tap":"KEY_B","hold":0}]})") << QString();
    QTest::newRow("empty steps") << QByteArray(R"({"name":"m"})") << QString();
    QTest::newRow("no name") << QByteArray(R"({"steps":[]})") << QStringLiteral("name");
    QTest::newRow("unknown step") << QByteArray(R"({"name":"m","steps":[{"jump":1}]})") << QStringLiteral("step 1");
    QTest::newRow("unknown key") << QByteArray(R"({"name":"m","steps":[{"tap":"KEY_NOPE"}]})") << QStringLiteral("unknown key");
    QTest::newRow("joystick button")
        << QByteArray(R"({"name":"m","steps":[{"press":"BTN_THUMB"}]})") << QStringLiteral("neither");
    QTest::newRow("negative delay") << QByteArray(R"({"name":"m","steps":[{"delay":-1}]})") << QStringLiteral("delay");
    QTest::newRow("huge delay") << QByteArray(R"({"name":"m","steps":[{"delay":600000}]})") << QStringLiteral("delay");
}

void TestMacros::parsing()
{
    QFETCH(QByteArray, json);
    QFETCH(QString, expectedError);
    const auto macro = Macro::fromJson(json);
    if (expectedError.isEmpty()) {
        QVERIFY2(macro.has_value(), macro ? "" : qPrintable(macro.error()));
    } else {
        QVERIFY(!macro.has_value());
        QVERIFY2(macro.error().contains(expectedError), qPrintable(macro.error()));
    }
}

void TestMacros::roundTrip()
{
    const Macro macro = parse(R"({"name":"Copy","description":"d","future":1,
        "steps":[{"press":"KEY_LEFTCTRL"},{"tap":"KEY_C","hold":25},{"release":"KEY_LEFTCTRL"},{"delay":40}]})");
    const Macro again = parse(macro.toJson());
    QCOMPARE(again.name(), QStringLiteral("Copy"));
    QCOMPARE(again.description(), QStringLiteral("d"));
    QCOMPARE(again.steps(), macro.steps());
    QCOMPARE(again.steps().at(1).duration, 25);
    QVERIFY(again.toJson().contains("\"future\""));
}

void TestMacros::playOnce()
{
    RecordingSink sink;
    KeyOutput output(sink);
    MacroRun run(parse(R"({"name":"m","steps":[{"tap":"KEY_A","hold":5},{"delay":5},{"press":"KEY_B"},{"release":"KEY_B"}]})"),
                 Action::Repeat::Once, 1, output);
    QSignalSpy finished(&run, &MacroRun::finished);
    run.start();
    QVERIFY(run.isRunning());
    QVERIFY(finished.wait(1000));
    QCOMPARE(sink.events, (QStringList{down(KEY_A), up(KEY_A), down(KEY_B), up(KEY_B)}));
    QVERIFY(!run.isRunning());
}

void TestMacros::playCount()
{
    RecordingSink sink;
    KeyOutput output(sink);
    MacroRun run(parse(R"({"name":"m","steps":[{"tap":"KEY_A","hold":1}]})"), Action::Repeat::Count, 3, output);
    QSignalSpy finished(&run, &MacroRun::finished);
    run.start();
    QVERIFY(finished.wait(1000));
    QCOMPARE(sink.events, (QStringList{down(KEY_A), up(KEY_A), down(KEY_A), up(KEY_A), down(KEY_A), up(KEY_A)}));
}

void TestMacros::stopReleasesHeldKeys()
{
    RecordingSink sink;
    KeyOutput output(sink);
    MacroRun run(parse(R"({"name":"m","steps":[{"press":"KEY_LEFTSHIFT"},{"delay":5000},{"release":"KEY_LEFTSHIFT"}]})"),
                 Action::Repeat::Once, 1, output);
    QSignalSpy finished(&run, &MacroRun::finished);
    run.start();
    QCOMPARE(sink.events, QStringList{down(KEY_LEFTSHIFT)});
    run.stop();   // must not wait for the 5 s delay
    QCOMPARE(finished.size(), 1);
    QCOMPARE(sink.events, (QStringList{down(KEY_LEFTSHIFT), up(KEY_LEFTSHIFT)}));
    QCOMPARE(output.pressCount(KEY_LEFTSHIFT), 0);
}

void TestMacros::store()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    MacroStore store(dir.filePath(QStringLiteral("macros")));
    QVERIFY(store.names().isEmpty());

    QVERIFY(store.save(parse(R"({"name":"Hello World","steps":[{"tap":"KEY_H"}]})")));
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral("macros/hello-world.json"))));
    QVERIFY(store.macro(QStringLiteral("Hello World")));

    QSignalSpy spy(&store, &MacroStore::changed);
    QFile file(dir.filePath(QStringLiteral("macros/other.json")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"name":"Other","steps":[{"delay":1}]})");
    file.close();
    QVERIFY(spy.wait(3000));
    QCOMPARE(store.names(), (QStringList{QStringLiteral("Hello World"), QStringLiteral("Other")}));

    QVERIFY(store.remove(QStringLiteral("Hello World")));
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("macros/hello-world.json"))));
    QVERIFY(!store.remove(QStringLiteral("Hello World")));
}

void TestMacros::engineWhileHeld()
{
    QTemporaryDir dir;
    MacroStore macros(dir.filePath(QStringLiteral("macros")));
    QVERIFY(macros.save(parse(R"({"name":"spam","steps":[{"tap":"KEY_A","hold":2},{"delay":20}]})")));

    Profile profile;
    profile.setName(QStringLiteral("p"));
    profile.setBinding(1, QStringLiteral("G1"),
                       *Action::fromJson(QJsonObject{{"type", "macro"}, {"macro", "spam"}, {"repeat", "while_held"}}));

    RecordingSink sink;
    KeyOutput output(sink);
    BindingEngine engine(*m_registry.findById(QStringLiteral("g13")), output);
    engine.setMacroStore(&macros);
    engine.setProfile(profile);

    engine.handleKey(QStringLiteral("keys"), KEY_MACRO1, 1);
    QTest::qWait(150);   // several passes of ~22 ms
    engine.handleKey(QStringLiteral("keys"), KEY_MACRO1, 0);
    QTRY_COMPARE(engine.runningMacros(), 0);
    const qsizetype taps = sink.events.count(down(KEY_A));
    QVERIFY2(taps >= 2, qPrintable(sink.events.join(u' ')));
    QCOMPARE(sink.events.count(up(KEY_A)), taps);   // the last pass was completed
}

void TestMacros::engineToggleStopsOnSecondPress()
{
    QTemporaryDir dir;
    MacroStore macros(dir.filePath(QStringLiteral("macros")));
    QVERIFY(macros.save(parse(R"({"name":"hold","steps":[{"press":"KEY_W"},{"delay":10}]})")));

    Profile profile;
    profile.setName(QStringLiteral("p"));
    profile.setBinding(1, QStringLiteral("G5"),
                       *Action::fromJson(QJsonObject{{"type", "macro"}, {"macro", "hold"}, {"repeat", "toggle"}}));

    RecordingSink sink;
    KeyOutput output(sink);
    BindingEngine engine(*m_registry.findById(QStringLiteral("g13")), output);
    engine.setMacroStore(&macros);
    engine.setProfile(profile);

    engine.handleKey(QStringLiteral("keys"), KEY_MACRO5, 1);
    engine.handleKey(QStringLiteral("keys"), KEY_MACRO5, 0);
    QTest::qWait(40);
    QCOMPARE(engine.runningMacros(), 1);   // toggle keeps running after release
    engine.handleKey(QStringLiteral("keys"), KEY_MACRO5, 1);
    engine.handleKey(QStringLiteral("keys"), KEY_MACRO5, 0);
    QCOMPARE(engine.runningMacros(), 0);
    QCOMPARE(sink.events, (QStringList{down(KEY_W), up(KEY_W)}));
}

void TestMacros::engineModeChangeStopsMacro()
{
    QTemporaryDir dir;
    MacroStore macros(dir.filePath(QStringLiteral("macros")));
    QVERIFY(macros.save(parse(R"({"name":"long","steps":[{"press":"KEY_LEFTALT"},{"delay":5000}]})")));

    Profile profile;
    profile.setName(QStringLiteral("p"));
    profile.setBinding(1, QStringLiteral("G1"),
                       *Action::fromJson(QJsonObject{{"type", "macro"}, {"macro", "long"}}));

    RecordingSink sink;
    KeyOutput output(sink);
    BindingEngine engine(*m_registry.findById(QStringLiteral("g13")), output);
    engine.setMacroStore(&macros);
    engine.setProfile(profile);

    engine.handleKey(QStringLiteral("keys"), KEY_MACRO1, 1);
    QCOMPARE(engine.runningMacros(), 1);
    engine.setMode(2);
    QCOMPARE(engine.runningMacros(), 0);
    QCOMPARE(sink.events, (QStringList{down(KEY_LEFTALT), up(KEY_LEFTALT)}));
}

void TestMacros::engineMissingMacro()
{
    Profile profile;
    profile.setName(QStringLiteral("p"));
    profile.setBinding(1, QStringLiteral("G1"),
                       *Action::fromJson(QJsonObject{{"type", "macro"}, {"macro", "nothere"}}));
    RecordingSink sink;
    KeyOutput output(sink);
    BindingEngine engine(*m_registry.findById(QStringLiteral("g13")), output);
    engine.setProfile(profile);
    engine.handleKey(QStringLiteral("keys"), KEY_MACRO1, 1);
    engine.handleKey(QStringLiteral("keys"), KEY_MACRO1, 0);
    QCOMPARE(engine.runningMacros(), 0);
    QVERIFY(sink.events.isEmpty());
}

QTEST_GUILESS_MAIN(TestMacros)
#include "tst_macros.moc"

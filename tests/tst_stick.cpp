// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "BindingEngine.h"
#include "KeyOutput.h"
#include "ModelRegistry.h"
#include "StickProcessor.h"

#include <QSignalSpy>
#include <QTest>

#include <linux/input-event-codes.h>

namespace {

class RecordingSink : public OutputSink {
public:
    void writeKey(int code, bool pressed) override
    {
        events.append((pressed ? QStringLiteral("+%1") : QStringLiteral("-%1")).arg(code));
    }
    void writeRel(int code, int value) override
    {
        if (code == REL_X)
            relX += value;
        else if (code == REL_Y)
            relY += value;
        else if (code == REL_WHEEL)
            wheel += value;
        else if (code == REL_HWHEEL)
            hwheel += value;
    }
    void sync() override {}
    QStringList events;
    int relX = 0;
    int relY = 0;
    int wheel = 0;
    int hwheel = 0;
};

QString down(int code) { return QStringLiteral("+%1").arg(code); }
QString up(int code) { return QStringLiteral("-%1").arg(code); }

}

class TestStick : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void settingsParsing();
    void profileDefaultIsPassthrough();
    void keysWithDiagonal();
    void keysHysteresis();
    void keysUseRestPosition();
    void mouseMovement();
    void scrollDirection();
    void passthroughDoesNothing();

private:
    ModelRegistry m_registry;
    RecordingSink *m_sink = nullptr;
    KeyOutput *m_output = nullptr;
    BindingEngine *m_engine = nullptr;
    StickProcessor *m_stick = nullptr;
};

void TestStick::initTestCase()
{
    m_registry.load({QStringLiteral(GKEY_SOURCE_MODEL_DIR)});
    QVERIFY(m_registry.findById(QStringLiteral("g13")));
}

void TestStick::init()
{
    m_sink = new RecordingSink;
    m_output = new KeyOutput(*m_sink);
    m_engine = new BindingEngine(*m_registry.findById(QStringLiteral("g13")), *m_output);
    Profile profile;
    profile.setName(QStringLiteral("p"));
    profile.setBinding(1, QStringLiteral("STICK_UP"), Action::key(KEY_W));
    profile.setBinding(1, QStringLiteral("STICK_DOWN"), Action::key(KEY_S));
    profile.setBinding(1, QStringLiteral("STICK_LEFT"), Action::key(KEY_A));
    profile.setBinding(1, QStringLiteral("STICK_RIGHT"), Action::key(KEY_D));
    m_engine->setProfile(profile);
    m_stick = new StickProcessor(*m_engine, *m_output, 0, 255);
    m_stick->setCenter(128, 128);
}

void TestStick::cleanup()
{
    delete m_stick;
    delete m_engine;
    delete m_output;
    delete m_sink;
}

void TestStick::settingsParsing()
{
    QStringList warnings;
    StickSettings settings = StickSettings::fromJson(
        QJsonObject{{"mode", "mouse"}, {"deadzone", 0.3}, {"speed", 20}, {"scrollSpeed", 5}}, &warnings);
    QVERIFY(warnings.isEmpty());
    QCOMPARE(settings.mode, StickSettings::Mode::Mouse);
    QCOMPARE(settings.deadzone, 0.3);
    QCOMPARE(settings.speed, 20);
    QCOMPARE(settings.scrollSpeed, 5);

    settings = StickSettings::fromJson(QJsonObject{{"mode", "warp"}, {"deadzone", 2.0}, {"speed", 0}}, &warnings);
    QCOMPARE(warnings.size(), 3);
    QCOMPARE(settings, StickSettings());

    QJsonObject json{{"future", 1}};
    StickSettings keys;
    keys.mode = StickSettings::Mode::Keys;
    keys.writeTo(json);
    QCOMPARE(json.value(u"mode").toString(), QStringLiteral("keys"));
    QCOMPARE(json.value(u"future").toInt(), 1);
    QCOMPARE(StickSettings::fromJson(json), keys);
}

void TestStick::profileDefaultIsPassthrough()
{
    const auto profile = Profile::fromJson(R"({"name":"old"})");
    QVERIFY(profile);
    QCOMPARE(profile->stickSettings().mode, StickSettings::Mode::Passthrough);

    Profile changed = *profile;
    StickSettings settings;
    settings.mode = StickSettings::Mode::Scroll;
    changed.setStickSettings(settings);
    QCOMPARE(Profile::fromJson(changed.toJson())->stickSettings().mode, StickSettings::Mode::Scroll);
}

void TestStick::keysWithDiagonal()
{
    StickSettings settings;
    settings.mode = StickSettings::Mode::Keys;
    m_stick->setSettings(settings);

    m_stick->setPosition(128, 0);      // up
    QCOMPARE(m_sink->events, QStringList{down(KEY_W)});
    m_stick->setPosition(255, 0);      // up + right
    QCOMPARE(m_sink->events, (QStringList{down(KEY_W), down(KEY_D)}));
    m_stick->setPosition(255, 128);    // right only
    QCOMPARE(m_sink->events, (QStringList{down(KEY_W), down(KEY_D), up(KEY_W)}));
    m_stick->setPosition(0, 255);      // down + left
    QCOMPARE(m_sink->events, (QStringList{down(KEY_W), down(KEY_D), up(KEY_W), down(KEY_S), down(KEY_A), up(KEY_D)}));
    m_stick->setPosition(128, 128);    // centre: all released
    QCOMPARE(m_output->pressCount(KEY_S) + m_output->pressCount(KEY_A), 0);
}

void TestStick::keysHysteresis()
{
    StickSettings settings;
    settings.mode = StickSettings::Mode::Keys;
    settings.deadzone = 0.25;
    m_stick->setSettings(settings);

    // Up direction: dead zone 0.25 of 128 = 32 -> pressed below y = 96.
    m_stick->setY(95);
    QCOMPARE(m_sink->events, QStringList{down(KEY_W)});
    m_stick->setY(98);                 // inside the hysteresis band: stays pressed
    QCOMPARE(m_sink->events, QStringList{down(KEY_W)});
    m_stick->setY(104);                // below 0.20: released
    QCOMPARE(m_sink->events, (QStringList{down(KEY_W), up(KEY_W)}));
    m_stick->setY(98);                 // not yet beyond 0.25 again
    QCOMPARE(m_sink->events.size(), 2);
}

void TestStick::keysUseRestPosition()
{
    StickSettings settings;
    settings.mode = StickSettings::Mode::Keys;
    m_stick->setSettings(settings);
    // Rest position of the test G13 is about 138/139: values around it must not press keys.
    m_stick->setCenter(138, 139);
    m_stick->setPosition(138, 139);
    m_stick->setPosition(160, 160);    // would be "right"+"down" with centre 128 (0.25 -> 160)
    QVERIFY(m_sink->events.isEmpty());
}

void TestStick::mouseMovement()
{
    StickSettings settings;
    settings.mode = StickSettings::Mode::Mouse;
    settings.speed = 10;
    m_stick->setSettings(settings);

    m_stick->setPosition(255, 128);    // full right
    QTest::qWait(100);
    m_stick->setPosition(128, 128);
    QVERIFY2(m_sink->relX >= 50, qPrintable(QString::number(m_sink->relX)));   // ~10 px per 10 ms
    QCOMPARE(m_sink->relY, 0);
    const int x = m_sink->relX;
    QTest::qWait(50);
    QCOMPARE(m_sink->relX, x);         // stopped at the centre
}

void TestStick::scrollDirection()
{
    StickSettings settings;
    settings.mode = StickSettings::Mode::Scroll;
    settings.scrollSpeed = 50;         // 50 steps per second at full deflection
    m_stick->setSettings(settings);
    m_stick->setPosition(128, 0);      // stick up -> wheel up (positive)
    QTest::qWait(150);
    m_stick->setPosition(128, 128);
    QVERIFY2(m_sink->wheel > 0, qPrintable(QString::number(m_sink->wheel)));
    QCOMPARE(m_sink->hwheel, 0);
}

void TestStick::passthroughDoesNothing()
{
    m_stick->setSettings(StickSettings());   // default: passthrough
    m_stick->setPosition(0, 0);
    QTest::qWait(30);
    QVERIFY(m_sink->events.isEmpty());
    QCOMPARE(m_sink->relX + m_sink->relY, 0);
}

QTEST_GUILESS_MAIN(TestStick)
#include "tst_stick.moc"

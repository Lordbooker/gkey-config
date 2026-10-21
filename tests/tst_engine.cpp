// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "BindingEngine.h"
#include "KeyOutput.h"
#include "ModelRegistry.h"

#include <QSignalSpy>
#include <QTest>

#include <linux/input-event-codes.h>

namespace {

// Records what would be written to the virtual device.
class RecordingSink : public OutputSink {
public:
    void writeKey(int code, bool pressed) override
    {
        events.append((pressed ? QStringLiteral("+%1") : QStringLiteral("-%1")).arg(code));
    }
    void sync() override { ++syncs; }

    QStringList events;
    int syncs = 0;
};

QString down(int code)
{
    return QStringLiteral("+%1").arg(code);
}

QString up(int code)
{
    return QStringLiteral("-%1").arg(code);
}

}

class TestEngine : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void keyPressAndRelease();
    void comboOrder();
    void unboundAndUnmappedKeys();
    void modes();
    void modeKeyAndActions();
    void profileSwitchReleasesHeldKeys();
    void sharedOutputReferenceCounting();
    void ignoresAutorepeatAndHardwareKeys();
    void recordKey();

private:
    const DeviceModel &g13() const { return *m_registry.findById(QStringLiteral("g13")); }

    ModelRegistry m_registry;
    Profile m_profile;
    RecordingSink *m_sink = nullptr;
    KeyOutput *m_output = nullptr;
    BindingEngine *m_engine = nullptr;
};

void TestEngine::initTestCase()
{
    m_registry.load({QStringLiteral(GKEY_SOURCE_MODEL_DIR)});
    QVERIFY(m_registry.findById(QStringLiteral("g13")));

    m_profile.setName(QStringLiteral("Test"));
    m_profile.setBinding(1, QStringLiteral("G1"), Action::key(KEY_A));
    m_profile.setBinding(1, QStringLiteral("G2"), Action::combo({KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_T}));
    m_profile.setBinding(1, QStringLiteral("G3"), Action::mouse(BTN_LEFT));
    m_profile.setBinding(1, QStringLiteral("G4"), Action::key(KEY_A));
    m_profile.setBinding(1, QStringLiteral("L1"), Action::mode(2));
    m_profile.setBinding(1, QStringLiteral("L2"), Action::profile(QStringLiteral("next")));
    m_profile.setBinding(2, QStringLiteral("G1"), Action::key(KEY_B));
}

void TestEngine::init()
{
    m_sink = new RecordingSink;
    m_output = new KeyOutput(*m_sink);
    m_engine = new BindingEngine(g13(), *m_output);
    m_engine->setProfile(m_profile);
}

void TestEngine::cleanup()
{
    delete m_engine;
    delete m_output;
    delete m_sink;
}

void TestEngine::keyPressAndRelease()
{
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO1, 1);
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO1, 0);
    QCOMPARE(m_sink->events, (QStringList{down(KEY_A), up(KEY_A)}));
    QCOMPARE(m_sink->syncs, 2);

    m_sink->events.clear();
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO3, 1);
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO3, 0);
    QCOMPARE(m_sink->events, (QStringList{down(BTN_LEFT), up(BTN_LEFT)}));
}

void TestEngine::comboOrder()
{
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO2, 1);
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO2, 0);
    QCOMPARE(m_sink->events, (QStringList{down(KEY_LEFTCTRL), down(KEY_LEFTSHIFT), down(KEY_T),
                                          up(KEY_T), up(KEY_LEFTSHIFT), up(KEY_LEFTCTRL)}));
}

void TestEngine::unboundAndUnmappedKeys()
{
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO22, 1);    // unbound
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO22, 0);
    m_engine->handleKey(QStringLiteral("keys"), KEY_A, 1);          // not a G13 key
    m_engine->handleKey(QStringLiteral("stick"), KEY_MACRO1, 1);    // wrong input role
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO1, 0);     // release without press
    QVERIFY(m_sink->events.isEmpty());
}

void TestEngine::modes()
{
    m_engine->setMode(2);
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO1, 1);
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO1, 0);
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO3, 1);   // not bound in M2
    QCOMPARE(m_sink->events, (QStringList{down(KEY_B), up(KEY_B)}));
}

void TestEngine::modeKeyAndActions()
{
    QSignalSpy modeSpy(m_engine, &BindingEngine::modeRequested);
    QSignalSpy profileSpy(m_engine, &BindingEngine::profileRequested);

    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO_PRESET3, 1);
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO_PRESET3, 0);
    m_engine->handleKey(QStringLiteral("keys"), KEY_KBD_LCD_MENU1, 1);   // L1: mode action
    m_engine->handleKey(QStringLiteral("keys"), KEY_KBD_LCD_MENU1, 0);
    m_engine->handleKey(QStringLiteral("keys"), KEY_KBD_LCD_MENU2, 1);   // L2: profile action
    m_engine->handleKey(QStringLiteral("keys"), KEY_KBD_LCD_MENU2, 0);

    QCOMPARE(modeSpy.size(), 2);
    QCOMPARE(modeSpy.at(0).at(0).toInt(), 3);
    QCOMPARE(modeSpy.at(1).at(0).toInt(), 2);
    QCOMPARE(profileSpy.size(), 1);
    QCOMPARE(profileSpy.at(0).at(0).toString(), QStringLiteral("next"));
    QVERIFY(m_sink->events.isEmpty());
}

void TestEngine::profileSwitchReleasesHeldKeys()
{
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO2, 1);   // combo held
    m_engine->setMode(2);                                          // must release it
    QCOMPARE(m_sink->events, (QStringList{down(KEY_LEFTCTRL), down(KEY_LEFTSHIFT), down(KEY_T),
                                          up(KEY_T), up(KEY_LEFTSHIFT), up(KEY_LEFTCTRL)}));

    m_sink->events.clear();
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO2, 0);   // late release: nothing more
    QVERIFY(m_sink->events.isEmpty());

    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO1, 1);   // M2: KEY_B
    m_engine->setProfile(m_profile);
    QCOMPARE(m_sink->events, (QStringList{down(KEY_B), up(KEY_B)}));
}

void TestEngine::sharedOutputReferenceCounting()
{
    // G1 and G4 are both bound to KEY_A; a second device shares the output.
    BindingEngine second(g13(), *m_output);
    second.setProfile(m_profile);

    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO1, 1);
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO4, 1);
    second.handleKey(QStringLiteral("keys"), KEY_MACRO1, 1);
    QCOMPARE(m_sink->events, QStringList{down(KEY_A)});

    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO1, 0);
    second.handleKey(QStringLiteral("keys"), KEY_MACRO1, 0);
    QCOMPARE(m_sink->events, QStringList{down(KEY_A)});

    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO4, 0);
    QCOMPARE(m_sink->events, (QStringList{down(KEY_A), up(KEY_A)}));
    QCOMPARE(m_output->pressCount(KEY_A), 0);
}

void TestEngine::ignoresAutorepeatAndHardwareKeys()
{
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO1, 1);
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO1, 2);
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO1, 1);   // duplicate press
    m_engine->handleKey(QStringLiteral("keys"), KEY_LIGHTS_TOGGLE, 1);
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO1, 0);
    QCOMPARE(m_sink->events, (QStringList{down(KEY_A), up(KEY_A)}));
}

void TestEngine::recordKey()
{
    QSignalSpy spy(m_engine, &BindingEngine::recordRequested);
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO_RECORD_START, 1);
    m_engine->handleKey(QStringLiteral("keys"), KEY_MACRO_RECORD_START, 0);
    QCOMPARE(spy.size(), 1);
}

QTEST_GUILESS_MAIN(TestEngine)
#include "tst_engine.moc"

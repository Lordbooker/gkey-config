#include "Action.h"
#include "DaemonState.h"
#include "ModelRegistry.h"
#include "Profile.h"
#include "ProfileStore.h"

#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <linux/input-event-codes.h>

class TestProfiles : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void actions_data();
    void actions();
    void actionRoundTrip();
    void profileParsing();
    void profileRoundTrip();
    void profileValidation();
    void storeLoadSaveAndReload();
    void storeDuplicateNames();
    void storeRenameAndRemove();
    void state();

private:
    const DeviceModel *g13() const { return m_registry.findById(QStringLiteral("g13")); }
    ModelRegistry m_registry;
};

namespace {

QJsonObject object(const QByteArray &json)
{
    return QJsonDocument::fromJson(json).object();
}

void writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

}

void TestProfiles::initTestCase()
{
    m_registry.load({QStringLiteral(GKEY_SOURCE_MODEL_DIR)});
    QVERIFY(g13());
}

void TestProfiles::actions_data()
{
    QTest::addColumn<QByteArray>("json");
    QTest::addColumn<QString>("expectedError");

    QTest::newRow("key") << QByteArray(R"({"type":"key","key":"KEY_A"})") << QString();
    QTest::newRow("combo") << QByteArray(R"({"type":"combo","keys":["KEY_LEFTCTRL","KEY_C"]})") << QString();
    QTest::newRow("mouse") << QByteArray(R"({"type":"mouse","button":"BTN_LEFT"})") << QString();
    QTest::newRow("macro") << QByteArray(R"({"type":"macro","macro":"x","repeat":"count","count":3})") << QString();
    QTest::newRow("mode") << QByteArray(R"({"type":"mode","mode":2})") << QString();
    QTest::newRow("profile") << QByteArray(R"({"type":"profile","profile":"next"})") << QString();
    QTest::newRow("lcd") << QByteArray(R"({"type":"lcd","page":"next"})") << QString();

    QTest::newRow("unknown type") << QByteArray(R"({"type":"teleport"})") << QStringLiteral("unknown action type");
    QTest::newRow("unknown key") << QByteArray(R"({"type":"key","key":"KEY_NOPE"})") << QStringLiteral("unknown key");
    QTest::newRow("joystick button as key")
        << QByteArray(R"({"type":"key","key":"BTN_THUMB"})") << QStringLiteral("not a keyboard key");
    QTest::newRow("macro key as key")
        << QByteArray(R"({"type":"key","key":"BTN_TRIGGER_HAPPY1"})") << QStringLiteral("not a keyboard key");
    QTest::newRow("combo of one") << QByteArray(R"({"type":"combo","keys":["KEY_A"]})") << QStringLiteral("at least two");
    QTest::newRow("combo duplicate")
        << QByteArray(R"({"type":"combo","keys":["KEY_A","KEY_A"]})") << QStringLiteral("twice");
    QTest::newRow("keyboard key as mouse")
        << QByteArray(R"({"type":"mouse","button":"KEY_A"})") << QStringLiteral("not a mouse button");
    QTest::newRow("mode 4") << QByteArray(R"({"type":"mode","mode":4})") << QStringLiteral("1, 2 or 3");
    QTest::newRow("macro repeat")
        << QByteArray(R"({"type":"macro","macro":"x","repeat":"forever"})") << QStringLiteral("unknown repeat");
}

void TestProfiles::actions()
{
    QFETCH(QByteArray, json);
    QFETCH(QString, expectedError);
    const auto action = Action::fromJson(object(json));
    if (expectedError.isEmpty()) {
        QVERIFY2(action.has_value(), action ? "" : qPrintable(action.error()));
    } else {
        QVERIFY(!action.has_value());
        QVERIFY2(action.error().contains(expectedError), qPrintable(action.error()));
    }
}

void TestProfiles::actionRoundTrip()
{
    const QList<QByteArray> samples = {
        R"({"type":"key","key":"KEY_A"})",
        R"({"type":"combo","keys":["KEY_LEFTCTRL","KEY_LEFTSHIFT","KEY_T"]})",
        R"({"type":"mouse","button":"BTN_RIGHT"})",
        R"({"type":"macro","macro":"hello","repeat":"while_held"})",
        R"({"type":"macro","macro":"hello","repeat":"count","count":4})",
        R"({"type":"mode","mode":3})",
        R"({"type":"profile","profile":"Gaming"})",
        R"({"type":"lcd","page":"clock"})",
    };
    for (const QByteArray &sample : samples) {
        const auto action = Action::fromJson(object(sample));
        QVERIFY2(action, sample.constData());
        QCOMPARE(action->toJson(), object(sample));
        QCOMPARE(*Action::fromJson(action->toJson()), *action);
    }
    QCOMPARE(Action::fromJson(object(R"({"type":"combo","keys":["KEY_LEFTCTRL","KEY_C"]})"))->codes(),
             (QList<int>{KEY_LEFTCTRL, KEY_C}));
}

void TestProfiles::profileParsing()
{
    const auto profile = Profile::fromJson(R"({
        "name": " Gaming ",
        "backlight": [10, 20, 30],
        "modes": {
            "M1": { "G1": { "type": "key", "key": "KEY_1" }, "G2": { "type": "key", "key": "KEY_NOPE" } },
            "M3": { "G22": { "type": "mouse", "button": "BTN_LEFT" } },
            "M7": {}
        },
        "futureOption": 42
    })");
    QVERIFY(profile);
    QCOMPARE(profile->name(), QStringLiteral("Gaming"));
    QCOMPARE(profile->backlight(), std::optional<Profile::Rgb>(Profile::Rgb{10, 20, 30}));
    QCOMPARE(profile->bindings(1).size(), 1);
    QCOMPARE(profile->bindings(1).value(QStringLiteral("G1")).codes(), QList<int>{KEY_1});
    QCOMPARE(profile->bindings(2).size(), 0);
    QCOMPARE(profile->bindings(3).value(QStringLiteral("G22")).type(), Action::Type::Mouse);
    QCOMPARE(profile->warnings().size(), 2);   // KEY_NOPE and M7

    QVERIFY(!Profile::fromJson(R"({"modes":{}})"));
    QVERIFY(!Profile::fromJson("[]"));
    QVERIFY(!Profile::fromJson("{"));

    const auto badBacklight = Profile::fromJson(R"({"name":"x","backlight":[1,2,300]})");
    QVERIFY(badBacklight);
    QVERIFY(!badBacklight->backlight());
    QCOMPARE(badBacklight->warnings().size(), 1);
}

void TestProfiles::profileRoundTrip()
{
    const auto profile = Profile::fromJson(R"({
        "name": "Round",
        "backlight": [1, 2, 3],
        "modes": { "M2": { "L1": { "type": "combo", "keys": ["KEY_LEFTALT", "KEY_TAB"] } } },
        "stick": { "mode": "keys" },
        "futureOption": { "a": 1 }
    })");
    QVERIFY(profile);
    const auto again = Profile::fromJson(profile->toJson());
    QVERIFY(again);
    QCOMPARE(again->name(), profile->name());
    QCOMPARE(again->backlight(), profile->backlight());
    QCOMPARE(again->bindings(2), profile->bindings(2));
    QCOMPARE(again->stick(), profile->stick());
    QCOMPARE(QJsonDocument::fromJson(again->toJson()).object().value(u"futureOption").toObject().value(u"a").toInt(), 1);
}

void TestProfiles::profileValidation()
{
    auto profile = Profile::fromJson(R"({
        "name": "Check",
        "modes": { "M1": {
            "G1": { "type": "key", "key": "KEY_A" },
            "M1": { "type": "key", "key": "KEY_B" },
            "LIGHT": { "type": "key", "key": "KEY_C" },
            "G99": { "type": "key", "key": "KEY_D" },
            "STICK_UP": { "type": "key", "key": "KEY_W" }
        } }
    })");
    QVERIFY(profile);
    profile->validateFor(*g13());
    QCOMPARE(profile->bindings(1).keys(), (QList<QString>{QStringLiteral("G1"), QStringLiteral("STICK_UP")}));
    QCOMPARE(profile->warnings().size(), 3);
}

void TestProfiles::storeLoadSaveAndReload()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ProfileStore store(*g13(), dir.filePath(QStringLiteral("g13")));
    QVERIFY(store.names().isEmpty());

    store.ensureDefault();
    QCOMPARE(store.names(), QStringList{QStringLiteral("Default")});
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral("g13/default.json"))));

    Profile gaming;
    gaming.setName(QStringLiteral("Gaming / WoW"));
    gaming.setBinding(1, QStringLiteral("G1"), Action::key(KEY_1));
    QString error;
    QVERIFY2(store.save(gaming, &error), qPrintable(error));
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral("g13/gaming-wow.json"))));
    QCOMPARE(store.names(), (QStringList{QStringLiteral("Default"), QStringLiteral("Gaming / WoW")}));

    // A change on disk by another program is picked up.
    QSignalSpy spy(&store, &ProfileStore::changed);
    writeFile(dir.filePath(QStringLiteral("g13/other.json")),
              R"({"name":"Other","modes":{"M1":{"G2":{"type":"key","key":"KEY_2"}}}})");
    QVERIFY(spy.wait(3000));
    QVERIFY(store.profile(QStringLiteral("Other")));
    QCOMPARE(store.profile(QStringLiteral("Other"))->bindings(1).value(QStringLiteral("G2")).codes(),
             QList<int>{KEY_2});

    // Saving an existing profile keeps its file.
    Profile changed = *store.profile(QStringLiteral("Other"));
    changed.setBinding(1, QStringLiteral("G3"), Action::key(KEY_3));
    QVERIFY(store.save(changed));
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("g13/other-2.json"))));
}

void TestProfiles::storeDuplicateNames()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("g13"));
    QDir().mkpath(path);
    writeFile(path + QStringLiteral("/a.json"), R"({"name":"Same"})");
    writeFile(path + QStringLiteral("/b.json"), R"({"name":"Same"})");
    writeFile(path + QStringLiteral("/broken.json"), "{");

    ProfileStore store(*g13(), path);
    QCOMPARE(store.names(), QStringList{QStringLiteral("Same")});
    QCOMPARE(store.messages().size(), 2);
}

void TestProfiles::storeRenameAndRemove()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ProfileStore store(*g13(), dir.filePath(QStringLiteral("g13")));
    Profile profile;
    profile.setName(QStringLiteral("Old"));
    profile.setBinding(2, QStringLiteral("G7"), Action::key(KEY_7));
    QVERIFY(store.save(profile));
    Profile other;
    other.setName(QStringLiteral("Other"));
    QVERIFY(store.save(other));

    QString error;
    QVERIFY(!store.rename(QStringLiteral("Old"), QStringLiteral("Other"), &error));   // name taken
    QVERIFY(error.contains(QStringLiteral("already exists")));
    QVERIFY(store.rename(QStringLiteral("Old"), QStringLiteral("New Name"), &error));
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("g13/old.json"))));
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral("g13/new-name.json"))));
    QCOMPARE(store.profile(QStringLiteral("New Name"))->bindings(2).value(QStringLiteral("G7")).codes(), QList<int>{KEY_7});

    QVERIFY(store.remove(QStringLiteral("Other"), &error));
    QCOMPARE(store.names(), QStringList{QStringLiteral("New Name")});
    QVERIFY(!store.remove(QStringLiteral("Other"), &error));

    // The changes survive a reload from disk.
    store.reload();
    QCOMPARE(store.names(), QStringList{QStringLiteral("New Name")});
}

void TestProfiles::state()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("sub/state.json"));
    {
        DaemonState state(path);
        state.load();
        QCOMPARE(state.mode(QStringLiteral("g13")), 1);
        QVERIFY(state.profile(QStringLiteral("g13")).isEmpty());
        state.setProfile(QStringLiteral("g13"), QStringLiteral("Gaming"));
        state.setMode(QStringLiteral("g13"), 3);
        QVERIFY(state.save());
    }
    DaemonState state(path);
    state.load();
    QCOMPARE(state.profile(QStringLiteral("g13")), QStringLiteral("Gaming"));
    QCOMPARE(state.mode(QStringLiteral("g13")), 3);
}

QTEST_GUILESS_MAIN(TestProfiles)
#include "tst_profiles.moc"

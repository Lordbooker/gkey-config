// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "Action.h"
#include "DaemonState.h"
#include "MacroRename.h"
#include "MacroStore.h"
#include "ModelRegistry.h"
#include "Profile.h"
#include "ProfileStore.h"

#include <QDir>
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
    void invalidBindingsAreKept();
    void storeLoadSaveAndReload();
    void storeDuplicateNames();
    void storeIgnoresHugeFiles();
    void storeRenameAndRemove();
    void macrosOfAProfile();
    void renameMacroEverywhere();
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
    QTest::newRow("macro key as key") << QByteArray(R"({"type":"key","key":"BTN_TRIGGER_HAPPY1"})")
                                      << QStringLiteral("not a keyboard key");
    QTest::newRow("combo of one") << QByteArray(R"({"type":"combo","keys":["KEY_A"]})")
                                  << QStringLiteral("at least two");
    QTest::newRow("combo duplicate") << QByteArray(R"({"type":"combo","keys":["KEY_A","KEY_A"]})")
                                     << QStringLiteral("twice");
    QTest::newRow("keyboard key as mouse")
        << QByteArray(R"({"type":"mouse","button":"KEY_A"})") << QStringLiteral("not a mouse button");
    QTest::newRow("mode 4") << QByteArray(R"({"type":"mode","mode":4})") << QStringLiteral("1, 2 or 3");
    QTest::newRow("macro repeat") << QByteArray(R"({"type":"macro","macro":"x","repeat":"forever"})")
                                  << QStringLiteral("unknown repeat");
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

    // Brightness levels (G15 family)
    const auto levels = Profile::fromJson(R"({"name":"x","backlightLevel":2,"lcdBacklightLevel":-1})");
    QVERIFY(levels);
    QCOMPARE(levels->backlightLevel(), std::optional<int>(2));
    QVERIFY(!levels->lcdBacklightLevel());
    QCOMPARE(levels->warnings().size(), 1);
}

void TestProfiles::profileRoundTrip()
{
    const auto profile = Profile::fromJson(R"({
        "name": "Round",
        "backlight": [1, 2, 3],
        "modes": { "M2": { "L1": { "type": "combo", "keys": ["KEY_LEFTALT", "KEY_TAB"] } } },
        "stick": { "mode": "keys" },
        "backlightLevel": 1,
        "lcdBacklightLevel": 0,
        "futureOption": { "a": 1 }
    })");
    QVERIFY(profile);
    const auto again = Profile::fromJson(profile->toJson());
    QVERIFY(again);
    QCOMPARE(again->name(), profile->name());
    QCOMPARE(again->backlight(), profile->backlight());
    QCOMPARE(again->bindings(2), profile->bindings(2));
    QCOMPARE(again->stick(), profile->stick());
    QCOMPARE(again->backlightLevel(), std::optional<int>(1));
    QCOMPARE(again->lcdBacklightLevel(), std::optional<int>(0));
    QCOMPARE(QJsonDocument::fromJson(again->toJson()).object().value(u"futureOption").toObject().value(u"a").toInt(),
             1);
}

// Bindings the daemon cannot use (unknown key name, key not bindable on the
// model, unknown mode) must survive a load/save cycle, e.g. from the GUI.
void TestProfiles::invalidBindingsAreKept()
{
    auto profile = Profile::fromJson(R"({
        "name": "Keep",
        "modes": {
            "M1": { "G1": { "type": "key", "key": "KEY_FROM_THE_FUTURE" },
                    "G2": { "type": "key", "key": "KEY_2" },
                    "BD": { "type": "key", "key": "KEY_3" } },
            "M4": { "G1": { "type": "key", "key": "KEY_4" } }
        }
    })");
    QVERIFY(profile);
    profile->validateFor(*g13());   // BD is not bindable on the G13
    QCOMPARE(profile->bindings(1).size(), 1);
    QCOMPARE(profile->warnings().size(), 3);   // G1, M4, BD

    const QJsonObject saved = QJsonDocument::fromJson(profile->toJson()).object();
    const QJsonObject m1 = saved.value(u"modes").toObject().value(u"M1").toObject();
    QCOMPARE(m1.value(u"G1").toObject().value(u"key").toString(), QStringLiteral("KEY_FROM_THE_FUTURE"));
    QCOMPARE(m1.value(u"G2").toObject().value(u"key").toString(), QStringLiteral("KEY_2"));
    QCOMPARE(m1.value(u"BD").toObject().value(u"key").toString(), QStringLiteral("KEY_3"));
    QVERIFY(saved.value(u"modes").toObject().contains(u"M4"));

    // An explicit change of the key replaces the inactive binding.
    profile->setBinding(1, QStringLiteral("G1"), Action::key(KEY_5));
    const QJsonObject changed = QJsonDocument::fromJson(profile->toJson()).object();
    QCOMPARE(
        changed.value(u"modes").toObject().value(u"M1").toObject().value(u"G1").toObject().value(u"key").toString(),
        QStringLiteral("KEY_5"));
    profile->removeBinding(1, QStringLiteral("BD"));
    QVERIFY(!QJsonDocument::fromJson(profile->toJson())
                 .object()
                 .value(u"modes")
                 .toObject()
                 .value(u"M1")
                 .toObject()
                 .contains(u"BD"));
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

void TestProfiles::storeIgnoresHugeFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("g13"));
    QDir().mkpath(path);
    writeFile(path + QStringLiteral("/a.json"), R"({"name":"Small"})");
    // Valid JSON, but far larger than any profile.
    const QByteArray padding(static_cast<qsizetype>(JsonFileStore<Profile>::MaxFileSize), ' ');
    writeFile(path + QStringLiteral("/b.json"), R"({"name":"Huge"})" + padding);

    ProfileStore store(*g13(), path);
    QCOMPARE(store.names(), QStringList{QStringLiteral("Small")});
    QCOMPARE(store.messages().size(), 1);
    QVERIFY2(store.messages().first().contains(QStringLiteral("larger than")), qPrintable(store.messages().first()));
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
    QCOMPARE(store.profile(QStringLiteral("New Name"))->bindings(2).value(QStringLiteral("G7")).codes(),
             QList<int>{KEY_7});

    QVERIFY(store.remove(QStringLiteral("Other"), &error));
    QCOMPARE(store.names(), QStringList{QStringLiteral("New Name")});
    QVERIFY(!store.remove(QStringLiteral("Other"), &error));

    // The changes survive a reload from disk.
    store.reload();
    QCOMPARE(store.names(), QStringList{QStringLiteral("New Name")});
}

void TestProfiles::macrosOfAProfile()
{
    // G1 and G2 (M1), G3 (M2) play macros; G99 does not exist on the G13 and is
    // kept inactive, its macro still counts.
    auto profile = Profile::fromJson(R"({"name":"P","modes":{
        "M1":{"G1":{"type":"macro","macro":"Beta"},"G2":{"type":"macro","macro":"alpha"},
              "G4":{"type":"key","key":"KEY_A"}},
        "M2":{"G3":{"type":"macro","macro":"Beta","repeat":"toggle"},"G99":{"type":"macro","macro":"Gamma"}}}})");
    QVERIFY(profile);
    profile->validateFor(*g13());
    QCOMPARE(profile->macros(),
             (QStringList{QStringLiteral("alpha"), QStringLiteral("Beta"), QStringLiteral("Gamma")}));

    QCOMPARE(profile->renameMacro(QStringLiteral("Beta"), QStringLiteral("Delta")), 2);
    QCOMPARE(profile->renameMacro(QStringLiteral("Gamma"), QStringLiteral("Epsilon")), 1);
    QCOMPARE(profile->renameMacro(QStringLiteral("unknown"), QStringLiteral("x")), 0);
    QCOMPARE(profile->macros(),
             (QStringList{QStringLiteral("alpha"), QStringLiteral("Delta"), QStringLiteral("Epsilon")}));
    // Everything else of the binding stays.
    QCOMPARE(profile->bindings(2).value(QStringLiteral("G3")).repeat(), Action::Repeat::Toggle);
    const QJsonObject written = object(profile->toJson());
    QCOMPARE(written[u"modes"][u"M2"][u"G99"][u"macro"].toString(), QStringLiteral("Epsilon"));
}

// Renaming a macro renames its bindings in the profiles of all models.
void TestProfiles::renameMacroEverywhere()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto profileDir = [&dir](const QString &modelId) {
        return dir.filePath(QStringLiteral("profiles/") + modelId);
    };
    QDir().mkpath(profileDir(QStringLiteral("g13")));
    QDir().mkpath(profileDir(QStringLiteral("g15")));
    QDir().mkpath(dir.filePath(QStringLiteral("macros")));
    writeFile(dir.filePath(QStringLiteral("macros/a.json")), R"({"name":"Old","steps":[{"tap":"KEY_A"}]})");
    writeFile(dir.filePath(QStringLiteral("macros/b.json")), R"({"name":"Taken","steps":[]})");
    writeFile(profileDir(QStringLiteral("g13")) + QStringLiteral("/a.json"),
              R"({"name":"Uses it","modes":{"M1":{"G1":{"type":"macro","macro":"Old"}},
                                           "M3":{"G2":{"type":"macro","macro":"Old"}}}})");
    const QByteArray untouched = R"({"name":"Other","modes":{"M1":{"G1":{"type":"macro","macro":"Taken"}}}})";
    writeFile(profileDir(QStringLiteral("g13")) + QStringLiteral("/b.json"), untouched);
    writeFile(profileDir(QStringLiteral("g15")) + QStringLiteral("/a.json"),
              R"({"name":"G15","modes":{"M2":{"G1":{"type":"macro","macro":"Old"}}}})");

    MacroStore macros(dir.filePath(QStringLiteral("macros")));
    // A name that is taken changes nothing.
    MacroRename::Result result =
        MacroRename::rename(macros, QStringLiteral("Old"), QStringLiteral("Taken"), m_registry.models(), profileDir);
    QVERIFY(!result.renamed);
    QCOMPARE(result.errors.size(), 1);
    QCOMPARE(result.bindings, 0);

    result = MacroRename::rename(macros, QStringLiteral("Old"), QStringLiteral("New"), m_registry.models(), profileDir);
    QVERIFY(result.renamed);
    QVERIFY2(result.errors.isEmpty(), qPrintable(result.errors.join(u'\n')));
    QCOMPARE(result.bindings, 3);
    QCOMPARE(result.profiles, 2);
    QVERIFY(!macros.macro(QStringLiteral("Old")));
    QCOMPARE(macros.macro(QStringLiteral("New"))->steps().size(), 1);

    ProfileStore g13Store(*g13(), profileDir(QStringLiteral("g13")));
    QCOMPARE(g13Store.profile(QStringLiteral("Uses it"))->macros(), QStringList{QStringLiteral("New")});
    QCOMPARE(g13Store.profile(QStringLiteral("Uses it"))->bindings(3).value(QStringLiteral("G2")).macro(),
             QStringLiteral("New"));
    // A profile without the macro is not written again.
    QFile file(profileDir(QStringLiteral("g13")) + QStringLiteral("/b.json"));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), untouched);
    ProfileStore g15Store(*m_registry.findById(QStringLiteral("g15")), profileDir(QStringLiteral("g15")));
    QCOMPARE(g15Store.profile(QStringLiteral("G15"))->macros(), QStringList{QStringLiteral("New")});
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

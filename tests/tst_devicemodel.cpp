// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "DeviceModel.h"
#include "EventCodes.h"
#include "ModelRegistry.h"

#include <QDir>
#include <QSet>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <linux/input-event-codes.h>

class TestDeviceModel : public QObject {
    Q_OBJECT

private slots:
    void eventCodes();
    void allShippedModelsAreValid();
    void g13Model();
    void g510Model();
    void g15Model();
    void invalidModels_data();
    void invalidModels();
    void registryOverride();
};

namespace {

QByteArray minimalModel(const QByteArray &keys = R"([{ "id": "G1", "code": "KEY_MACRO1" }])",
                        const QByteArray &extra = {})
{
    return R"({ "id": "test", "name": "Test", "usb": [{ "vendor": "046d", "product": "ffff" }],
               "inputs": { "keys": "Test Keys" }, "keys": )" + keys + extra + "}";
}

}

void TestDeviceModel::eventCodes()
{
    QCOMPARE(EventCodes::keyCode(QStringLiteral("KEY_MACRO1")), std::optional<int>(KEY_MACRO1));
    QCOMPARE(EventCodes::keyCode(QStringLiteral("BTN_THUMB")), std::optional<int>(BTN_THUMB));
    QCOMPARE(EventCodes::absCode(QStringLiteral("ABS_Y")), std::optional<int>(ABS_Y));
    QVERIFY(!EventCodes::keyCode(QStringLiteral("KEY_DOES_NOT_EXIST")));
    QCOMPARE(EventCodes::keyName(KEY_KBD_LCD_MENU5), QStringLiteral("KEY_KBD_LCD_MENU5"));
}

void TestDeviceModel::allShippedModelsAreValid()
{
    ModelRegistry registry;
    registry.load({QStringLiteral(GKEY_SOURCE_MODEL_DIR)});
    QVERIFY2(registry.errors().isEmpty(), qPrintable(registry.errors().join(u'\n')));
    QVERIFY(!registry.models().isEmpty());
}

void TestDeviceModel::g13Model()
{
    ModelRegistry registry;
    registry.load({QStringLiteral(GKEY_SOURCE_MODEL_DIR)});
    const DeviceModel *model = registry.findByUsb(0x046d, 0xc21c);
    QVERIFY(model);
    QCOMPARE(model->id(), QStringLiteral("g13"));
    QCOMPARE(model->inputs().value(QStringLiteral("keys")), QStringLiteral("Logitech G13 Gaming Keypad"));
    QCOMPARE(model->inputs().value(QStringLiteral("stick")), QStringLiteral("Logitech G13 Thumbstick"));

    // Values from drivers/hid/hid-lg-g15.c
    QCOMPARE(model->keyForCode(QStringLiteral("keys"), KEY_MACRO22)->id, QStringLiteral("G22"));
    QCOMPARE(model->keyForCode(QStringLiteral("keys"), KEY_KBD_LCD_MENU1)->id, QStringLiteral("L1"));
    QCOMPARE(model->keyForCode(QStringLiteral("keys"), KEY_KBD_LCD_MENU5)->id, QStringLiteral("BD"));
    QCOMPARE(model->keyForCode(QStringLiteral("stick"), BTN_THUMB)->id, QStringLiteral("STICK_PRESS"));
    QVERIFY(!model->keyForCode(QStringLiteral("keys"), BTN_THUMB));

    QCOMPARE(model->keyById(QStringLiteral("M1"))->role, DeviceModel::KeyRole::Mode1);
    QCOMPARE(model->keyById(QStringLiteral("MR"))->role, DeviceModel::KeyRole::Record);
    QCOMPARE(model->keyById(QStringLiteral("LIGHT"))->role, DeviceModel::KeyRole::Hardware);
    QCOMPARE(model->keyById(QStringLiteral("BD"))->role, DeviceModel::KeyRole::LcdNext);

    // 22 G keys + L1-L4 + 3 stick buttons + 4 stick directions (BD is fixed)
    QCOMPARE(model->bindableKeyIds().size(), 33);
    QVERIFY(!model->bindableKeyIds().contains(QStringLiteral("BD")));
    QVERIFY(!model->bindableKeyIds().contains(QStringLiteral("M1")));
    QVERIFY(!model->bindableKeyIds().contains(QStringLiteral("LIGHT")));
    QVERIFY(model->bindableKeyIds().contains(QStringLiteral("STICK_UP")));

    QCOMPARE(model->ledByRole(QStringLiteral("backlight"))->type, DeviceModel::LedType::Rgb);
    QCOMPARE(model->ledByRole(QStringLiteral("mode2"))->suffix, QStringLiteral("macro_preset_2"));
    QVERIFY(model->stick());
    QCOMPARE(model->stick()->axisX, ABS_X);
    QVERIFY(model->lcd());
    QCOMPARE(model->lcd()->width, 160);
    QCOMPARE(model->lcd()->height, 43);

    // Every bindable key and every fixed key is drawn exactly once.
    QSet<QString> drawn;
    for (const auto &area : model->view().areas)
        drawn.insert(area.key);
    for (const QString &id : model->bindableKeyIds())
        QVERIFY2(drawn.contains(id), qPrintable(id));
    for (const auto &key : model->keys())
        QVERIFY2(drawn.contains(key.id), qPrintable(key.id));
    QCOMPARE(drawn.size(), model->view().areas.size());
}

void TestDeviceModel::g510Model()
{
    ModelRegistry registry;
    registry.load({QStringLiteral(GKEY_SOURCE_MODEL_DIR)});
    const DeviceModel *model = registry.findByUsb(0x046d, 0xc22e);   // with headset
    QVERIFY(model);
    QCOMPARE(model->id(), QStringLiteral("g510"));
    QVERIFY(registry.findByUsb(0x046d, 0xc22d) == model);
    QVERIFY(!model->tested());
    QCOMPARE(model->minKernel(), QStringLiteral("5.5"));
    QCOMPARE(model->inputs().value(QStringLiteral("keys")), QStringLiteral("Logitech Gaming Keyboard Gaming Keys"));

    QCOMPARE(model->keyForCode(QStringLiteral("keys"), KEY_KBD_LCD_MENU1)->id, QStringLiteral("BD"));
    QCOMPARE(model->keyById(QStringLiteral("BD"))->role, DeviceModel::KeyRole::LcdNext);
    QCOMPARE(model->keyForCode(QStringLiteral("keys"), KEY_KBD_LCD_MENU2)->id, QStringLiteral("L1"));
    QCOMPARE(model->keyById(QStringLiteral("MUTE_HEADPHONE"))->role, DeviceModel::KeyRole::Passthrough);

    // 18 G keys + L1-L4; no stick
    QCOMPARE(model->bindableKeyIds().size(), 22);
    QVERIFY(!model->bindableKeyIds().contains(QStringLiteral("MUTE_MIC")));
    QVERIFY(!model->stick());
    QCOMPARE(model->ledByRole(QStringLiteral("backlight"))->type, DeviceModel::LedType::Rgb);
    QCOMPARE(model->ledByRole(QStringLiteral("mode1"))->suffix, QStringLiteral("macro_preset1"));
    QVERIFY(model->lcd());
    QCOMPARE(model->lcd()->type, QStringLiteral("g15-hidraw"));
}

void TestDeviceModel::g15Model()
{
    ModelRegistry registry;
    registry.load({QStringLiteral(GKEY_SOURCE_MODEL_DIR)});
    const DeviceModel *model = registry.findByUsb(0x046d, 0xc222);
    QVERIFY(model);
    QCOMPARE(model->id(), QStringLiteral("g15"));
    QVERIFY(!model->tested());
    QCOMPARE(model->inputs().value(QStringLiteral("keys")), QStringLiteral("Logitech Gaming Keyboard Gaming Keys"));
    QCOMPARE(model->keyById(QStringLiteral("BD"))->role, DeviceModel::KeyRole::LcdNext);
    QCOMPARE(model->keyForCode(QStringLiteral("keys"), KEY_KBD_LCD_MENU5)->id, QStringLiteral("L4"));
    QCOMPARE(model->bindableKeyIds().size(), 22);   // 18 G keys + L1-L4

    // Backlight of keys and LCD in three levels (0..2, hid-lg-g15)
    const DeviceModel::Led *keys = model->ledByRole(QStringLiteral("backlight"));
    QCOMPARE(keys->type, DeviceModel::LedType::Level);
    QCOMPARE(keys->max, 2);
    QCOMPARE(model->ledByRole(QStringLiteral("lcd-backlight"))->suffix, QStringLiteral("lcd_backlight"));
    QCOMPARE(model->lcd()->type, QStringLiteral("g15-hidraw"));
}

void TestDeviceModel::invalidModels_data()
{
    QTest::addColumn<QByteArray>("json");
    QTest::addColumn<QString>("expectedError");

    QTest::newRow("valid") << minimalModel() << QString();
    QTest::newRow("broken json") << QByteArray("{") << QStringLiteral("invalid JSON");
    QTest::newRow("unknown code")
        << minimalModel(R"([{ "id": "G1", "code": "KEY_NOPE" }])") << QStringLiteral("unknown event code");
    QTest::newRow("duplicate id")
        << minimalModel(R"([{ "id": "G1", "code": "KEY_MACRO1" }, { "id": "G1", "code": "KEY_MACRO2" }])")
        << QStringLiteral("duplicate key id");
    QTest::newRow("duplicate code")
        << minimalModel(R"([{ "id": "G1", "code": "KEY_MACRO1" }, { "id": "G2", "code": "KEY_MACRO1" }])")
        << QStringLiteral("already mapped");
    QTest::newRow("unknown input")
        << minimalModel(R"([{ "id": "G1", "code": "KEY_MACRO1", "input": "stick" }])")
        << QStringLiteral("unknown input");
    QTest::newRow("reserved id")
        << minimalModel(R"([{ "id": "STICK_UP", "code": "KEY_MACRO1" }])") << QStringLiteral("reserved");
    QTest::newRow("unknown role")
        << minimalModel(R"([{ "id": "G1", "code": "KEY_MACRO1", "role": "turbo" }])")
        << QStringLiteral("unknown role");
    QTest::newRow("unknown led role")
        << minimalModel(R"([{ "id": "G1", "code": "KEY_MACRO1" }])",
                        R"(, "leds": { "disco": { "suffix": "x" } })")
        << QStringLiteral("unknown LED role");
    QTest::newRow("view area for unknown key")
        << minimalModel(R"([{ "id": "G1", "code": "KEY_MACRO1" }])",
                        R"(, "view": { "width": 10, "height": 10,
                             "keys": [{ "key": "G9", "x": 1, "y": 1, "w": 5, "h": 5 }] })")
        << QStringLiteral("unknown key");
    QTest::newRow("view key drawn twice")
        << minimalModel(R"([{ "id": "G1", "code": "KEY_MACRO1" }])",
                        R"(, "view": { "width": 10, "height": 10, "keys": [
                             { "key": "G1", "x": 1, "y": 1, "w": 5, "h": 5 },
                             { "key": "G1", "x": 2, "y": 2, "w": 5, "h": 5 }] })")
        << QStringLiteral("drawn twice");
    QTest::newRow("view bad shape")
        << minimalModel(R"([{ "id": "G1", "code": "KEY_MACRO1" }])",
                        R"(, "view": { "width": 10, "height": 10,
                             "keys": [{ "key": "G1", "shape": "star", "x": 1, "y": 1, "w": 5, "h": 5 }] })")
        << QStringLiteral("invalid shape");
    QTest::newRow("view without size")
        << minimalModel(R"([{ "id": "G1", "code": "KEY_MACRO1" }])", R"(, "view": { "keys": [] })")
        << QStringLiteral("width");
    QTest::newRow("bad usb id")
        << QByteArray(R"({ "id": "test", "name": "Test", "usb": [{ "vendor": "46d", "product": "ffff" }],
                         "inputs": { "keys": "x" }, "keys": [{ "id": "G1", "code": "KEY_MACRO1" }] })")
        << QStringLiteral("4-digit hex");
}

void TestDeviceModel::invalidModels()
{
    QFETCH(QByteArray, json);
    QFETCH(QString, expectedError);

    const auto model = DeviceModel::fromJson(json, QStringLiteral("test.json"));
    if (expectedError.isEmpty()) {
        QVERIFY2(model.has_value(), model ? "" : qPrintable(model.error()));
    } else {
        QVERIFY(!model.has_value());
        QVERIFY2(model.error().contains(expectedError), qPrintable(model.error()));
        QVERIFY(model.error().startsWith(QStringLiteral("test.json: ")));
    }
}

void TestDeviceModel::registryOverride()
{
    // A model in an earlier search path overrides the shipped one with the same id.
    QTemporaryDir userDir;
    QVERIFY(userDir.isValid());
    QFile file(userDir.filePath(QStringLiteral("my-g13.json")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({ "id": "g13", "name": "Overridden G13", "usb": [{ "vendor": "046d", "product": "c21c" }],
                   "inputs": { "keys": "Logitech G13 Gaming Keypad" },
                   "keys": [{ "id": "G1", "code": "KEY_MACRO1" }] })");
    file.close();

    ModelRegistry registry;
    registry.load({userDir.path(), QStringLiteral(GKEY_SOURCE_MODEL_DIR)});
    QVERIFY(registry.errors().isEmpty());
    QCOMPARE(registry.findById(QStringLiteral("g13"))->name(), QStringLiteral("Overridden G13"));
}

QTEST_GUILESS_MAIN(TestDeviceModel)
#include "tst_devicemodel.moc"

#include "LedController.h"
#include "ModelRegistry.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

// LedController against a fake sysfs tree shaped like the one of the G13
// (checked on hardware, see docs/CONCEPT.md 1.3a).
class TestLeds : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void findsLeds();
    void modeAndRecording();
    void backlight();
    void noBacklightInProfile();
    void permissionDenied();

private:
    QString read(const QString &led, const QString &attribute) const;
    void createLed(const QString &name, bool multicolor);

    ModelRegistry m_registry;
    std::unique_ptr<QTemporaryDir> m_dir;
    QString m_hid;
};

void TestLeds::initTestCase()
{
    m_registry.load({QStringLiteral(GKEY_SOURCE_MODEL_DIR)});
    QVERIFY(m_registry.findById(QStringLiteral("g13")));
}

void TestLeds::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_hid = m_dir->filePath(QStringLiteral("0003:046D:C21C.0024"));
    for (int i = 1; i <= 3; ++i)
        createLed(QStringLiteral("g13:red:macro_preset_%1").arg(i), false);
    createLed(QStringLiteral("g13:red:macro_record"), false);
    createLed(QStringLiteral("g13:rgb:kbd_backlight"), true);
}

void TestLeds::createLed(const QString &name, bool multicolor)
{
    const QString dir = m_hid + QStringLiteral("/leds/") + name;
    QVERIFY(QDir().mkpath(dir));
    auto write = [&](const QString &attribute, const QByteArray &value) {
        QFile file(dir + u'/' + attribute);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(value);
    };
    write(QStringLiteral("brightness"), "0\n");
    write(QStringLiteral("max_brightness"), multicolor ? "255\n" : "1\n");
    if (multicolor)
        write(QStringLiteral("multi_intensity"), "0 255 0\n");
}

QString TestLeds::read(const QString &led, const QString &attribute) const
{
    QFile file(m_hid + QStringLiteral("/leds/") + led + u'/' + attribute);
    if (!file.open(QIODevice::ReadOnly))
        return QStringLiteral("<missing>");
    return QString::fromUtf8(file.readAll()).trimmed();
}

void TestLeds::findsLeds()
{
    LedController leds(*m_registry.findById(QStringLiteral("g13")), m_hid);
    QList<QString> roles = leds.roles();
    std::sort(roles.begin(), roles.end());
    QCOMPARE(roles, (QList<QString>{QStringLiteral("backlight"), QStringLiteral("mode1"), QStringLiteral("mode2"),
                                    QStringLiteral("mode3"), QStringLiteral("record")}));
}

void TestLeds::modeAndRecording()
{
    LedController leds(*m_registry.findById(QStringLiteral("g13")), m_hid);
    leds.setMode(2);
    QCOMPARE(read(QStringLiteral("g13:red:macro_preset_1"), QStringLiteral("brightness")), QStringLiteral("0"));
    QCOMPARE(read(QStringLiteral("g13:red:macro_preset_2"), QStringLiteral("brightness")), QStringLiteral("1"));
    QCOMPARE(read(QStringLiteral("g13:red:macro_preset_3"), QStringLiteral("brightness")), QStringLiteral("0"));
    leds.setRecording(true);
    QCOMPARE(read(QStringLiteral("g13:red:macro_record"), QStringLiteral("brightness")), QStringLiteral("1"));
    leds.setRecording(false);
    QCOMPARE(read(QStringLiteral("g13:red:macro_record"), QStringLiteral("brightness")), QStringLiteral("0"));
    QVERIFY(!leds.permissionDenied());
}

void TestLeds::backlight()
{
    LedController leds(*m_registry.findById(QStringLiteral("g13")), m_hid);
    leds.setBacklight(Profile::Rgb{51, 153, 255});
    QCOMPARE(read(QStringLiteral("g13:rgb:kbd_backlight"), QStringLiteral("multi_intensity")),
             QStringLiteral("51 153 255"));
    QCOMPARE(read(QStringLiteral("g13:rgb:kbd_backlight"), QStringLiteral("brightness")), QStringLiteral("255"));
}

void TestLeds::noBacklightInProfile()
{
    LedController leds(*m_registry.findById(QStringLiteral("g13")), m_hid);
    leds.setBacklight(std::nullopt);
    QCOMPARE(read(QStringLiteral("g13:rgb:kbd_backlight"), QStringLiteral("multi_intensity")),
             QStringLiteral("0 255 0"));
}

void TestLeds::permissionDenied()
{
    if (::geteuid() == 0)
        QSKIP("file permissions do not apply to root");
    QFile::setPermissions(m_hid + QStringLiteral("/leds/g13:red:macro_preset_1/brightness"), QFile::ReadOwner);
    LedController leds(*m_registry.findById(QStringLiteral("g13")), m_hid);
    leds.setMode(1);
    QVERIFY(leds.permissionDenied());
}

QTEST_GUILESS_MAIN(TestLeds)
#include "tst_leds.moc"

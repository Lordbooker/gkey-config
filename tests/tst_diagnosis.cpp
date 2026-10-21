// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

// Why a plugged-in device is not usable, checked on a fake sysfs tree.

#include "DeviceDiagnosis.h"
#include "ModelRegistry.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using DeviceDiagnosis::Problem;

class TestDiagnosis : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void versions();
    void notPlugged();
    void ok();
    void kernelTooOld();
    void usedByOtherProgram();
    void genericDriver();
    void noHidDevice();

private:
    void write(const QString &path, const QByteArray &content);
    void link(const QString &target, const QString &path);
    void plugUsb(const QString &interfaceDriver);
    void addHid(const QString &driver, const QString &inputName);
    DeviceDiagnosis::Result run(const QString &release = QStringLiteral("7.2.11-arch1-1"));

    ModelRegistry m_registry;
    const DeviceModel *m_g13 = nullptr;
    std::unique_ptr<QTemporaryDir> m_dir;
    QString m_root;
};

void TestDiagnosis::initTestCase()
{
    m_registry.load({QStringLiteral(GKEY_SOURCE_MODEL_DIR)});
    m_g13 = m_registry.findById(QStringLiteral("g13"));
    QVERIFY(m_g13);
    QCOMPARE(m_g13->minKernel(), QStringLiteral("6.19"));
}

void TestDiagnosis::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_root = m_dir->path();
    QVERIFY(QDir().mkpath(m_root + QStringLiteral("/bus/usb/devices")));
    QVERIFY(QDir().mkpath(m_root + QStringLiteral("/bus/hid/devices")));
    // Another USB device without a match.
    write(m_root + QStringLiteral("/bus/usb/devices/1-1/idVendor"), "046d\n");
    write(m_root + QStringLiteral("/bus/usb/devices/1-1/idProduct"), "c52b\n");
}

void TestDiagnosis::write(const QString &path, const QByteArray &content)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

void TestDiagnosis::link(const QString &target, const QString &path)
{
    QVERIFY(QDir().mkpath(target));
    QVERIFY(QFile::link(target, path));
}

void TestDiagnosis::plugUsb(const QString &interfaceDriver)
{
    const QString dev = m_root + QStringLiteral("/bus/usb/devices/3-2");
    write(dev + QStringLiteral("/idVendor"), "046d\n");
    write(dev + QStringLiteral("/idProduct"), "c21c\n");
    QVERIFY(QDir().mkpath(dev + QStringLiteral("/3-2:1.0")));
    link(m_root + QStringLiteral("/bus/usb/drivers/") + interfaceDriver, dev + QStringLiteral("/3-2:1.0/driver"));
}

void TestDiagnosis::addHid(const QString &driver, const QString &inputName)
{
    const QString hid = m_root + QStringLiteral("/bus/hid/devices/0003:046D:C21C.000A");
    QVERIFY(QDir().mkpath(hid));
    if (!driver.isEmpty())
        link(m_root + QStringLiteral("/bus/hid/drivers/") + driver, hid + QStringLiteral("/driver"));
    write(hid + QStringLiteral("/input/input42/name"), inputName.toUtf8() + '\n');
}

DeviceDiagnosis::Result TestDiagnosis::run(const QString &release)
{
    return DeviceDiagnosis::diagnose(*m_g13, m_root, release);
}

void TestDiagnosis::versions()
{
    QVERIFY(DeviceDiagnosis::versionAtLeast(QStringLiteral("7.2.11-arch1-1"), QStringLiteral("6.19")));
    QVERIFY(DeviceDiagnosis::versionAtLeast(QStringLiteral("6.19.0-1-generic"), QStringLiteral("6.19")));
    QVERIFY(DeviceDiagnosis::versionAtLeast(QStringLiteral("6.20.1"), QStringLiteral("6.19")));
    QVERIFY(!DeviceDiagnosis::versionAtLeast(QStringLiteral("6.18.9"), QStringLiteral("6.19")));
    QVERIFY(!DeviceDiagnosis::versionAtLeast(QStringLiteral("6.12.48+deb13-amd64"), QStringLiteral("6.19")));
    QVERIFY(!DeviceDiagnosis::kernelRelease().isEmpty());
}

void TestDiagnosis::notPlugged()
{
    const auto result = run();
    QVERIFY(!result.plugged);
    QCOMPARE(result.problem, Problem::None);
}

void TestDiagnosis::ok()
{
    plugUsb(QStringLiteral("usbhid"));
    addHid(QStringLiteral("lg-g15"), QStringLiteral("Logitech G13 Gaming Keypad"));
    const auto result = run();
    QVERIFY(result.plugged);
    QCOMPARE(result.problem, Problem::None);
}

void TestDiagnosis::kernelTooOld()
{
    plugUsb(QStringLiteral("usbhid"));
    addHid(QStringLiteral("hid-generic"), QStringLiteral("Logitech G13 Joystick"));
    const auto result = run(QStringLiteral("6.12.48+deb13-amd64"));
    QCOMPARE(result.problem, Problem::KernelTooOld);
    QCOMPARE(result.detail, QStringLiteral("6.12.48+deb13-amd64"));
}

void TestDiagnosis::usedByOtherProgram()
{
    plugUsb(QStringLiteral("usbfs"));
    QCOMPARE(run().problem, Problem::UsedByOtherProgram);
}

void TestDiagnosis::genericDriver()
{
    plugUsb(QStringLiteral("usbhid"));
    addHid(QStringLiteral("hid-generic"), QStringLiteral("Logitech G13 Joystick"));
    const auto result = run();
    QCOMPARE(result.problem, Problem::NoKernelDriver);
    QCOMPARE(result.detail, QStringLiteral("hid-generic"));
}

void TestDiagnosis::noHidDevice()
{
    plugUsb(QStringLiteral("usbhid"));
    const auto result = run();
    QCOMPARE(result.problem, Problem::NoKernelDriver);
    QVERIFY(result.detail.isEmpty());
}

QTEST_GUILESS_MAIN(TestDiagnosis)
#include "tst_diagnosis.moc"

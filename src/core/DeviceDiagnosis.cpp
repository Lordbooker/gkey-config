// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "DeviceDiagnosis.h"

#include "DeviceModel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <sys/utsname.h>

namespace {

QString readLine(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readLine()).trimmed();
}

QString driverName(const QString &deviceDir)
{
    const QFileInfo link(deviceDir + QStringLiteral("/driver"));
    return link.exists() ? QFileInfo(link.symLinkTarget()).fileName() : QString();
}

QList<int> versionNumbers(const QString &version)
{
    static const QRegularExpression leading(QStringLiteral("^(\\d+)(?:\\.(\\d+))?(?:\\.(\\d+))?"));
    const QRegularExpressionMatch match = leading.match(version);
    QList<int> numbers;
    for (int i = 1; i <= 3; ++i)
        numbers.append(match.captured(i).isEmpty() ? 0 : match.captured(i).toInt());
    return numbers;
}

}

namespace DeviceDiagnosis {

QString kernelRelease()
{
    utsname name{};
    return uname(&name) == 0 ? QString::fromLocal8Bit(name.release) : QString();
}

bool versionAtLeast(const QString &release, const QString &minimum)
{
    return versionNumbers(release) >= versionNumbers(minimum);
}

Result diagnose(const DeviceModel &model, const QString &sysfsRoot, const QString &release)
{
    Result result;

    // 1. Plugged in? USB devices carry idVendor/idProduct; interfaces are subdirectories "<dev>:<cfg>.<if>".
    QString usbDevice;
    const QDir usbDir(sysfsRoot + QStringLiteral("/bus/usb/devices"));
    for (const QString &entry : usbDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::System)) {
        const QString path = usbDir.filePath(entry);
        bool vendorOk = false, productOk = false;
        const quint16 vendor = readLine(path + QStringLiteral("/idVendor")).toUShort(&vendorOk, 16);
        const quint16 product = readLine(path + QStringLiteral("/idProduct")).toUShort(&productOk, 16);
        if (vendorOk && productOk && model.matchesUsb(vendor, product)) {
            usbDevice = path;
            break;
        }
    }
    if (usbDevice.isEmpty())
        return result;
    result.plugged = true;

    // 2. Kernel version.
    const QString running = release.isEmpty() ? kernelRelease() : release;
    if (!model.minKernel().isEmpty() && !running.isEmpty() && !versionAtLeast(running, model.minKernel())) {
        result.problem = Problem::KernelTooOld;
        result.detail = running;
        return result;
    }

    // 3. USB interfaces claimed through usbfs belong to a program using libusb.
    const QDir device(usbDevice);
    const QString base = QFileInfo(usbDevice).fileName();
    for (const QString &entry : device.entryList({base + QStringLiteral(":*")}, QDir::Dirs | QDir::System)) {
        if (driverName(device.filePath(entry)) == u"usbfs") {
            result.problem = Problem::UsedByOtherProgram;
            return result;
        }
    }

    // 4. HID device ("0003:046D:C21C.000A") driven by the kernel driver: it
    // provides the input devices named in the model file.
    QStringList hidDevices;
    const QDir hidDir(sysfsRoot + QStringLiteral("/bus/hid/devices"));
    for (const auto &id : model.usbIds()) {
        const QString prefix = QStringLiteral("*:%1:%2.*")
                                   .arg(id.vendor, 4, 16, QLatin1Char('0'))
                                   .arg(id.product, 4, 16, QLatin1Char('0'))
                                   .toUpper();
        for (const QString &entry : hidDir.entryList({prefix}, QDir::Dirs | QDir::System))
            hidDevices.append(hidDir.filePath(entry));
    }
    QString driver;
    for (const QString &hid : hidDevices) {
        driver = driverName(hid);
        const QDir inputs(hid + QStringLiteral("/input"));
        for (const QString &input : inputs.entryList({QStringLiteral("input*")}, QDir::Dirs | QDir::System)) {
            const QString name = readLine(inputs.filePath(input) + QStringLiteral("/name"));
            for (const QString &expected : model.inputs()) {
                if (name == expected)
                    return result; // kernel side is fine
            }
        }
    }
    result.problem = Problem::NoKernelDriver;
    result.detail = driver;
    return result;
}

}

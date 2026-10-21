// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <QString>

class DeviceModel;

// Explains why a device that is plugged in does not show up as usable: the
// kernel side is checked through sysfs (USB device, HID device, its driver
// and input devices).
namespace DeviceDiagnosis {

enum class Problem {
    None,                // plugged in and provided by the kernel driver (or not plugged in)
    KernelTooOld,        // kernel older than the model's minKernel
    UsedByOtherProgram,  // a program claimed the USB interface directly (e.g. linux-g13-driver)
    NoKernelDriver,      // the HID device is missing or not driven by hid-lg-g15
};

struct Result {
    bool plugged = false;
    Problem problem = Problem::None;
    QString detail;   // kernel release or driver name, for messages
};

Result diagnose(const DeviceModel &model, const QString &sysfsRoot = QStringLiteral("/sys"),
                const QString &kernelRelease = QString());

// Release of the running kernel (uname -r).
QString kernelRelease();

// "7.2.11-arch1-1" >= "6.19"; compares the numeric parts.
bool versionAtLeast(const QString &release, const QString &minimum);

}

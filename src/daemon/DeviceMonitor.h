// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <QObject>
#include <QString>

struct udev;
struct udev_device;
struct udev_monitor;
class QSocketNotifier;

// Finds evdev nodes that belong to HID devices and reports them as they come
// and go (libudev enumeration + monitor).
class DeviceMonitor : public QObject {
    Q_OBJECT
public:
    struct InputInfo {
        QString devNode;      // /dev/input/eventN
        QString inputName;    // kernel input device name
        QString hidSyspath;   // sysfs path of the HID device (identifies the physical device)
        QString hidDriver;    // driver bound to the HID device, e.g. "lg-g15"
        quint16 vendor = 0;
        quint16 product = 0;
    };

    explicit DeviceMonitor(QObject *parent = nullptr);
    ~DeviceMonitor() override;

    // Starts monitoring and reports all evdev nodes that exist already.
    bool start(QString *error);

signals:
    void inputAdded(const DeviceMonitor::InputInfo &info);
    void inputRemoved(const QString &devNode);

private:
    void onMonitorEvent();
    void report(udev_device *device);

    udev *m_udev = nullptr;
    udev_monitor *m_monitor = nullptr;
    QSocketNotifier *m_notifier = nullptr;
};

// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "DeviceMonitor.h"

#include <QSocketNotifier>

#include <libudev.h>

namespace {

QString fromUtf8(const char *text)
{
    return text ? QString::fromUtf8(text) : QString();
}

// HID_ID has the form "0003:0000046D:0000C21C" (bus:vendor:product).
bool parseHidId(const QString &hidId, quint16 *vendor, quint16 *product)
{
    const QStringList parts = hidId.split(u':');
    if (parts.size() != 3)
        return false;
    bool okVendor = false;
    bool okProduct = false;
    *vendor = static_cast<quint16>(parts.at(1).toUInt(&okVendor, 16));
    *product = static_cast<quint16>(parts.at(2).toUInt(&okProduct, 16));
    return okVendor && okProduct;
}

}

DeviceMonitor::DeviceMonitor(QObject *parent) : QObject(parent) {}

DeviceMonitor::~DeviceMonitor()
{
    if (m_monitor)
        udev_monitor_unref(m_monitor);
    if (m_udev)
        udev_unref(m_udev);
}

bool DeviceMonitor::start(QString *error)
{
    m_udev = udev_new();
    if (!m_udev) {
        *error = QStringLiteral("udev_new() failed");
        return false;
    }

    m_monitor = udev_monitor_new_from_netlink(m_udev, "udev");
    if (!m_monitor || udev_monitor_filter_add_match_subsystem_devtype(m_monitor, "input", nullptr) < 0 ||
        udev_monitor_enable_receiving(m_monitor) < 0) {
        *error = QStringLiteral("cannot monitor udev events");
        return false;
    }
    m_notifier = new QSocketNotifier(udev_monitor_get_fd(m_monitor), QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &DeviceMonitor::onMonitorEvent);

    // Devices that are already connected.
    udev_enumerate *enumerate = udev_enumerate_new(m_udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_add_match_sysname(enumerate, "event*");
    udev_enumerate_scan_devices(enumerate);
    udev_list_entry *entry = nullptr;
    udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate))
    {
        udev_device *device = udev_device_new_from_syspath(m_udev, udev_list_entry_get_name(entry));
        if (device) {
            report(device);
            udev_device_unref(device);
        }
    }
    udev_enumerate_unref(enumerate);
    return true;
}

void DeviceMonitor::onMonitorEvent()
{
    udev_device *device = udev_monitor_receive_device(m_monitor);
    if (!device)
        return;
    const QString action = fromUtf8(udev_device_get_action(device));
    const QString sysname = fromUtf8(udev_device_get_sysname(device));
    if (sysname.startsWith(u"event")) {
        if (action == u"add")
            report(device);
        else if (action == u"remove")
            emit inputRemoved(fromUtf8(udev_device_get_devnode(device)));
    }
    udev_device_unref(device);
}

void DeviceMonitor::report(udev_device *device)
{
    const char *devNode = udev_device_get_devnode(device);
    if (!devNode)
        return;
    // Parents are owned by the child device; they must not be unref'd.
    udev_device *input = udev_device_get_parent_with_subsystem_devtype(device, "input", nullptr);
    udev_device *hid = udev_device_get_parent_with_subsystem_devtype(device, "hid", nullptr);
    if (!input || !hid)
        return;   // not a HID device (also excludes our own virtual device)

    InputInfo info;
    info.devNode = QString::fromUtf8(devNode);
    info.inputName = fromUtf8(udev_device_get_sysattr_value(input, "name"));
    info.hidSyspath = fromUtf8(udev_device_get_syspath(hid));
    info.hidDriver = fromUtf8(udev_device_get_driver(hid));
    if (!parseHidId(fromUtf8(udev_device_get_property_value(hid, "HID_ID")), &info.vendor, &info.product))
        return;
    emit inputAdded(info);
}

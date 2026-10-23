// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "VirtualInput.h"

#include "OutputCodes.h"

#include <libevdev/libevdev-uinput.h>
#include <libevdev/libevdev.h>

#include <QLoggingCategory>

#include <cstring>

Q_LOGGING_CATEGORY(lcOutput, "gkey.output")

VirtualInput::~VirtualInput()
{
    if (m_uinput)
        libevdev_uinput_destroy(m_uinput);
    if (m_device)
        libevdev_free(m_device);
}

bool VirtualInput::create(QString *error)
{
    m_permissionDenied = false;
    if (m_device)
        libevdev_free(m_device);   // from an earlier, failed attempt
    m_device = libevdev_new();
    libevdev_set_name(m_device, DeviceName);
    libevdev_set_id_bustype(m_device, BUS_VIRTUAL);
    libevdev_set_id_vendor(m_device, 0);
    libevdev_set_id_product(m_device, 0);
    libevdev_set_id_version(m_device, 1);

    libevdev_enable_event_type(m_device, EV_KEY);
    for (int code : OutputCodes::allKeys())
        libevdev_enable_event_code(m_device, EV_KEY, static_cast<unsigned int>(code), nullptr);

    // Relative axes for the stick's pointer and scroll modes.
    libevdev_enable_event_type(m_device, EV_REL);
    for (const int code : {REL_X, REL_Y, REL_WHEEL, REL_HWHEEL})
        libevdev_enable_event_code(m_device, EV_REL, static_cast<unsigned int>(code), nullptr);

    const int rc = libevdev_uinput_create_from_device(m_device, LIBEVDEV_UINPUT_OPEN_MANAGED, &m_uinput);
    if (rc < 0) {
        m_uinput = nullptr;
        m_permissionDenied = rc == -EACCES || rc == -EPERM;
        if (error) {
            *error = QStringLiteral("cannot create the virtual input device (/dev/uinput): %1")
                         .arg(QString::fromLocal8Bit(std::strerror(-rc)));
            if (rc == -EACCES)
                *error += QStringLiteral(" - is the udev rule 70-gkey-config.rules installed?");
        }
        return false;
    }
    return true;
}

void VirtualInput::writeKey(int code, bool pressed)
{
    write(EV_KEY, code, pressed ? 1 : 0);
}

void VirtualInput::writeRel(int code, int value)
{
    write(EV_REL, code, value);
}

void VirtualInput::sync()
{
    write(EV_SYN, SYN_REPORT, 0);
}

void VirtualInput::write(unsigned int type, int code, int value)
{
    if (!m_uinput)
        return;
    const int rc = libevdev_uinput_write_event(m_uinput, type, static_cast<unsigned int>(code), value);
    // Logged once per kind of error, so a broken device cannot flood the journal.
    if (rc < 0 && rc != m_lastError) {
        qCWarning(lcOutput).noquote() << QStringLiteral("writing to the virtual input device failed: %1")
                                             .arg(QString::fromLocal8Bit(std::strerror(-rc)));
    }
    m_lastError = rc < 0 ? rc : 0;
}

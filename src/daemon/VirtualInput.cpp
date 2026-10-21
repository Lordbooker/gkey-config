#include "VirtualInput.h"

#include "OutputCodes.h"

#include <libevdev/libevdev-uinput.h>
#include <libevdev/libevdev.h>

#include <cstring>

VirtualInput::~VirtualInput()
{
    if (m_uinput)
        libevdev_uinput_destroy(m_uinput);
    if (m_device)
        libevdev_free(m_device);
}

bool VirtualInput::create(QString *error)
{
    m_device = libevdev_new();
    libevdev_set_name(m_device, DeviceName);
    libevdev_set_id_bustype(m_device, BUS_VIRTUAL);
    libevdev_set_id_vendor(m_device, 0);
    libevdev_set_id_product(m_device, 0);
    libevdev_set_id_version(m_device, 1);

    libevdev_enable_event_type(m_device, EV_KEY);
    for (int code : OutputCodes::allKeys())
        libevdev_enable_event_code(m_device, EV_KEY, static_cast<unsigned int>(code), nullptr);

    // Relative axes for pointer and scroll actions (stick modes, milestone 5).
    libevdev_enable_event_type(m_device, EV_REL);
    for (unsigned int code : {REL_X, REL_Y, REL_WHEEL, REL_HWHEEL})
        libevdev_enable_event_code(m_device, EV_REL, code, nullptr);

    const int rc = libevdev_uinput_create_from_device(m_device, LIBEVDEV_UINPUT_OPEN_MANAGED, &m_uinput);
    if (rc < 0) {
        m_uinput = nullptr;
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
    if (m_uinput)
        libevdev_uinput_write_event(m_uinput, EV_KEY, static_cast<unsigned int>(code), pressed ? 1 : 0);
}

void VirtualInput::sync()
{
    if (m_uinput)
        libevdev_uinput_write_event(m_uinput, EV_SYN, SYN_REPORT, 0);
}

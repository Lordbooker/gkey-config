// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "InputDevice.h"

#include "SystemError.h"

#include <QLoggingCategory>
#include <QSocketNotifier>

#include <libevdev/libevdev.h>

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace {
Q_LOGGING_CATEGORY(lcInput, "gkey.input")
}

InputDevice::InputDevice(const QString &devNode, const QString &role, QObject *parent)
    : QObject(parent), m_devNode(devNode), m_role(role)
{
}

InputDevice::~InputDevice()
{
    close();
}

bool InputDevice::open(QString *error, bool grab)
{
    m_fd = ::open(m_devNode.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (m_fd < 0) {
        const int err = errno;
        *error = QStringLiteral("%1: %2").arg(m_devNode, SystemError::text(err));
        if (err == EACCES)
            *error += QStringLiteral(" - is the udev rule 70-gkey-config.rules installed? Replug the device after installing it.");
        return false;
    }

    int rc = libevdev_new_from_fd(m_fd, &m_device);
    if (rc < 0) {
        *error = QStringLiteral("%1: %2").arg(m_devNode, SystemError::text(-rc));
        close();
        return false;
    }

    if (grab && !setGrabbed(true, error)) {
        close();
        return false;
    }

    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &InputDevice::onReadable);
    return true;
}

bool InputDevice::setGrabbed(bool grabbed, QString *error)
{
    if (!m_device || grabbed == m_grabbed)
        return m_device != nullptr;
    const int rc = libevdev_grab(m_device, grabbed ? LIBEVDEV_GRAB : LIBEVDEV_UNGRAB);
    if (rc < 0) {
        if (error)
            *error = QStringLiteral("%1: exclusive access failed (%2) - another program grabs the device")
                         .arg(m_devNode, SystemError::text(-rc));
        return false;
    }
    m_grabbed = grabbed;
    return true;
}

int InputDevice::absValue(int code) const
{
    return m_device ? libevdev_get_event_value(m_device, EV_ABS, static_cast<unsigned int>(code)) : 0;
}

void InputDevice::onReadable()
{
    input_event event;
    unsigned int flags = LIBEVDEV_READ_FLAG_NORMAL;
    for (;;) {
        const int rc = libevdev_next_event(m_device, flags, &event);
        if (rc == LIBEVDEV_READ_STATUS_SYNC) {
            // Events were dropped: libevdev replays the state difference.
            flags = LIBEVDEV_READ_FLAG_SYNC;
        } else if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
            // nothing to change
        } else if (rc == -EAGAIN) {
            if (flags == LIBEVDEV_READ_FLAG_SYNC) {
                flags = LIBEVDEV_READ_FLAG_NORMAL;   // sync finished, continue normally
                continue;
            }
            return;
        } else {
            qCInfo(lcInput) << m_devNode << "closed:" << SystemError::text(-rc);
            close();
            emit failed(m_devNode);
            return;
        }

        if (event.type == EV_KEY)
            emit keyEvent(m_role, event.code, event.value);
        else if (event.type == EV_ABS)
            emit absEvent(m_role, event.code, event.value);
    }
}

void InputDevice::close()
{
    if (m_notifier) {
        m_notifier->setEnabled(false);
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }
    if (m_device) {
        if (m_grabbed)
            libevdev_grab(m_device, LIBEVDEV_UNGRAB);
        m_grabbed = false;
        libevdev_free(m_device);
        m_device = nullptr;
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "EventCodes.h"

#include <libevdev/libevdev.h>

namespace EventCodes {

namespace {

std::optional<int> codeFor(unsigned int type, const QString &name)
{
    const QByteArray utf8 = name.toUtf8();
    const int code = libevdev_event_code_from_name(type, utf8.constData());
    if (code < 0)
        return std::nullopt;
    return code;
}

}

std::optional<int> keyCode(const QString &name)
{
    return codeFor(EV_KEY, name);
}

std::optional<int> absCode(const QString &name)
{
    return codeFor(EV_ABS, name);
}

QString keyName(int code)
{
    const char *name = libevdev_event_code_get_name(EV_KEY, static_cast<unsigned int>(code));
    return name ? QString::fromLatin1(name) : QString();
}

}

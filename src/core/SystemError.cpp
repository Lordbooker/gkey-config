// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "SystemError.h"

#include <array>
#include <cstring>

namespace SystemError {

namespace {

// strerror_r() comes in two variants: the GNU one returns the text (which may
// or may not be in the buffer), the XSI one returns 0 and fills the buffer.
[[maybe_unused]] const char *result(const char *text, const char * /*buffer*/)
{
    return text;
}

[[maybe_unused]] const char *result(int status, const char *buffer)
{
    return status == 0 ? buffer : nullptr;
}

}

QString text(int errorNumber)
{
    std::array<char, 256> buffer{};
    const char *description = result(strerror_r(errorNumber, buffer.data(), buffer.size()), buffer.data());
    return description ? QString::fromLocal8Bit(description) : QStringLiteral("error %1").arg(errorNumber);
}

}

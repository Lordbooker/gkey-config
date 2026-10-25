// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <QString>

namespace SystemError {

// Description of an errno value, e.g. "Permission denied". Thread-safe, unlike
// std::strerror() (the LCD is written from a thread of its own).
QString text(int errorNumber);

}

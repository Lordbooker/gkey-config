// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <QString>

#include <optional>

// Conversion between evdev event code names ("KEY_MACRO1", "BTN_THUMB",
// "ABS_X") and their numeric values, based on libevdev's name tables.
namespace EventCodes {

// Numeric code of a key or button name (event type EV_KEY).
std::optional<int> keyCode(const QString &name);

// Numeric code of an absolute axis name (event type EV_ABS).
std::optional<int> absCode(const QString &name);

// Name of a key or button code; empty if unknown.
QString keyName(int code);

}

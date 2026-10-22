// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "DeviceModel.h"
#include "Profile.h"

#include <QMap>
#include <QString>

#include <optional>

// The LEDs of one physical device, found in sysfs below its HID node
// (<hid>/leds/<name>) and matched to the model's LED roles by name suffix.
//
// Writing needs membership in the group "gkey" (see 70-gkey-config.rules);
// without it the LEDs are left alone and a hint is logged once.
class LedController {
public:
    LedController(const DeviceModel &model, const QString &hidSyspath);

    // Roles found on the device, e.g. "backlight", "mode1".
    QList<QString> roles() const { return m_paths.keys(); }

    void setMode(int mode);
    void setRecording(bool recording);
    // Applies the colour if the device has an RGB backlight; nullopt leaves it unchanged.
    void setBacklight(const std::optional<Profile::Rgb> &rgb);
    // Applies the brightness level of a Level LED ("backlight", "lcd-backlight");
    // nullopt leaves it unchanged. Limited to the LED's max_brightness.
    void setLevel(const QString &role, const std::optional<int> &level);
    // Backlight of a profile: colour or levels, whatever the device has.
    void applyProfile(const Profile &profile);

    // True if a write failed for lack of permission.
    bool permissionDenied() const { return m_permissionDenied; }

private:
    void writeOnOff(const QString &role, bool on);
    bool writeAttribute(const QString &role, const QString &attribute, const QByteArray &value);

    DeviceModel m_model;
    QMap<QString, QString> m_paths;   // role -> LED class directory
    bool m_permissionDenied = false;
    bool m_permissionLogged = false;
};

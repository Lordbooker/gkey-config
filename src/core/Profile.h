// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "Action.h"
#include "StickSettings.h"

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>

#include <array>
#include <expected>
#include <optional>

class DeviceModel;

// One profile of one device model (docs/CONCEPT.md section 5).
//
//   {
//     "name": "Default",
//     "backlight": [51, 153, 255],      (RGB backlight, e.g. G13, G510)
//     "backlightLevel": 2,              (level backlight, e.g. G15: 0 off .. max)
//     "lcdBacklightLevel": 1,           (separate LCD backlight, G15)
//     "modes": {
//       "M1": { "G1": { "type": "key", "key": "KEY_1" } },
//       "M2": {},
//       "M3": {}
//     },
//     "stick": { ... }
//   }
//
// Nothing the daemon cannot use is lost when the profile is written again:
// unknown top-level members, unknown modes and bindings that are invalid or not
// bindable on the model (e.g. written by a newer version) are kept unchanged.
// Such bindings are inactive; setting or removing a binding of that key
// replaces them.
class Profile {
public:
    static constexpr int ModeCount = 3;

    struct Rgb {
        int red = 0;
        int green = 0;
        int blue = 0;
        bool operator==(const Rgb &other) const = default;
    };

    using Bindings = QMap<QString, Action>;   // key ID -> action

    // Parses a profile file. Invalid bindings do not fail the whole profile:
    // they are kept inactive and reported in warnings().
    static std::expected<Profile, QString> fromJson(const QByteArray &json);
    QByteArray toJson() const;

    // Makes bindings of key IDs that the model cannot bind inactive and reports
    // them in warnings().
    void validateFor(const DeviceModel &model);

    const QString &name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    const std::optional<Rgb> &backlight() const { return m_backlight; }
    void setBacklight(const std::optional<Rgb> &rgb) { m_backlight = rgb; }

    // Brightness levels for devices without colour (0 = off .. the LED's maximum).
    const std::optional<int> &backlightLevel() const { return m_backlightLevel; }
    void setBacklightLevel(const std::optional<int> &level) { m_backlightLevel = level; }
    const std::optional<int> &lcdBacklightLevel() const { return m_lcdBacklightLevel; }
    void setLcdBacklightLevel(const std::optional<int> &level) { m_lcdBacklightLevel = level; }

    // mode: 1..3
    const Bindings &bindings(int mode) const;
    void setBinding(int mode, const QString &keyId, const Action &action);
    void removeBinding(int mode, const QString &keyId);

    const QJsonObject &stick() const { return m_stick; }
    StickSettings stickSettings() const { return StickSettings::fromJson(m_stick); }
    void setStickSettings(const StickSettings &settings) { settings.writeTo(m_stick); }

    const QStringList &warnings() const { return m_warnings; }

private:
    static std::size_t modeIndex(int mode);

    QString m_name;
    std::optional<Rgb> m_backlight;
    std::optional<int> m_backlightLevel;
    std::optional<int> m_lcdBacklightLevel;
    std::array<Bindings, ModeCount> m_modes;
    std::array<QJsonObject, ModeCount> m_inactive;   // key ID -> binding as read, per mode
    QJsonObject m_unknownModes;                      // e.g. "M4" from a newer version
    QJsonObject m_stick;
    QJsonObject m_unknown;
    QStringList m_warnings;
};

// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <QJsonObject>
#include <QStringList>

// Stick settings of a profile (docs/CONCEPT.md section 7.1), stored in the
// profile's "stick" member:
//
//   "stick": { "mode": "keys", "deadzone": 0.25, "speed": 10, "scrollSpeed": 10 }
//
// Without a "stick" member the stick is left to the system (passthrough), so
// existing profiles behave as before.
struct StickSettings {
    enum class Mode { Keys, Mouse, Scroll, Passthrough };

    static constexpr double DefaultDeadzone = 0.25;
    static constexpr int DefaultSpeed = 10;
    static constexpr int MaxSpeed = 50;

    Mode mode = Mode::Passthrough;
    double deadzone = DefaultDeadzone;   // fraction of the travel from the centre, 0.05-0.9
    int speed = DefaultSpeed;            // mouse: pixels per 10 ms at full deflection, 1-50
    int scrollSpeed = DefaultSpeed;      // scroll: wheel steps per second at full deflection, 1-50

    // Invalid values fall back to the defaults and are reported in warnings.
    static StickSettings fromJson(const QJsonObject &json, QStringList *warnings = nullptr);

    // Writes the settings into json, keeping members it does not know.
    void writeTo(QJsonObject &json) const;

    static QString modeName(Mode mode);

    bool operator==(const StickSettings &other) const = default;
};

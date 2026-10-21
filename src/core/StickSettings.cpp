// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "StickSettings.h"

namespace {

struct ModeName {
    StickSettings::Mode mode;
    const char *name;
};

constexpr ModeName modeNames[] = {
    {StickSettings::Mode::Keys, "keys"},
    {StickSettings::Mode::Mouse, "mouse"},
    {StickSettings::Mode::Scroll, "scroll"},
    {StickSettings::Mode::Passthrough, "passthrough"},
};

}

QString StickSettings::modeName(Mode mode)
{
    for (const ModeName &entry : modeNames) {
        if (entry.mode == mode)
            return QLatin1String(entry.name);
    }
    return {};
}

StickSettings StickSettings::fromJson(const QJsonObject &json, QStringList *warnings)
{
    StickSettings settings;
    const auto warn = [warnings](const QString &text) {
        if (warnings)
            warnings->append(text);
    };

    if (json.contains(u"mode")) {
        const QString mode = json.value(u"mode").toString();
        bool known = false;
        for (const ModeName &entry : modeNames) {
            if (mode == QLatin1String(entry.name)) {
                settings.mode = entry.mode;
                known = true;
            }
        }
        if (!known)
            warn(QStringLiteral("stick: unknown mode \"%1\"; the stick is left to the system").arg(mode));
    }
    if (json.contains(u"deadzone")) {
        const double deadzone = json.value(u"deadzone").toDouble(-1);
        if (deadzone >= 0.05 && deadzone <= 0.9)
            settings.deadzone = deadzone;
        else
            warn(QStringLiteral("stick: \"deadzone\" must be between 0.05 and 0.9; default used"));
    }
    if (json.contains(u"speed")) {
        const int speed = json.value(u"speed").toInt(-1);
        if (speed >= 1 && speed <= MaxSpeed)
            settings.speed = speed;
        else
            warn(QStringLiteral("stick: \"speed\" must be between 1 and %1; default used").arg(MaxSpeed));
    }
    if (json.contains(u"scrollSpeed")) {
        const int speed = json.value(u"scrollSpeed").toInt(-1);
        if (speed >= 1 && speed <= MaxSpeed)
            settings.scrollSpeed = speed;
        else
            warn(QStringLiteral("stick: \"scrollSpeed\" must be between 1 and %1; default used").arg(MaxSpeed));
    }
    return settings;
}

void StickSettings::writeTo(QJsonObject &json) const
{
    json.insert(QStringLiteral("mode"), modeName(mode));
    json.insert(QStringLiteral("deadzone"), deadzone);
    json.insert(QStringLiteral("speed"), speed);
    json.insert(QStringLiteral("scrollSpeed"), scrollSpeed);
}

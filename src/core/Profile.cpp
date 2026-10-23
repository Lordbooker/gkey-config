// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "Profile.h"

#include "DeviceModel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>

#include <algorithm>

namespace {

QString modeName(int mode)
{
    return QStringLiteral("M%1").arg(mode);
}

}

std::expected<Profile, QString> Profile::fromJson(const QByteArray &json)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return std::unexpected(QStringLiteral("invalid JSON at offset %1: %2")
                                   .arg(parseError.offset)
                                   .arg(parseError.errorString()));
    if (!document.isObject())
        return std::unexpected(QStringLiteral("top level must be an object"));
    QJsonObject root = document.object();

    Profile profile;
    profile.m_name = root.take(QStringLiteral("name")).toString().trimmed();
    if (profile.m_name.isEmpty())
        return std::unexpected(QStringLiteral("\"name\" is missing"));

    const QJsonValue backlight = root.take(QStringLiteral("backlight"));
    if (!backlight.isUndefined()) {
        const QJsonArray rgb = backlight.toArray();
        const bool valid = rgb.size() == 3 && std::all_of(rgb.begin(), rgb.end(), [](const QJsonValue &v) {
            return v.isDouble() && v.toInt(-1) >= 0 && v.toInt(-1) <= 255;
        });
        if (valid)
            profile.m_backlight = Rgb{rgb.at(0).toInt(), rgb.at(1).toInt(), rgb.at(2).toInt()};
        else
            profile.m_warnings.append(QStringLiteral("\"backlight\" must be [r, g, b] with values 0-255; ignored"));
    }

    // Brightness levels (0..255; the daemon limits them to the LED's maximum).
    for (const auto &[key, target] : {std::pair{QStringLiteral("backlightLevel"), &profile.m_backlightLevel},
                                      std::pair{QStringLiteral("lcdBacklightLevel"), &profile.m_lcdBacklightLevel}}) {
        const QJsonValue value = root.take(key);
        if (value.isUndefined())
            continue;
        if (value.isDouble() && value.toInt(-1) >= 0 && value.toInt(-1) <= 255)
            *target = value.toInt();
        else
            profile.m_warnings.append(QStringLiteral("\"%1\" must be a number 0-255; ignored").arg(key));
    }

    const QJsonObject modes = root.take(QStringLiteral("modes")).toObject();
    for (auto it = modes.begin(); it != modes.end(); ++it) {
        int mode = 0;
        for (int m = 1; m <= ModeCount; ++m) {
            if (it.key() == modeName(m))
                mode = m;
        }
        if (mode == 0) {
            profile.m_warnings.append(QStringLiteral("unknown mode \"%1\"; ignored").arg(it.key()));
            profile.m_unknownModes.insert(it.key(), it.value());
            continue;
        }
        const QJsonObject bindings = it.value().toObject();
        for (auto binding = bindings.begin(); binding != bindings.end(); ++binding) {
            const auto action = Action::fromJson(binding.value().toObject());
            if (!action) {
                profile.m_warnings.append(QStringLiteral("%1/%2: %3; binding ignored")
                                              .arg(it.key(), binding.key(), action.error()));
                profile.m_inactive[modeIndex(mode)].insert(binding.key(), binding.value());
                continue;
            }
            profile.m_modes[modeIndex(mode)].insert(binding.key(), *action);
        }
    }

    profile.m_stick = root.take(QStringLiteral("stick")).toObject();
    StickSettings::fromJson(profile.m_stick, &profile.m_warnings);   // report invalid values
    profile.m_unknown = root;
    return profile;
}

QByteArray Profile::toJson() const
{
    QJsonObject root = m_unknown;
    root.insert(QStringLiteral("name"), m_name);
    if (m_backlight)
        root.insert(QStringLiteral("backlight"), QJsonArray{m_backlight->red, m_backlight->green, m_backlight->blue});
    if (m_backlightLevel)
        root.insert(QStringLiteral("backlightLevel"), *m_backlightLevel);
    if (m_lcdBacklightLevel)
        root.insert(QStringLiteral("lcdBacklightLevel"), *m_lcdBacklightLevel);

    QJsonObject modes = m_unknownModes;
    for (int mode = 1; mode <= ModeCount; ++mode) {
        QJsonObject bindings = m_inactive[modeIndex(mode)];
        const Bindings &map = m_modes[modeIndex(mode)];
        for (auto it = map.begin(); it != map.end(); ++it)
            bindings.insert(it.key(), it.value().toJson());
        modes.insert(modeName(mode), bindings);
    }
    root.insert(QStringLiteral("modes"), modes);

    if (!m_stick.isEmpty())
        root.insert(QStringLiteral("stick"), m_stick);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

void Profile::validateFor(const DeviceModel &model)
{
    const QList<QString> bindable = model.bindableKeyIds();
    const QSet<QString> allowed(bindable.begin(), bindable.end());
    for (int mode = 1; mode <= ModeCount; ++mode) {
        Bindings &map = m_modes[modeIndex(mode)];
        for (auto it = map.begin(); it != map.end();) {
            if (allowed.contains(it.key())) {
                ++it;
                continue;
            }
            m_warnings.append(QStringLiteral("%1/%2: key cannot be bound on the %3; binding ignored")
                                  .arg(modeName(mode), it.key(), model.name()));
            m_inactive[modeIndex(mode)].insert(it.key(), it.value().toJson());
            it = map.erase(it);
        }
    }
}

const Profile::Bindings &Profile::bindings(int mode) const
{
    return m_modes[modeIndex(mode)];
}

void Profile::setBinding(int mode, const QString &keyId, const Action &action)
{
    m_inactive[modeIndex(mode)].remove(keyId);
    m_modes[modeIndex(mode)].insert(keyId, action);
}

void Profile::removeBinding(int mode, const QString &keyId)
{
    m_inactive[modeIndex(mode)].remove(keyId);
    m_modes[modeIndex(mode)].remove(keyId);
}

int Profile::modeIndex(int mode)
{
    return std::clamp(mode, 1, ModeCount) - 1;
}

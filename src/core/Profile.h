#pragma once

#include "Action.h"

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
//     "backlight": [51, 153, 255],
//     "modes": {
//       "M1": { "G1": { "type": "key", "key": "KEY_1" } },
//       "M2": {},
//       "M3": {}
//     },
//     "stick": { ... }
//   }
//
// Unknown top-level members (e.g. written by a newer version) are kept and
// written back unchanged.
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
    // they are skipped and reported in warnings().
    static std::expected<Profile, QString> fromJson(const QByteArray &json);
    QByteArray toJson() const;

    // Removes bindings for key IDs that the model cannot bind and reports them
    // in warnings().
    void validateFor(const DeviceModel &model);

    const QString &name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    const std::optional<Rgb> &backlight() const { return m_backlight; }
    void setBacklight(const std::optional<Rgb> &rgb) { m_backlight = rgb; }

    // mode: 1..3
    const Bindings &bindings(int mode) const;
    void setBinding(int mode, const QString &keyId, const Action &action);
    void removeBinding(int mode, const QString &keyId);

    const QJsonObject &stick() const { return m_stick; }

    const QStringList &warnings() const { return m_warnings; }

private:
    static int modeIndex(int mode);

    QString m_name;
    std::optional<Rgb> m_backlight;
    std::array<Bindings, ModeCount> m_modes;
    QJsonObject m_stick;
    QJsonObject m_unknown;
    QStringList m_warnings;
};

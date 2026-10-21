#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <expected>

// A macro from the shared macro library (docs/CONCEPT.md section 6).
//
//   {
//     "name": "copy-line",
//     "description": "Select the current line and copy it",
//     "steps": [
//       { "tap": "KEY_HOME" },
//       { "press": "KEY_LEFTSHIFT" },
//       { "tap": "KEY_END", "hold": 20 },
//       { "release": "KEY_LEFTSHIFT" },
//       { "delay": 30 },
//       { "press": "BTN_LEFT" }, { "release": "BTN_LEFT" }
//     ]
//   }
//
// "tap" presses and releases a key; "hold" is the time in between
// (default DefaultTapHold ms). Keys are keyboard keys or mouse buttons.
// How a macro repeats is part of the binding (Action), not of the macro.
class Macro {
public:
    static constexpr int DefaultTapHold = 10;   // ms
    static constexpr int MaxDelay = 60000;      // ms, guards against typos

    struct Step {
        enum class Type { Press, Release, Tap, Delay };
        Type type = Type::Delay;
        int code = 0;       // Press, Release, Tap
        int duration = 0;   // Delay: wait time; Tap: hold time (ms)
        bool operator==(const Step &other) const = default;
    };

    static std::expected<Macro, QString> fromJson(const QByteArray &json);
    QByteArray toJson() const;

    const QString &name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }
    const QString &description() const { return m_description; }
    void setDescription(const QString &description) { m_description = description; }
    const QList<Step> &steps() const { return m_steps; }
    void setSteps(const QList<Step> &steps) { m_steps = steps; }

private:
    QString m_name;
    QString m_description;
    QList<Step> m_steps;
    QJsonObject m_unknown;
};

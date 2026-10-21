#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

#include <expected>

// What a bound key does (docs/CONCEPT.md section 5.2).
//
// JSON forms:
//   { "type": "key",     "key": "KEY_A" }
//   { "type": "combo",   "keys": ["KEY_LEFTCTRL", "KEY_C"] }
//   { "type": "mouse",   "button": "BTN_LEFT" }
//   { "type": "macro",   "macro": "name", "repeat": "once|while_held|toggle|count", "count": 3 }
//   { "type": "mode",    "mode": 2 }
//   { "type": "profile", "profile": "Gaming" | "next" | "previous" }
//   { "type": "lcd",     "page": "next" | "previous" | "<page>" }
class Action {
public:
    enum class Type { Key, Combo, Mouse, Macro, Mode, Profile, Lcd };
    enum class Repeat { Once, WhileHeld, Toggle, Count };

    static std::expected<Action, QString> fromJson(const QJsonObject &json);
    QJsonObject toJson() const;

    static Action key(int code);
    static Action combo(const QList<int> &codes);
    static Action mouse(int button);
    static Action mode(int mode);
    static Action profile(const QString &target);

    Type type() const { return m_type; }

    // Key, Combo, Mouse: the evdev codes to press (in this order) and release
    // (in reverse order).
    const QList<int> &codes() const { return m_codes; }

    // Macro
    const QString &macro() const { return m_macro; }
    Repeat repeat() const { return m_repeat; }
    int count() const { return m_count; }

    // Mode (1..3)
    int modeNumber() const { return m_mode; }

    // Profile: profile name, "next" or "previous"
    const QString &target() const { return m_target; }

    // Lcd: page name, "next" or "previous"
    const QString &page() const { return m_page; }

    bool operator==(const Action &other) const = default;

private:
    Type m_type = Type::Key;
    QList<int> m_codes;
    QString m_macro;
    Repeat m_repeat = Repeat::Once;
    int m_count = 1;
    int m_mode = 1;
    QString m_target;
    QString m_page;
};

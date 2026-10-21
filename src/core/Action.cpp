#include "Action.h"

#include "EventCodes.h"
#include "OutputCodes.h"

#include <QJsonArray>

namespace {

struct TypeName {
    Action::Type type;
    const char *name;
};

constexpr TypeName typeNames[] = {
    {Action::Type::Key, "key"},       {Action::Type::Combo, "combo"},
    {Action::Type::Mouse, "mouse"},   {Action::Type::Macro, "macro"},
    {Action::Type::Mode, "mode"},     {Action::Type::Profile, "profile"},
    {Action::Type::Lcd, "lcd"},
};

struct RepeatName {
    Action::Repeat repeat;
    const char *name;
};

constexpr RepeatName repeatNames[] = {
    {Action::Repeat::Once, "once"},
    {Action::Repeat::WhileHeld, "while_held"},
    {Action::Repeat::Toggle, "toggle"},
    {Action::Repeat::Count, "count"},
};

std::unexpected<QString> fail(const QString &message)
{
    return std::unexpected(message);
}

// Keyboard key by evdev name; error text if unknown or not offered by the virtual device.
std::expected<int, QString> keyboardKey(const QString &name)
{
    const auto code = EventCodes::keyCode(name);
    if (!code)
        return fail(QStringLiteral("unknown key \"%1\"").arg(name));
    if (!OutputCodes::isKeyboardKey(*code))
        return fail(QStringLiteral("\"%1\" is not a keyboard key").arg(name));
    return *code;
}

}

std::expected<Action, QString> Action::fromJson(const QJsonObject &json)
{
    const QString typeName = json.value(u"type").toString();
    Action action;
    bool known = false;
    for (const TypeName &entry : typeNames) {
        if (typeName == QLatin1String(entry.name)) {
            action.m_type = entry.type;
            known = true;
        }
    }
    if (!known)
        return fail(QStringLiteral("unknown action type \"%1\"").arg(typeName));

    switch (action.m_type) {
    case Type::Key: {
        const auto code = keyboardKey(json.value(u"key").toString());
        if (!code)
            return fail(code.error());
        action.m_codes = {*code};
        break;
    }
    case Type::Combo: {
        const QJsonArray keys = json.value(u"keys").toArray();
        if (keys.size() < 2)
            return fail(QStringLiteral("a combo needs at least two keys"));
        for (const QJsonValue &key : keys) {
            const auto code = keyboardKey(key.toString());
            if (!code)
                return fail(code.error());
            if (action.m_codes.contains(*code))
                return fail(QStringLiteral("key \"%1\" appears twice in the combo").arg(key.toString()));
            action.m_codes.append(*code);
        }
        break;
    }
    case Type::Mouse: {
        const QString name = json.value(u"button").toString();
        const auto code = EventCodes::keyCode(name);
        if (!code || !OutputCodes::isMouseButton(*code))
            return fail(QStringLiteral("\"%1\" is not a mouse button").arg(name));
        action.m_codes = {*code};
        break;
    }
    case Type::Macro: {
        action.m_macro = json.value(u"macro").toString();
        if (action.m_macro.isEmpty())
            return fail(QStringLiteral("a macro action needs \"macro\""));
        const QString repeat = json.value(u"repeat").toString(QStringLiteral("once"));
        bool repeatKnown = false;
        for (const RepeatName &entry : repeatNames) {
            if (repeat == QLatin1String(entry.name)) {
                action.m_repeat = entry.repeat;
                repeatKnown = true;
            }
        }
        if (!repeatKnown)
            return fail(QStringLiteral("unknown repeat mode \"%1\"").arg(repeat));
        action.m_count = json.value(u"count").toInt(1);
        if (action.m_repeat == Repeat::Count && action.m_count < 1)
            return fail(QStringLiteral("\"count\" must be at least 1"));
        break;
    }
    case Type::Mode:
        action.m_mode = json.value(u"mode").toInt();
        if (action.m_mode < 1 || action.m_mode > 3)
            return fail(QStringLiteral("\"mode\" must be 1, 2 or 3"));
        break;
    case Type::Profile:
        action.m_target = json.value(u"profile").toString();
        if (action.m_target.isEmpty())
            return fail(QStringLiteral("a profile action needs \"profile\""));
        break;
    case Type::Lcd:
        action.m_page = json.value(u"page").toString();
        if (action.m_page.isEmpty())
            return fail(QStringLiteral("an lcd action needs \"page\""));
        break;
    }
    return action;
}

QJsonObject Action::toJson() const
{
    QJsonObject json;
    for (const TypeName &entry : typeNames) {
        if (entry.type == m_type)
            json.insert(QStringLiteral("type"), QLatin1String(entry.name));
    }
    switch (m_type) {
    case Type::Key:
        json.insert(QStringLiteral("key"), EventCodes::keyName(m_codes.value(0)));
        break;
    case Type::Combo: {
        QJsonArray keys;
        for (int code : m_codes)
            keys.append(EventCodes::keyName(code));
        json.insert(QStringLiteral("keys"), keys);
        break;
    }
    case Type::Mouse:
        json.insert(QStringLiteral("button"), EventCodes::keyName(m_codes.value(0)));
        break;
    case Type::Macro:
        json.insert(QStringLiteral("macro"), m_macro);
        for (const RepeatName &entry : repeatNames) {
            if (entry.repeat == m_repeat)
                json.insert(QStringLiteral("repeat"), QLatin1String(entry.name));
        }
        if (m_repeat == Repeat::Count)
            json.insert(QStringLiteral("count"), m_count);
        break;
    case Type::Mode:
        json.insert(QStringLiteral("mode"), m_mode);
        break;
    case Type::Profile:
        json.insert(QStringLiteral("profile"), m_target);
        break;
    case Type::Lcd:
        json.insert(QStringLiteral("page"), m_page);
        break;
    }
    return json;
}

Action Action::key(int code)
{
    Action action;
    action.m_type = Type::Key;
    action.m_codes = {code};
    return action;
}

Action Action::combo(const QList<int> &codes)
{
    Action action;
    action.m_type = Type::Combo;
    action.m_codes = codes;
    return action;
}

Action Action::mouse(int button)
{
    Action action;
    action.m_type = Type::Mouse;
    action.m_codes = {button};
    return action;
}

Action Action::mode(int mode)
{
    Action action;
    action.m_type = Type::Mode;
    action.m_mode = mode;
    return action;
}

Action Action::profile(const QString &target)
{
    Action action;
    action.m_type = Type::Profile;
    action.m_target = target;
    return action;
}

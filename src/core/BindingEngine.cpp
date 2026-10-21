#include "BindingEngine.h"

#include "KeyOutput.h"

#include <QLoggingCategory>

#include <algorithm>

Q_LOGGING_CATEGORY(lcEngine, "gkey.engine")

BindingEngine::BindingEngine(const DeviceModel &model, KeyOutput &output, QObject *parent)
    : QObject(parent), m_model(model), m_output(output)
{
}

BindingEngine::~BindingEngine()
{
    releaseAll();
}

void BindingEngine::setProfile(const Profile &profile)
{
    releaseAll();
    m_profile = profile;
}

void BindingEngine::setMode(int mode)
{
    releaseAll();
    m_mode = std::clamp(mode, 1, Profile::ModeCount);
}

void BindingEngine::handleKey(const QString &inputRole, int code, int value)
{
    if (value != 0 && value != 1)
        return;   // autorepeat: the target application handles repeat itself
    const DeviceModel::Key *key = m_model.keyForCode(inputRole, code);
    if (!key) {
        qCDebug(lcEngine) << "unmapped event" << inputRole << code;
        return;
    }
    const bool pressed = value == 1;

    switch (key->role) {
    case DeviceModel::KeyRole::Mode1:
    case DeviceModel::KeyRole::Mode2:
    case DeviceModel::KeyRole::Mode3:
        if (pressed)
            emit modeRequested(static_cast<int>(key->role) - static_cast<int>(DeviceModel::KeyRole::Mode1) + 1);
        return;
    case DeviceModel::KeyRole::Record:
        if (pressed)
            emit recordRequested();
        return;
    case DeviceModel::KeyRole::Hardware:
        return;
    case DeviceModel::KeyRole::Bindable:
        break;
    }

    if (pressed) {
        if (m_held.contains(key->id))
            return;   // duplicate press without release
        const auto &bindings = m_profile.bindings(m_mode);
        const auto it = bindings.constFind(key->id);
        if (it == bindings.constEnd())
            return;   // unbound in this mode
        press(key->id, it.value());
    } else {
        const auto it = m_held.find(key->id);
        if (it == m_held.end())
            return;
        const Action action = it.value();
        m_held.erase(it);
        release(action);
    }
    m_output.sync();
}

void BindingEngine::releaseAll()
{
    if (m_held.isEmpty())
        return;
    const auto held = m_held;
    m_held.clear();
    for (const Action &action : held)
        release(action);
    m_output.sync();
}

void BindingEngine::press(const QString &keyId, const Action &action)
{
    switch (action.type()) {
    case Action::Type::Key:
    case Action::Type::Combo:
    case Action::Type::Mouse:
        m_held.insert(keyId, action);
        for (int code : action.codes())
            m_output.press(code);
        break;
    case Action::Type::Mode:
        emit modeRequested(action.modeNumber());
        break;
    case Action::Type::Profile:
        emit profileRequested(action.target());
        break;
    case Action::Type::Macro:
    case Action::Type::Lcd:
        qCInfo(lcEngine) << "action type not implemented yet, key" << keyId;
        break;
    }
}

void BindingEngine::release(const Action &action)
{
    const QList<int> &codes = action.codes();
    for (auto it = codes.crbegin(); it != codes.crend(); ++it)
        m_output.release(*it);
}

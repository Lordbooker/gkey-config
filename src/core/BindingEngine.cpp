// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "BindingEngine.h"

#include "KeyOutput.h"
#include "MacroRun.h"
#include "MacroStore.h"

#include <QLoggingCategory>

#include <algorithm>
#include <ranges>
#include <utility>

namespace {
Q_LOGGING_CATEGORY(lcEngine, "gkey.engine")
}

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
    handle(key->id, key->role, value == 1);
}

void BindingEngine::handleKeyId(const QString &keyId, bool pressed)
{
    if (const DeviceModel::Key *key = m_model.keyById(keyId))
        handle(key->id, key->role, pressed);
    else if (DeviceModel::stickDirectionKeys().contains(keyId))
        handle(keyId, DeviceModel::KeyRole::Bindable, pressed);
}

void BindingEngine::handle(const QString &keyId, DeviceModel::KeyRole role, bool pressed)
{
    emit keyEvent(keyId, pressed);

    switch (role) {
    case DeviceModel::KeyRole::Mode1:
    case DeviceModel::KeyRole::Mode2:
    case DeviceModel::KeyRole::Mode3:
        if (pressed)
            emit modeRequested(static_cast<int>(role) - static_cast<int>(DeviceModel::KeyRole::Mode1) + 1);
        return;
    case DeviceModel::KeyRole::Record:
        if (pressed)
            emit recordRequested();
        return;
    case DeviceModel::KeyRole::Hardware:
        return;
    case DeviceModel::KeyRole::LcdNext:
        // Fixed: the only way through the LCD pages on the device itself.
        if (pressed)
            emit lcdPageRequested(QStringLiteral("next"));
        return;
    case DeviceModel::KeyRole::Passthrough: {
        // The device is grabbed, so keys meant for the system (G510 mute keys)
        // are sent on through the virtual device as they are.
        const DeviceModel::Key *key = m_model.keyById(keyId);
        if (!key)
            return;
        if (pressed && !m_held.contains(keyId)) {
            press(keyId, Action::key(key->code));
        } else if (!pressed) {
            const auto it = m_held.find(keyId);
            if (it == m_held.end())
                return;
            const Action action = std::move(it.value());   // erase() removes the original
            m_held.erase(it);
            release(action);
        }
        m_output.sync();
        return;
    }
    case DeviceModel::KeyRole::Bindable:
        break;
    }

    if (pressed) {
        if (m_held.contains(keyId))
            return;   // duplicate press without release
        if (MacroRun *run = m_runs.value(keyId)) {
            run->stop();   // a new press stops a running macro
            return;
        }
        const auto &bindings = m_profile.bindings(m_mode);
        const auto it = bindings.constFind(keyId);
        if (it != bindings.constEnd()) {
            press(keyId, it.value());
        } else if (const DeviceModel::Key *key = m_model.keyById(keyId); key && key->defaultAction) {
            press(keyId, *key->defaultAction);
        } else {
            return;   // unbound in this mode
        }
    } else {
        if (MacroRun *run = m_runs.value(keyId)) {
            if (m_runRepeat.value(keyId) == Action::Repeat::WhileHeld)
                run->keyReleased();
            return;
        }
        const auto it = m_held.find(keyId);
        if (it == m_held.end())
            return;
        const Action action = std::move(it.value());   // erase() removes the original
        m_held.erase(it);
        release(action);
    }
    m_output.sync();
}

void BindingEngine::stopMacros()
{
    const auto runs = m_runs;
    for (MacroRun *run : runs)
        run->stop();
}

void BindingEngine::releaseAll()
{
    stopMacros();
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
        startMacro(keyId, action);
        break;
    case Action::Type::Lcd:
        emit lcdPageRequested(action.page());
        break;
    }
}

void BindingEngine::release(const Action &action)
{
    for (const int code : std::views::reverse(action.codes()))
        m_output.release(code);
}

void BindingEngine::startMacro(const QString &keyId, const Action &action)
{
    const Macro *macro = m_macros ? m_macros->macro(action.macro()) : nullptr;
    if (!macro) {
        qCWarning(lcEngine).noquote() << QStringLiteral("key %1: macro \"%2\" does not exist").arg(keyId, action.macro());
        return;
    }
    auto *run = new MacroRun(*macro, action.repeat(), action.count(), m_output, this);
    m_runs.insert(keyId, run);
    m_runRepeat.insert(keyId, action.repeat());
    connect(run, &MacroRun::finished, this, [this, keyId, run] {
        if (m_runs.value(keyId) == run) {
            m_runs.remove(keyId);
            m_runRepeat.remove(keyId);
        }
        run->deleteLater();
    });
    run->start();
}

// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "Action.h"
#include "DeviceModel.h"
#include "Profile.h"

#include <QMap>
#include <QObject>
#include <QString>

class KeyOutput;
class MacroRun;
class MacroStore;

// Turns the key events of one physical device into the actions of the active
// profile and mode.
//
// The action is remembered when a key goes down, so its release always undoes
// exactly what the press did. Switching profile or mode first releases
// everything that is still held, so no key can get stuck.
class BindingEngine : public QObject {
    Q_OBJECT
public:
    BindingEngine(const DeviceModel &model, KeyOutput &output, QObject *parent = nullptr);
    ~BindingEngine() override;

    // Macro library used by macro actions (may be null: macros do nothing).
    void setMacroStore(const MacroStore *store) { m_macros = store; }

    void setProfile(const Profile &profile);
    void setMode(int mode);   // 1..3
    int mode() const { return m_mode; }

    // An evdev key event from the input device with the given input role
    // ("keys", "stick"). value: 1 press, 0 release, 2 autorepeat (ignored).
    void handleKey(const QString &inputRole, int code, int value);

    // A key given by its ID, e.g. the stick directions "STICK_UP" produced by
    // the stick processing.
    void handleKeyId(const QString &keyId, bool pressed);

    // Releases all held keys and stops running macros (e.g. before the device
    // goes away).
    void releaseAll();

    // Stops running macros only.
    void stopMacros();

    // Number of macros currently running.
    int runningMacros() const { return static_cast<int>(m_runs.size()); }

signals:
    // A mode key or a mode action asks for another mode.
    void modeRequested(int mode);
    // A profile action asks for another profile ("next", "previous" or a name).
    void profileRequested(const QString &target);
    // An LCD action asks for another page ("next", "previous" or a page name).
    void lcdPageRequested(const QString &page);
    // MR was pressed.
    void recordRequested();
    // Any mapped key of the device went down or up (for the GUI).
    void keyEvent(const QString &keyId, bool pressed);

private:
    void handle(const QString &keyId, DeviceModel::KeyRole role, bool pressed);
    void press(const QString &keyId, const Action &action);
    void release(const Action &action);
    void startMacro(const QString &keyId, const Action &action);

    DeviceModel m_model;
    KeyOutput &m_output;
    Profile m_profile;
    int m_mode = 1;
    QMap<QString, Action> m_held;   // key ID -> action started by its press
    QMap<QString, MacroRun *> m_runs;   // key ID -> running macro
    QMap<QString, Action::Repeat> m_runRepeat;
    const MacroStore *m_macros = nullptr;
};

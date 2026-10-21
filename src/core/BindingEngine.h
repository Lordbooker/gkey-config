#pragma once

#include "Action.h"
#include "DeviceModel.h"
#include "Profile.h"

#include <QMap>
#include <QObject>
#include <QString>

class KeyOutput;

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

    void setProfile(const Profile &profile);
    void setMode(int mode);   // 1..3
    int mode() const { return m_mode; }

    // An evdev key event from the input device with the given input role
    // ("keys", "stick"). value: 1 press, 0 release, 2 autorepeat (ignored).
    void handleKey(const QString &inputRole, int code, int value);

    // Releases all held keys (e.g. before the device goes away).
    void releaseAll();

signals:
    // A mode key or a mode action asks for another mode.
    void modeRequested(int mode);
    // A profile action asks for another profile ("next", "previous" or a name).
    void profileRequested(const QString &target);
    // MR was pressed.
    void recordRequested();

private:
    void press(const QString &keyId, const Action &action);
    void release(const Action &action);

    DeviceModel m_model;
    KeyOutput &m_output;
    Profile m_profile;
    int m_mode = 1;
    QMap<QString, Action> m_held;   // key ID -> action started by its press
};

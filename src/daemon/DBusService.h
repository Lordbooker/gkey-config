// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <QObject>
#include <QString>

class Daemon;

// D-Bus API of gkeyd on the session bus (docs/CONCEPT.md section 10).
//
//   service    io.github.lordbooker.GKeyConfig
//   object     /io/github/lordbooker/GKeyConfig
//   interface  io.github.lordbooker.GKeyConfig1
//
// Owning the service name also guarantees that only one daemon runs per session.
class DBusService : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.lordbooker.GKeyConfig1")
    Q_PROPERTY(bool Recording READ recording)

public:
    static constexpr const char *ServiceName = "io.github.lordbooker.GKeyConfig";
    static constexpr const char *ObjectPath = "/io/github/lordbooker/GKeyConfig";

    explicit DBusService(Daemon *daemon);

    // Registers the object and claims the service name.
    bool registerOnSessionBus(QString *error);

    bool recording() const;

public slots:
    // JSON: version, recording, devices[] (model, syspath, ledPermission, lcdPage,
    // stickCenter), models{id: {name, tested, profile, mode, profiles[], profileDir}},
    // macros[], macroDir
    QString Status();
    bool SetProfile(const QString &model, const QString &name);
    bool SetMode(const QString &model, int mode);
    // Temporary colour until the next profile change.
    bool SetBacklight(const QString &model, int red, int green, int blue);
    // Restores the colour of the active profile.
    bool ResetBacklight(const QString &model);
    void Reload();
    bool RunMacro(const QString &name);
    void StopMacros();
    void SetRecording(bool recording);
    // Shows text on the LCD text page ("\n" separates lines); seconds > 0
    // returns to the previous page afterwards.
    void LcdText(const QString &text, int seconds);
    // "next", "previous", "status", "system", "cpu", "amdgpu", "nvidia",
    // "clock", "text", "profile" or "intro" (plays the start animation).
    void LcdPage(const QString &page);
    // Takes the current stick position as its rest position.
    bool CalibrateStick(const QString &model);

signals:
    void DeviceAdded(const QString &model, const QString &syspath);
    void DeviceRemoved(const QString &model, const QString &syspath);
    void ProfileChanged(const QString &model, const QString &name);
    void ModeChanged(const QString &model, int mode);
    void KeyEvent(const QString &model, const QString &key, bool pressed);
    void RecordingChanged(bool recording);

private:
    Daemon *m_daemon;
};

// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <QColor>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

class QDBusMessage;
class QDBusServiceWatcher;

// Talks to gkeyd over D-Bus (docs/CONCEPT.md section 10). Works without a
// running daemon: calls then fail and available() is false.
//
// All calls are asynchronous: a daemon that hangs must not freeze the window.
// Calls that report a result take a callback; it runs when the answer has
// arrived (false after an error or a timeout of 3 seconds), and is dropped if
// the context object is destroyed before. Calls reach the daemon in the order
// they are made.
class DaemonClient : public QObject {
    Q_OBJECT
public:
    explicit DaemonClient(QObject *parent = nullptr);

    bool available() const { return m_available; }

    using Done = std::function<void(bool)>;

    // Last status returned by the daemon (refreshed by refreshStatus()).
    const QJsonObject &status() const { return m_status; }
    // Asks for the status; statusChanged() follows when it has arrived, then
    // done (if given) runs with context.
    void refreshStatus(QObject *context = nullptr, const std::function<void()> &done = {});

    QStringList connectedModels() const;
    QString activeProfile(const QString &modelId) const;
    int activeMode(const QString &modelId) const;
    bool ledPermission(const QString &modelId) const;
    bool recording() const;
    // Rest position of the stick of a connected device, e.g. "138 / 139"; empty if unknown.
    QString stickCenter(const QString &modelId) const;

    void setProfile(const QString &modelId, const QString &name);
    void setMode(const QString &modelId, int mode);
    void setBacklight(const QString &modelId, const QColor &color);
    void resetBacklight(const QString &modelId);
    void runMacro(const QString &name, QObject *context = nullptr, const Done &done = {});
    void stopMacros();
    void setRecording(bool recording);
    // done runs after the status has been refreshed, so stickCenter() is current.
    void calibrateStick(const QString &modelId, QObject *context = nullptr, const Done &done = {});
    void lcdPage(const QString &page);

signals:
    void availableChanged(bool available);
    void statusChanged();
    void deviceAdded(const QString &modelId);
    void deviceRemoved(const QString &modelId);
    void profileChanged(const QString &modelId, const QString &name);
    void modeChanged(const QString &modelId, int mode);
    void keyEvent(const QString &modelId, const QString &key, bool pressed);
    void recordingChanged(bool recording);

private slots:
    // Slots for the D-Bus signals (connected by name).
    void onDeviceAdded(const QString &model, const QString &syspath);
    void onDeviceRemoved(const QString &model, const QString &syspath);
    void onProfileChanged(const QString &model, const QString &name);
    void onModeChanged(const QString &model, int mode);
    void onKeyEvent(const QString &model, const QString &key, bool pressed);
    void onRecordingChanged(bool recording);

private:
    void setAvailable(bool available);
    // Asynchronous call; onReply gets the reply or the error message.
    void call(const QString &method, const QList<QVariant> &arguments, QObject *context,
              const std::function<void(const QDBusMessage &)> &onReply);
    void send(const QString &method, const QList<QVariant> &arguments = {}) const;
    // Call of a method that returns a boolean; done gets false after an error.
    void callBool(const QString &method, const QList<QVariant> &arguments, QObject *context = nullptr,
                  const Done &done = {});

    QDBusServiceWatcher *m_watcher = nullptr;
    QJsonObject m_status;
    bool m_available = false;
};

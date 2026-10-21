#pragma once

#include <QColor>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

class QDBusMessage;
class QDBusServiceWatcher;

// Talks to gkeyd over D-Bus (docs/CONCEPT.md section 10). Works without a
// running daemon: calls then fail and available() is false.
class DaemonClient : public QObject {
    Q_OBJECT
public:
    explicit DaemonClient(QObject *parent = nullptr);

    bool available() const { return m_available; }

    // Last status returned by the daemon (refreshed by refreshStatus()).
    const QJsonObject &status() const { return m_status; }
    void refreshStatus();

    QStringList connectedModels() const;
    QString activeProfile(const QString &modelId) const;
    int activeMode(const QString &modelId) const;
    bool ledPermission(const QString &modelId) const;
    bool recording() const;
    // Rest position of the stick of a connected device, e.g. "138 / 139"; empty if unknown.
    QString stickCenter(const QString &modelId) const;

    bool setProfile(const QString &modelId, const QString &name);
    bool setMode(const QString &modelId, int mode);
    bool setBacklight(const QString &modelId, const QColor &color);
    bool resetBacklight(const QString &modelId);
    bool runMacro(const QString &name);
    void stopMacros();
    void setRecording(bool recording);
    bool calibrateStick(const QString &modelId);
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
    QDBusMessage call(const QString &method, const QList<QVariant> &arguments = {}) const;
    void send(const QString &method, const QList<QVariant> &arguments = {}) const;
    bool callBool(const QString &method, const QList<QVariant> &arguments);

    QDBusServiceWatcher *m_watcher = nullptr;
    QJsonObject m_status;
    bool m_available = false;
};

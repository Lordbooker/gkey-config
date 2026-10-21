#pragma once

#include <QMap>
#include <QPoint>
#include <QString>

#include <optional>

// Runtime state that survives restarts: active profile and mode per device
// model. Stored in $XDG_STATE_HOME/gkey-config/state.json.
//
//   { "models": { "g13": { "profile": "Default", "mode": 1, "stickCenter": [138, 139] } } }
class DaemonState {
public:
    explicit DaemonState(const QString &filePath);

    void load();
    bool save() const;

    QString profile(const QString &modelId) const;
    void setProfile(const QString &modelId, const QString &profile);

    // 1..3; 1 if unknown
    int mode(const QString &modelId) const;
    void setMode(const QString &modelId, int mode);

    // Calibrated rest position of the stick; nullopt if not calibrated.
    std::optional<QPoint> stickCenter(const QString &modelId) const;
    void setStickCenter(const QString &modelId, const QPoint &center);

private:
    struct Entry {
        QString profile;
        int mode = 1;
        std::optional<QPoint> stickCenter;
    };

    QString m_filePath;
    QMap<QString, Entry> m_entries;
};

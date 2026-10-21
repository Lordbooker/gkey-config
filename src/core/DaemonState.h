#pragma once

#include <QMap>
#include <QString>

// Runtime state that survives restarts: active profile and mode per device
// model. Stored in $XDG_STATE_HOME/gkey-config/state.json.
//
//   { "models": { "g13": { "profile": "Default", "mode": 1 } } }
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

private:
    struct Entry {
        QString profile;
        int mode = 1;
    };

    QString m_filePath;
    QMap<QString, Entry> m_entries;
};

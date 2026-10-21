#include "ConfigPaths.h"

#include <QDir>
#include <QStandardPaths>

namespace ConfigPaths {

QString configDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + QStringLiteral("/gkey-config");
}

QString profileDir(const QString &modelId)
{
    return configDir() + QStringLiteral("/profiles/") + modelId;
}

QString macroDir()
{
    return configDir() + QStringLiteral("/macros");
}

QString stateDir()
{
    // QStandardPaths::GenericStateLocation needs Qt 6.7; resolve it by hand.
    QString base = qEnvironmentVariable("XDG_STATE_HOME");
    if (base.isEmpty() || !QDir::isAbsolutePath(base))
        base = QDir::homePath() + QStringLiteral("/.local/state");
    return base + QStringLiteral("/gkey-config");
}

}

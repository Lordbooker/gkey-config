// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "MacroRename.h"

#include "DeviceModel.h"
#include "MacroStore.h"
#include "Profile.h"
#include "ProfileStore.h"

namespace MacroRename {

Result rename(MacroStore &macros, const QString &from, const QString &to, const QList<DeviceModel> &models,
              const std::function<QString(const QString &)> &profileDir)
{
    Result result;
    QString error;
    if (!macros.rename(from, to, &error)) {
        result.errors.append(error);
        return result;
    }
    result.renamed = true;
    if (from == to)
        return result;

    for (const DeviceModel &model : models) {
        ProfileStore store(model, profileDir(model.id()));
        for (const QString &name : store.names()) {
            const Profile *stored = store.profile(name);
            if (!stored)
                continue;
            Profile profile = *stored;
            const int count = profile.renameMacro(from, to);
            if (count == 0)
                continue;
            if (!store.save(profile, &error)) {
                result.errors.append(error);
                continue;
            }
            result.bindings += count;
            ++result.profiles;
        }
    }
    return result;
}

}

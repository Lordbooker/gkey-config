// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "Macro.h"
#include "Profile.h"

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

#include <expected>
#include <functional>

class MacroStore;

// Export and import of a profile together with the macros it plays, to share
// or back up a profile:
//
//   {
//     "gkeyConfigExport": 1,
//     "model": "g13",
//     "profile": { ...profile, as in its file... },
//     "macros": [ { ...macro, as in its file... }, ... ]
//   }
//
// A plain profile file (as in the profile directory) can be imported as well;
// it brings no macros and belongs to the model shown at that moment.
namespace ProfileTransfer {

constexpr int FormatVersion = 1;
// Larger files are not read (profile and macro files have some kilobytes).
constexpr qint64 MaxFileMiB = 4;
constexpr qint64 MaxFileSize = MaxFileMiB * 1024 * 1024;

// The export of the profile with every macro of macros it plays; macros it
// plays that do not exist are listed in missing (if given).
QByteArray exportProfile(const QString &modelId, const Profile &profile, const MacroStore &macros,
                         QStringList *missing = nullptr);

struct Import {
    QString modelId;   // the file's model, or fallbackModelId for a plain profile file
    Profile profile;
    QList<Macro> macros;
};

// Reads an export or a plain profile file.
std::expected<Import, QString> parse(const QByteArray &data, const QString &fallbackModelId);

// name, or "name (2)", "name (3)", ... whichever is not taken.
QString uniqueName(const QString &name, const std::function<bool(const QString &)> &taken);

// What happens to the macros of an import: macros that do not exist yet are
// saved; one that exists with the same steps and description is used as it
// is; one whose name is taken by a different macro is saved under a new name
// (renamed), and the profile plays that copy.
struct MacroPlan {
    QList<Macro> save;                 // with their final names
    QMap<QString, QString> renamed;    // name in the file -> name of the copy
    QStringList reused;                // existing macros used as they are
};
MacroPlan planMacros(const QList<Macro> &imported, const MacroStore &macros);

// Carries out the plan: saves the macros, then makes the profile play the
// renamed copies. False (with error) if a macro cannot be saved; macros saved
// before stay.
bool applyMacros(const MacroPlan &plan, MacroStore &macros, Profile &profile, QString *error = nullptr);

}

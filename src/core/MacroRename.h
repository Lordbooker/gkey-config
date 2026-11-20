// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

class DeviceModel;
class MacroStore;

// Renaming a macro: profiles name the macros they play, so the bindings in
// the profiles of all models are renamed with it.
namespace MacroRename {

struct Result {
    bool renamed = false;   // the macro itself
    int bindings = 0;       // bindings changed in the profiles
    int profiles = 0;       // profiles written
    QStringList errors;     // macro or profiles that could not be written
};

// Renames the macro in the store, then its bindings in the profiles of all
// models; profileDir gives the profile directory of a model ID. If the macro
// cannot be renamed, nothing else is changed.
Result rename(MacroStore &macros, const QString &from, const QString &to, const QList<DeviceModel> &models,
              const std::function<QString(const QString &)> &profileDir);

}

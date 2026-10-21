// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <QString>

// Locations of the user's files (XDG Base Directory Specification).
namespace ConfigPaths {

// $XDG_CONFIG_HOME/gkey-config (settings, profiles, macros)
QString configDir();

// $XDG_CONFIG_HOME/gkey-config/profiles/<modelId>
QString profileDir(const QString &modelId);

// $XDG_CONFIG_HOME/gkey-config/settings.json (global settings)
QString settingsFile();

// $XDG_CONFIG_HOME/gkey-config/macros (shared by all models)
QString macroDir();

// $XDG_STATE_HOME/gkey-config (state that is not configuration, e.g. the active profile)
QString stateDir();

}

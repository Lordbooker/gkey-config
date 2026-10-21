// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <QString>

// Membership of the current user in a group (e.g. "gkey" for the LEDs).
namespace GroupAccess {

enum class State {
    Missing,       // the group does not exist
    NotMember,     // the user is not in the group
    NeedsRelogin,  // in the group database, but not yet in this login session
    Active,        // this process has the group
};

State state(const QString &group);

// Login name of the current user.
QString userName();

}

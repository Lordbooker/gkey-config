// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "GroupAccess.h"

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

namespace GroupAccess {

QString userName()
{
    const passwd *pw = getpwuid(getuid());
    return pw ? QString::fromLocal8Bit(pw->pw_name) : qEnvironmentVariable("USER");
}

State state(const QString &group)
{
    const struct group *entry = getgrnam(group.toLocal8Bit().constData());
    if (!entry)
        return State::Missing;
    const gid_t gid = entry->gr_gid;

    // Groups of this process = groups of the login session.
    if (getegid() == gid)
        return State::Active;
    const int count = getgroups(0, nullptr);
    if (count > 0) {
        std::vector<gid_t> groups(static_cast<size_t>(count));
        const int read = getgroups(count, groups.data());
        if (read > 0 && std::find(groups.begin(), groups.begin() + read, gid) != groups.begin() + read)
            return State::Active;
    }

    // Group database: a change takes effect at the next login.
    const passwd *pw = getpwuid(getuid());
    if (!pw)
        return State::NotMember;
    int size = 32;
    std::vector<gid_t> groups(static_cast<size_t>(size));
    if (getgrouplist(pw->pw_name, pw->pw_gid, groups.data(), &size) < 0) {
        groups.resize(static_cast<size_t>(size));
        if (getgrouplist(pw->pw_name, pw->pw_gid, groups.data(), &size) < 0)
            return State::NotMember;
    }
    const auto end = groups.begin() + size;
    return std::find(groups.begin(), end, gid) != end ? State::NeedsRelogin : State::NotMember;
}

}

// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "GroupAccess.h"

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <optional>
#include <vector>

namespace GroupAccess {

namespace {

// getpwuid() and getgrnam() return static data and are not thread-safe; their
// _r variants need a buffer, which grows while it is too small (ERANGE).
template <typename Lookup>
bool withBuffer(Lookup lookup)
{
    std::vector<char> buffer(1024);
    for (;;) {
        const int rc = lookup(buffer);
        if (rc != ERANGE)
            return rc == 0;
        if (buffer.size() >= std::size_t{1024} * 1024)
            return false;
        buffer.resize(buffer.size() * 2);
    }
}

struct User {
    QByteArray name;
    gid_t group = 0;
};

std::optional<User> currentUser()
{
    std::optional<User> user;
    withBuffer([&user](std::vector<char> &buffer) {
        passwd entry{};
        passwd *found = nullptr;
        const int rc = getpwuid_r(getuid(), &entry, buffer.data(), buffer.size(), &found);
        if (rc == 0 && found)
            user = User{QByteArray(entry.pw_name), entry.pw_gid};
        return rc;
    });
    return user;
}

std::optional<gid_t> groupId(const QByteArray &name)
{
    std::optional<gid_t> gid;
    withBuffer([&gid, &name](std::vector<char> &buffer) {
        group entry{};
        group *found = nullptr;
        const int rc = getgrnam_r(name.constData(), &entry, buffer.data(), buffer.size(), &found);
        if (rc == 0 && found)
            gid = entry.gr_gid;
        return rc;
    });
    return gid;
}

}

QString userName()
{
    const std::optional<User> user = currentUser();
    return user ? QString::fromLocal8Bit(user->name) : qEnvironmentVariable("USER");
}

State state(const QString &group)
{
    const std::optional<gid_t> found = groupId(group.toLocal8Bit());
    if (!found)
        return State::Missing;
    const gid_t gid = *found;

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
    const std::optional<User> user = currentUser();
    if (!user)
        return State::NotMember;
    int size = 32;
    std::vector<gid_t> groups(static_cast<size_t>(size));
    if (getgrouplist(user->name.constData(), user->group, groups.data(), &size) < 0) {
        groups.resize(static_cast<size_t>(size));
        if (getgrouplist(user->name.constData(), user->group, groups.data(), &size) < 0)
            return State::NotMember;
    }
    const auto end = groups.begin() + size;
    return std::find(groups.begin(), end, gid) != end ? State::NeedsRelogin : State::NotMember;
}

}

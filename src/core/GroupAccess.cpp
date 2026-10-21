#include "GroupAccess.h"

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

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
        std::vector<gid_t> groups(count);
        const int read = getgroups(count, groups.data());
        for (int i = 0; i < read; ++i) {
            if (groups[i] == gid)
                return State::Active;
        }
    }

    // Group database: a change takes effect at the next login.
    const passwd *pw = getpwuid(getuid());
    if (!pw)
        return State::NotMember;
    int size = 32;
    std::vector<gid_t> groups(size);
    if (getgrouplist(pw->pw_name, pw->pw_gid, groups.data(), &size) < 0) {
        groups.resize(size);
        if (getgrouplist(pw->pw_name, pw->pw_gid, groups.data(), &size) < 0)
            return State::NotMember;
    }
    for (int i = 0; i < size; ++i) {
        if (groups[i] == gid)
            return State::NeedsRelogin;
    }
    return State::NotMember;
}

}

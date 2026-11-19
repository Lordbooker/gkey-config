#!/bin/sh
# SPDX-FileCopyrightText: 2026 Lordbooker
# SPDX-License-Identifier: GPL-2.0-only
#
# Tags a release on main, after the pull request from dev has been merged
# (CONTRIBUTING.md, "Releases"):
#
#   scripts/tag-release.sh [--yes]
#
# The version comes from CMakeLists.txt on main. Before it tags, the script
# checks that
#   - dev has no commits that main lacks (the pull request has been merged),
#   - the release is complete on main: the tag does not exist yet, and
#     CHANGELOG.md and the AppStream file have an entry for the version.
# Then it shows what it will do and asks (--yes: does not ask), creates the
# annotated tag on the newest commit of main, pushes it (which starts the
# release workflow) and fast-forwards dev to main.
#
# Works on the remote branches; the checked-out branch and the working tree
# are not touched. Remote: $REMOTE, default origin.

set -eu

remote=${REMOTE:-origin}
assume_yes=false
case ${1:-} in
    --yes) assume_yes=true ;;
    "") ;;
    *)
        echo "usage: $0 [--yes]" >&2
        exit 2
        ;;
esac

fail() {
    echo "error: $*" >&2
    exit 1
}

cd "$(dirname "$0")/.."

echo "Fetching $remote ..."
git fetch --quiet --tags "$remote" main dev || fail "cannot fetch main and dev from $remote"
main=$(git rev-parse --verify --quiet "refs/remotes/$remote/main") || fail "$remote/main not found"
dev=$(git rev-parse --verify --quiet "refs/remotes/$remote/dev") || fail "$remote/dev not found"

# The pull request from dev has been merged.
missing=$(git rev-list --count "$main..$dev")
if [ "$missing" -ne 0 ]; then
    git --no-pager log --oneline "$main..$dev" >&2
    fail "$remote/dev has $missing commit(s) that $remote/main lacks: merge the pull request from dev first"
fi

version=$(git show "$main:CMakeLists.txt" | sed -n 's/^project(gkey-config VERSION \([0-9.]*\).*/\1/p')
[ -n "$version" ] || fail "no version in CMakeLists.txt on $remote/main"
tag="v$version"

# The release is complete on main.
if git rev-parse --verify --quiet "refs/tags/$tag" >/dev/null ||
    [ -n "$(git ls-remote --tags "$remote" "refs/tags/$tag")" ]; then
    fail "$tag exists already: raise the version on dev and merge it into main first"
fi
git show "$main:CHANGELOG.md" | grep -q "^## \[$version\]" ||
    fail "CHANGELOG.md on $remote/main has no section \"## [$version]\""
git show "$main:data/metainfo/io.github.lordbooker.GKeyConfig.metainfo.xml" |
    grep -q "<release version=\"$version\"" ||
    fail "the AppStream file on $remote/main has no release $version"

echo
echo "Release $tag on $remote/main:"
git --no-pager log --oneline -1 "$main"
echo
echo "This creates the annotated tag $tag on that commit, pushes it to $remote"
echo "(the workflow \"Release packages\" then publishes the release) and"
echo "fast-forwards $remote/dev to it."
if ! $assume_yes; then
    printf 'Continue? [y/N] '
    read -r answer
    case $answer in
        y | Y | yes) ;;
        *)
            echo "Nothing done."
            exit 1
            ;;
    esac
fi

git tag -a "$tag" -m "gkey-config $version" "$main"
git push "$remote" "refs/tags/$tag"
# Only a fast-forward (dev has nothing that main lacks, see above).
git push "$remote" "$main:refs/heads/dev"
echo
echo "Done: $tag is pushed. Follow the release under Actions > Release packages."

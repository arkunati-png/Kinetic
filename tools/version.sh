#!/bin/sh

# Kinetic version string: tagged releases are named "vX.Y.Z" in git; fall
# back to the VERSION file when there is no git, or no matching tag yet.
if test -d .git && type git >/dev/null 2>&1 ; then
v=$(git describe --tags --match 'v[0-9]*' 2>/dev/null | sed -e 's/^v//' -e 's/-/-git-/')
if test -n "$v" ; then echo "$v" ; else sed 's/$/-git/' < VERSION ; fi
else
cat VERSION
fi

#!/bin/sh
# Build Kinetic into a throwaway prefix. Usage: tests/build.sh [configure-args...]
# Environment: KINETIC_WORK (default /tmp/kinetic-work) holds build/ and prefix/.
set -e
here=$(cd "$(dirname "$0")/.." && pwd)
work=${KINETIC_WORK:-/tmp/kinetic-work}
rm -rf "$work"
mkdir -p "$work/build"
cd "$work/build"
"$here/configure" --prefix="$work/prefix" --syslibdir="$work/prefix/lib" "$@" >configure.log 2>&1 \
	|| { cat configure.log; exit 1; }
make -j"$(nproc 2>/dev/null || echo 2)" >make.log 2>&1 || { grep -E "error:|Error [0-9]|undefined reference|warning: " make.log | head -30; exit 1; }
make install >install.log 2>&1
echo "built: $work/prefix"

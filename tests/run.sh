#!/bin/sh
# Build Kinetic and run the compatibility tests, statically and dynamically
# linked. Also checks that --disable-compat produces a libc without the
# Kinetic additions. Usage: tests/run.sh
set -e
here=$(cd "$(dirname "$0")/.." && pwd)
work=${KINETIC_WORK:-/tmp/kinetic-work}
export KINETIC_WORK="$work"

sh "$here/tests/build.sh"
cc="$work/prefix/bin/musl-gcc"
cd "$work"

$cc -O1 -Wall -Wno-unused-result -static -o compat-static "$here/tests/compat.c"
$cc -O1 -Wall -Wno-unused-result -o compat-dynamic "$here/tests/compat.c"

echo "=== static ==="
./compat-static
echo "=== dynamic ==="
./compat-dynamic

# Size report and opt-out check
full=$(wc -c < "$work/prefix/lib/libc.so")
KINETIC_WORK="$work-nocompat" sh "$here/tests/build.sh" --disable-compat
bare=$(wc -c < "$work-nocompat/prefix/lib/libc.so")
echo "libc.so: $full bytes with compat, $bare bytes without (+$((full - bare)))"
if nm -D "$work-nocompat/prefix/lib/libc.so" | grep -q ' T obstack_free'; then
	echo "FAIL: --disable-compat still exports Kinetic symbols"; exit 1
fi
if [ -e "$work-nocompat/prefix/include/error.h" ]; then
	echo "FAIL: --disable-compat still installs Kinetic headers"; exit 1
fi
echo "ALL OK"

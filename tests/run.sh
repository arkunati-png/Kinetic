#!/bin/sh
# Build Kinetic and run the compatibility tests, statically and dynamically
# linked. Also checks that --disable-compat produces a libc without the
# Kinetic additions, and (if a cross toolchain and qemu-user are available)
# repeats the tests for aarch64 (opt-in, experimental).
#
#   tests/run.sh
#
# Optional environment:
#   KINETIC_WORK            scratch directory   (default /tmp/kinetic-work)
#   KINETIC_AARCH64_CROSS   cross prefix, e.g. ~/a64/aarch64-linux-musl-cross/bin/aarch64-linux-musl-
#   KINETIC_QEMU_AARCH64    path to qemu-aarch64(-static)
set -e
here=$(cd "$(dirname "$0")/.." && pwd)
work=${KINETIC_WORK:-/tmp/kinetic-work}

# run_suite NAME "CC COMMAND" [runner...]: compile and run both test
# programs with the given compiler, static and dynamic. A runner (such as
# qemu) is prepended to the test command when given.
run_suite() {
	name=$1; cc=$2; shift 2
	out="$work/$name-tests"
	mkdir -p "$out"
	for t in compat compat_ext stress; do
		$cc -O1 -Wall -Wno-unused-result -static -o "$out/$t-static" "$here/tests/$t.c" -lpthread
		$cc -O1 -Wall -Wno-unused-result -o "$out/$t-dynamic" "$here/tests/$t.c" -lpthread
		for link in static dynamic; do
			echo "=== $name: $t ($link) ==="
			if "$@" "$out/$t-$link" >"$out/$t-$link.log" 2>&1; then
				tail -n 1 "$out/$t-$link.log"
			else
				grep -E '^FAIL|checks,' "$out/$t-$link.log"
				echo "TEST FAILURE in $name: $t ($link)"
				exit 1
			fi
		done
	done
}

KINETIC_WORK="$work/x86_64" sh "$here/tests/build.sh"
run_suite x86_64 "$work/x86_64/prefix/bin/musl-gcc"

# Opt-out check and size report
full=$(wc -c < "$work/x86_64/prefix/lib/libc.so")
KINETIC_WORK="$work/nocompat" sh "$here/tests/build.sh" --disable-compat
bare=$(wc -c < "$work/nocompat/prefix/lib/libc.so")
echo "libc.so: $full bytes with compat, $bare bytes without (+$((full - bare)))"
if nm -D "$work/nocompat/prefix/lib/libc.so" | grep -q ' T obstack_free'; then
	echo "FAIL: --disable-compat still exports Kinetic symbols"; exit 1
fi
for h in error.h fts.h argp.h sys/tree.h; do
	if [ -e "$work/nocompat/prefix/include/$h" ]; then
		echo "FAIL: --disable-compat still installs $h"; exit 1
	fi
done

# aarch64 under qemu-user. Opt-in: only when both variables are set.
cross=${KINETIC_AARCH64_CROSS:-}
qemu=${KINETIC_QEMU_AARCH64:-}
if [ -n "$cross" ] && [ -n "$qemu" ] && [ -x "${cross}gcc" ] && [ -x "$qemu" ]; then
	toolbin=$(dirname "$cross")
	PATH="$toolbin:$PATH" CROSS_COMPILE="$(basename "$cross")" \
		KINETIC_WORK="$work/aarch64" sh "$here/tests/build.sh"
	p="$work/aarch64/prefix"
	# cross builds get no musl-gcc wrapper; use the specs it would have used
	sh "$here/tools/musl-gcc.specs.sh" "$p/include" "$p/lib" "$p/lib/ld-musl-aarch64.so.1" \
		> "$p/lib/musl-gcc.specs"
	PATH="$toolbin:$PATH" run_suite aarch64 "${cross}gcc -specs=$p/lib/musl-gcc.specs" "$qemu"
else
	echo "(aarch64 tests skipped: set KINETIC_AARCH64_CROSS and KINETIC_QEMU_AARCH64 to enable)"
fi
echo "ALL OK"

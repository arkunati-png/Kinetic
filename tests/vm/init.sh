#!/bin/busybox sh
# PID 1 of the Kinetic test VM. Boots, runs the test programs against
# Kinetic as the system libc, prints one RESULT line per test, powers off.

/bin/busybox mount -t proc proc /proc
/bin/busybox mount -t sysfs sys /sys
/bin/busybox mount -t devtmpfs dev /dev 2>/dev/null
/bin/busybox mount -t tmpfs tmp /tmp
[ -c /dev/null ] || /bin/busybox mknod /dev/null c 1 3
export PATH=/bin:/sbin
export HOME=/root TERM=linux

echo
echo "=== KINETIC VM BOOT ==="
uname -a
echo "kernel cmdline: $(cat /proc/cmdline)"
echo "system libc / loader:"
ls -la /lib/ld-musl-x86_64.so.1
/lib/ld-musl-x86_64.so.1 2>&1 | head -2
echo "PID 1 is $(readlink /proc/1/exe)  interpreter-linked: $(head -c 4 /bin/busybox | od -c | head -1)"
echo

cd /tests
fails=0
run() {
	name=$1; shift
	echo "--- $name"
	if "$@" >/tmp/$name.log 2>&1; then
		echo "RESULT $name PASS: $(tail -n 1 /tmp/$name.log)"
	else
		rc=$?
		fails=$((fails+1))
		echo "RESULT $name FAIL (rc=$rc)"
		tail -n 20 /tmp/$name.log
	fi
}

run compat-dyn ./compat-dyn
run compat-static ./compat-static
run compat_ext-dyn ./compat_ext-dyn
run compat_ext-static ./compat_ext-static
run busybox-smoke sh /tests/smoke.sh
run stress-dyn ./stress-dyn 6
run stress-static ./stress-static 6

# SQLite: output must match the reference produced by a glibc build
run sqlite sh -c './sqlite-dyn /tmp/db < t.sql > /tmp/sqlite.out 2>&1 && cmp /tmp/sqlite.out expected.out && echo "sqlite output identical to glibc reference"'

# Lua's own test suite (soft, portable mode)
run lua sh -c 'cd /tests/lt && ../lua-dyn -e"_U=true" all.lua 2>&1 | tail -n 3 | tr "\n" " "; cd /tests/lt && ../lua-dyn -e"_U=true" all.lua >/dev/null 2>&1'

echo
echo "=== SUMMARY: $fails failing test group(s) ==="
if [ "$fails" = 0 ]; then echo "KINETIC-VM-RESULT: ALL PASS"; else echo "KINETIC-VM-RESULT: FAILURES"; fi
echo "memory at end:"; head -n 3 /proc/meminfo
/bin/busybox poweroff -f
sleep 5

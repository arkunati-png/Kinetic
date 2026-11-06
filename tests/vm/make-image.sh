#!/bin/sh
# Build an initramfs whose system libc is Kinetic, for booting in QEMU.
#
# Everything is compiled against an installed Kinetic ($KROOT) but with the
# dynamic loader path set to /lib/ld-musl-x86_64.so.1, exactly as on a real
# system, and Kinetic's libc.so is installed at that path inside the image.
#
# Required environment (all absolute paths):
#   KROOT   installed Kinetic prefix       (from tests/build.sh)
#   KSRC    Kinetic source tree            (this repository)
#   BB      configured, dynamic busybox source tree
#   BBLIST  a runnable busybox to list applets from (--list)
#   SQ      sqlite amalgamation directory (shell.c, sqlite3.c)
#   SQREF   reference output for SQ/t.sql produced by a different libc
#   LUA     Lua source tree (makefile at top level)
#   LUAT    Lua test-suite directory
#   KH      directory of linux kernel headers (linux/, asm/, ...)
#   OUT     output directory
set -e
for v in KROOT KSRC BB BBLIST SQ SQREF LUA LUAT KH OUT; do
	eval "[ -n \"\$$v\" ]" || { echo "missing \$$v"; exit 1; }
done

rm -rf "$OUT" && mkdir -p "$OUT"
sh "$KSRC/tools/musl-gcc.specs.sh" "$KROOT/include" "$KROOT/lib" /lib/ld-musl-x86_64.so.1 > "$OUT/vm.specs"
CC="gcc -specs=$OUT/vm.specs"
R="$OUT/root"
mkdir -p "$R"/bin "$R"/sbin "$R"/etc "$R"/proc "$R"/sys "$R"/dev "$R"/tmp "$R"/lib "$R"/tests "$R"/root

echo "== libc"
cp "$KROOT/lib/libc.so" "$R/lib/ld-musl-x86_64.so.1"

echo "== test programs"
for t in compat compat_ext; do
	$CC -O1 -Wno-unused-result -o "$R/tests/$t-dyn" "$KSRC/tests/$t.c"
	$CC -O1 -Wno-unused-result -static -o "$R/tests/$t-static" "$KSRC/tests/$t.c"
done
$CC -O2 -Wno-unused-result -o "$R/tests/stress-dyn" "$KSRC/tests/stress.c" -lpthread
$CC -O2 -Wno-unused-result -static -o "$R/tests/stress-static" "$KSRC/tests/stress.c" -lpthread

echo "== busybox"
rm -rf "$OUT/bb" && cp -r "$BB" "$OUT/bb"
( cd "$OUT/bb" && make -s clean >/dev/null 2>&1 \
	&& make -j"$(nproc)" CC="$CC" HOSTCC=gcc EXTRA_CFLAGS="-isystem $KH" >"$OUT/bb.log" 2>&1 ) \
	|| { tail -20 "$OUT/bb.log"; exit 1; }
cp "$OUT/bb/busybox" "$R/bin/busybox"
for a in $("$BBLIST" --list); do
	case "$a" in
	init|poweroff|halt|reboot|mount|umount|swapon|swapoff|mknod|ifconfig|route|ip) ln -sf /bin/busybox "$R/sbin/$a" ;;
	esac
	ln -sf busybox "$R/bin/$a" 2>/dev/null || true
done
ln -sf /bin/busybox "$R/bin/sh"

echo "== sqlite"
$CC -O2 -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_ENABLE_FTS5 -DSQLITE_ENABLE_JSON1 \
	-o "$R/tests/sqlite-dyn" "$SQ/shell.c" "$SQ/sqlite3.c" -lpthread -lm
cp "$SQ/t.sql" "$R/tests/t.sql"
cp "$SQREF" "$R/tests/expected.out"

echo "== lua"
rm -rf "$OUT/lua" && cp -r "$LUA" "$OUT/lua"
( cd "$OUT/lua" && make -s clean >/dev/null 2>&1; make -s -j"$(nproc)" CC="$CC" CFLAGS="-Wall -O2 -std=c99 -DLUA_USE_LINUX -fno-stack-protector -fno-common" MYLIBS=-ldl >"$OUT/lua.log" 2>&1 ) \
	|| { tail -20 "$OUT/lua.log"; exit 1; }
cp "$OUT/lua/lua" "$R/tests/lua-dyn"
cp -r "$LUAT" "$R/tests/lt"

echo "== scripts"
cp "$KSRC/tests/vm/init.sh" "$R/init"
cp "$KSRC/tests/vm/smoke.sh" "$R/tests/smoke.sh"
chmod +x "$R/init" "$R/tests/smoke.sh"
echo "root:x:0:0:root:/root:/bin/sh" > "$R/etc/passwd"
echo "root:x:0:" > "$R/etc/group"

echo "== initramfs"
if command -v cpio >/dev/null 2>&1; then CPIO=cpio; else CPIO="$BBLIST cpio"; fi
( cd "$R" && find . | $CPIO -o -H newc 2>/dev/null | gzip -9 > "$OUT/initramfs.cpio.gz" )
ls -la "$OUT/initramfs.cpio.gz" | awk '{print "initramfs bytes:", $5}'
echo "interpreters in image:"
for f in bin/busybox tests/compat-dyn tests/sqlite-dyn tests/lua-dyn tests/stress-dyn; do
	printf '  %-22s %s\n' "$f" "$(readelf -l "$R/$f" 2>/dev/null | sed -n 's/.*interpreter: \(.*\)\]/\1/p')"
done

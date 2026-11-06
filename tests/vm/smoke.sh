#!/bin/sh
# Busybox applet smoke test, run inside the VM. Each check compares real
# output with a known-good value; exits non-zero if any check fails.
n=0; bad=0
ok() { n=$((n+1)); if [ "$1" = "$2" ]; then :; else bad=$((bad+1)); echo "FAIL [$3]: got '$1' want '$2'"; fi; }

ok "$(echo hello | sed 's/l/L/g')" "heLLo" sed
ok "$(printf 'b\na\nc\n' | sort | tr '\n' ' ')" "a b c " sort
ok "$(seq 1 1000 | awk '{s+=$1} END{print s}')" "500500" awk
ok "$(echo -n abc | sha256sum | cut -d' ' -f1)" "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" sha256sum
ok "$(echo -n abc | md5sum | cut -d' ' -f1)" "900150983cd24fb0d6963f7d28e17f72" md5sum
ok "$(date -u -d @86400 +%F)" "1970-01-02" date
ok "$(echo 'a1b22c333' | grep -oE '[0-9]+' | tr '\n' ',')" "1,22,333," grep-E
ok "$(printf 'x y z' | wc -w)" "3" wc
ok "$(echo 255 | awk '{printf "%x", $1}')" "ff" printf

d=/tmp/smoke.$$
mkdir -p $d/src/sub && cd $d
i=0; while [ $i -lt 200 ]; do echo "line $i" > src/sub/f$i; i=$((i+1)); done
ln -s sub src/link
tar czf a.tgz src && mkdir out && tar xzf a.tgz -C out
ok "$(diff -r src out/src >/dev/null 2>&1; echo $?)" "0" tar-roundtrip
ok "$(ls out/src/sub | wc -l)" "200" ls
ok "$(find out -type f | wc -l)" "200" find
ok "$(find out -name 'f1*' | wc -l)" "111" find-name
ok "$(grep -rl 'line 7$' out | wc -l)" "1" grep-r
ok "$(readlink out/src/link)" "sub" symlink
cp -a src copy && mv copy moved
ok "$(ls moved/sub | wc -l)" "200" cp-mv
ok "$(gzip -c src/sub/f1 | gunzip)" "line 1" gzip
ok "$(echo test | bzip2 | bunzip2)" "test" bzip2
dd if=/dev/zero of=big bs=1k count=2048 2>/dev/null
ok "$(wc -c < big)" "2097152" dd
ok "$(stat -c %s big)" "2097152" stat

# processes and signals
sleep 30 & p=$!
kill -0 $p && ok "alive" "alive" kill0
kill -TERM $p; wait $p 2>/dev/null
ok "$?" "143" sigterm-status
ok "$(sh -c 'exit 7'; echo $?)" "7" exit-status
ok "$( (echo a; echo b) | xargs -n1 echo | tr '\n' ' ')" "a b " xargs

# environment and process substitution
ok "$(FOO=bar sh -c 'echo $FOO')" "bar" env
ok "$(echo $(( 7 * 6 )))" "42" arith
ok "$(cat /proc/self/status | grep -c '^Name')" "1" procfs
ok "$(mount | grep -c '^proc ')" "1" mount

# pipes under load
ok "$(seq 1 20000 | sort -n | tail -n 1)" "20000" pipe-sort
ok "$(seq 1 5000 | gzip | gunzip | wc -l)" "5000" pipe-gzip

cd /; rm -rf $d
echo "smoke: $n checks, $bad failures"
[ "$bad" = 0 ]

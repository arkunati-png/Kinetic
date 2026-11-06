# Kinetic

Kinetic is a small, fast, MIT-licensed C library for Linux: a fork of
[musl libc](https://musl.libc.org) (v1.2.5) that keeps musl's size and
simplicity and adds the glibc-isms that most often stop real software from
building or running on musl.

* **As light as musl.** The base is unmodified musl. The compatibility layer
  adds about 33 KB (4.2%) to `libc.so` and, in static builds, is only linked
  into programs that call it (a program using none of it is byte-identical to
  one built with musl). `./configure --disable-compat` removes it entirely.
* **Drop-in for musl.** The ABI is musl's. The dynamic loader keeps the name
  `ld-musl-<arch>.so.1`, so existing musl binaries run unchanged on Kinetic.
* **More apps.** Missing headers and functions that cause "this doesn't build
  on Alpine" are provided; see [COMPAT.md](COMPAT.md) for the full list and
  the honest limitations.
* **MIT.** Everything Kinetic adds is original code under the MIT license.
  Nothing is copied from glibc (LGPL) or the BSDs.

## Build

```sh
./configure --prefix=/usr/local/kinetic
make
make install
```

Then compile with the generated `musl-gcc` wrapper (`$prefix/bin/musl-gcc`).
Build requirements and cross-compiling are exactly musl's; see `INSTALL`.

## Test

```sh
tests/run.sh      # builds Kinetic, runs the compat tests static + dynamic
```

## Status

Version 0.1.0. Developed and tested on **x86_64 Linux only**. The
compatibility layer is architecture-neutral except `getcontext`/`setcontext`/
`makecontext`/`swapcontext`, which exist for x86_64 only. Other architectures
are untested. See COMPAT.md for the feature list, limitations, and what is not
done (notably NSS and `iconv`).

### Verification (x86_64, 2026-11-06)

Everything below was run against this tree. "Stock musl" means unmodified
musl 1.2.5 built the same way, used as the control.

| Test | Result |
|------|--------|
| musl's libc-test suite (~476 programs) | Kinetic and stock musl fail the **same 7** tests and print the same 113 math-accuracy warnings (all in code Kinetic does not touch: `strptime` `%F/%s/%z`, a few `fmal`/`powf` exception flags, `malloc-brk-fail`, two missing `unistd.h` constants) |
| Kinetic's own tests (`tests/run.sh`) | 433 checks, 0 failures, static and dynamic; plus a threaded stress test (about 1M allocations with integrity checks, 3,000 forks, signals, condvar hand-offs) |
| zlib 1.3.1 `make check` | passes (static, shared, 64-bit) |
| SQLite 3.46.1 (200k-row workload, WAL, FTS5, JSON) | output identical to a glibc build |
| Lua 5.4.7 official test suite (soft mode, 55 sections) | passes on Kinetic, stock musl and glibc, after skipping one locale check that assumes `setlocale("pt_BR")` fails when the locale is absent; musl accepts any name |
| BusyBox 1.36.1 `make check` | dynamic: 908 pass / 6 fail; static: 909 pass / 18 fail. Fail lists are **identical to stock musl** in both modes |
| Booted system (`tests/vm/`) | Linux 6.8 kernel under QEMU 8.2 (TCG), BusyBox as PID 1, Kinetic as the system libc at `/lib/ld-musl-x86_64.so.1`. All test groups pass on 3 configurations (2, 4 and 8 vCPUs; `qemu64`, `max` and `Haswell` CPUs) |

What this does **not** show: behaviour on other architectures, on real
hardware, with a GUI/network stack, or under long-running production loads.
musl's own known gaps (e.g. no NSS, minimal locales) apply unchanged.

To reproduce the VM boot, see `tests/vm/make-image.sh`.

## Lineage and license

Kinetic is a derivative of musl, (c) 2005-2020 Rich Felker, et al., and
Kinetic's additions are (c) 2026 Kinetic contributors; both under MIT. See
`LICENSE` and `COPYRIGHT`. The original musl README is kept as `README.musl`.
Upstream baseline: musl v1.2.5, commit
`0784374d561435f7c787a555aeab8ede699ed298`.

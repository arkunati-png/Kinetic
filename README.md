# Kinetic

Kinetic is a small, fast, MIT-licensed C library for Linux: a fork of
[musl libc](https://musl.libc.org) (v1.2.5) that keeps musl's size and
simplicity and adds the glibc-isms that most often stop real software from
building or running on musl.

* **As light as musl.** The base is unmodified musl. The compatibility layer
  adds about 15 KB (1.9%) to `libc.so` and, in static builds, is only linked
  into programs that call it. `./configure --disable-compat` removes it.
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

Version 0.1.0. Developed and tested on x86_64 Linux. The compatibility layer
is architecture-neutral except `getcontext`/`setcontext`/`makecontext`/
`swapcontext`, which are implemented for x86_64 only so far. See COMPAT.md
for what is not done yet (notably NSS, full `iconv`, `fts`, `argp`).

## Lineage and license

Kinetic is a derivative of musl, (c) 2005-2020 Rich Felker, et al., and
Kinetic's additions are (c) 2026 Kinetic contributors; both under MIT. See
`LICENSE` and `COPYRIGHT`. The original musl README is kept as `README.musl`.
Upstream baseline: musl v1.2.5, commit
`0784374d561435f7c787a555aeab8ede699ed298`.

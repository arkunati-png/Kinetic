# Kinetic compatibility layer

Everything here lives in `kinetic/` (new files) plus small, clearly marked
additions to a few musl headers. Build without it using
`./configure --disable-compat`.

## What Kinetic adds over musl 1.2.5

| Area | Provided | Notes |
|------|----------|-------|
| `<sys/cdefs.h>` | `__BEGIN_DECLS`, `__THROW`, `__GNUC_PREREQ`, `__glibc_likely`, `__nonnull`, ... | Header only. Widely `#include`d by autotools-era code. |
| `<sys/queue.h>` | `SLIST`, `LIST`, `STAILQ`, `TAILQ`, `CIRCLEQ` | Header only; original implementation of the traditional interface. |
| `<error.h>` | `error`, `error_at_line`, `error_message_count`, `error_one_per_line`, `error_print_progname` | GNU coreutils-style diagnostics. |
| `<execinfo.h>` | `backtrace`, `backtrace_symbols`, `backtrace_symbols_fd` | Uses the compiler's unwinder (`_Unwind_Backtrace`) if the program links one; otherwise reports only the caller. Symbol names need a dynamically linked program (`dladdr`). |
| `<obstack.h>` | Full obstack API incl. `obstack_printf` | Used by gcc, binutils, make, m4, bison. |
| `<gnu/libc-version.h>` | `gnu_get_libc_version`, `gnu_get_libc_release` | Return `kinetic-X.Y.Z` / `kinetic`. They deliberately do **not** pretend to be glibc. |
| `<malloc.h>` | `mallinfo`, `mallinfo2`, `mallopt`, `malloc_trim`, `malloc_stats` | **Stubs.** The allocator keeps no global statistics: `mallinfo*` returns zeros, `mallopt` returns 1, `malloc_trim` returns 0. Enough for software that only logs or tunes. |
| `<ucontext.h>` | `getcontext`, `setcontext`, `makecontext`, `swapcontext` | musl declares these but does not implement them. **x86_64 only** so far. See below. |
| `<pthread.h>` | `pthread_mutex_clocklock`, `pthread_cond_clockwait`, `pthread_rwlock_clockrdlock/wrlock`, `pthread_clockjoin_np`, `pthread_yield` | glibc 2.30+ APIs. Implemented by converting the deadline between `CLOCK_REALTIME` and `CLOCK_MONOTONIC`; other clocks return `EINVAL`, as in glibc. |
| `<semaphore.h>` | `sem_clockwait` | As above. |
| `<unistd.h>` | `close_range`, `CLOSE_RANGE_*` | Needs Linux 5.9+ (5.11 for `CLOEXEC`); `ENOSYS` otherwise. |
| `<sys/pidfd.h>` | `pidfd_open`, `pidfd_getfd`, `pidfd_send_signal` | Needs Linux 5.3+ / 5.6+. |
| `<stdlib.h>` | `rpmatch`, `canonicalize_file_name` | |
| `<features.h>` | `__KINETIC__`, `__KINETIC_MAJOR__/MINOR__/PATCH__` | Feature-test macro. Kinetic does **not** define `__GLIBC__`. |

## Behavior notes and limitations

* **ucontext on other architectures.** Only x86_64 has the assembly. On other
  targets the functions remain declared-but-undefined, exactly as in musl.
  `setcontext` restores x87/SSE control state only when the context was
  produced by `getcontext`/`swapcontext` (not for a context handed to a signal
  handler, whose FP save area has a different layout).
* **`makecontext` arguments** are read as 64-bit values (as glibc does), so
  pointer arguments work although POSIX says `int`.
* **`backtrace`** does not walk frame pointers; see the table above.
* **Data symbols** (`error_message_count`, ...) are listed in `dynamic.list`
  so dynamically linked non-PIE programs share one copy with libc.

## Deliberately unchanged from musl

The dynamic loader name (`ld-musl-*`), the allocator, locale/`iconv` scope,
the DNS resolver, and the absence of NSS and `dlopen`-based plugins.
Changing these would break musl ABI compatibility or size goals; they may
become opt-in options later.

## Not done yet (roadmap)

* `fts.h`, `argp.h`, `<sys/tree.h>`
* Real `mallinfo` statistics
* `getcontext` family for aarch64 (and others)
* Fuller `iconv` charset coverage
* NSS-style user/group lookup plugins
* Fortify `__*_chk` entry points, to run binaries built against glibc
  headers with `_FORTIFY_SOURCE`

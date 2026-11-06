#define _GNU_SOURCE
#include <execinfo.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Stack unwinding is delegated to the compiler runtime (libgcc_eh /
 * libgcc_s / libunwind). They are weak references, so libc never forces
 * an unwinder on programs that do not already carry one; without one,
 * backtrace() reports just its caller instead of walking frame pointers
 * that may not exist. */
struct _Unwind_Context;
typedef int (*unwind_trace_fn)(struct _Unwind_Context *, void *);
extern int _Unwind_Backtrace(unwind_trace_fn, void *) __attribute__((__weak__));
extern unsigned long _Unwind_GetIP(struct _Unwind_Context *) __attribute__((__weak__));

struct bt_state {
	void **buf;
	int size;
	int n;
};

static int trace_cb(struct _Unwind_Context *ctx, void *arg)
{
	struct bt_state *st = arg;
	unsigned long ip = _Unwind_GetIP(ctx);
	if (!ip) return 5; /* _URC_END_OF_STACK */
	/* Skip backtrace()'s own frame. */
	if (st->n == -1) {
		st->n = 0;
		return 0;
	}
	if (st->n >= st->size) return 5;
	st->buf[st->n++] = (void *)ip;
	return st->n >= st->size ? 5 : 0;
}

int backtrace(void **buf, int size)
{
	struct bt_state st = { buf, size, -1 };
	if (size <= 0) return 0;
	if (_Unwind_Backtrace && _Unwind_GetIP) {
		_Unwind_Backtrace(trace_cb, &st);
		return st.n < 0 ? 0 : st.n;
	}
	buf[0] = __builtin_return_address(0);
	return 1;
}

static int format_one(char *out, size_t cap, void *addr)
{
	Dl_info info;
	if (dladdr(addr, &info) && info.dli_fname) {
		if (info.dli_sname) {
			unsigned long off = (char *)addr - (char *)info.dli_saddr;
			return snprintf(out, cap, "%s(%s+0x%lx) [%p]",
				info.dli_fname, info.dli_sname, off, addr);
		}
		return snprintf(out, cap, "%s(+0x%lx) [%p]", info.dli_fname,
			(unsigned long)((char *)addr - (char *)info.dli_fbase), addr);
	}
	return snprintf(out, cap, "[%p]", addr);
}

char **backtrace_symbols(void *const *buf, int size)
{
	size_t total = (size_t)size * sizeof(char *);
	char **res;
	char *p;
	int i;

	if (size <= 0) return 0;
	for (i = 0; i < size; i++) total += format_one(0, 0, buf[i]) + 1;
	res = malloc(total);
	if (!res) return 0;
	p = (char *)(res + size);
	for (i = 0; i < size; i++) {
		int n = format_one(p, total - (p - (char *)res), buf[i]);
		res[i] = p;
		p += n + 1;
	}
	return res;
}

void backtrace_symbols_fd(void *const *buf, int size, int fd)
{
	char line[512];
	int i;
	for (i = 0; i < size; i++) {
		int n = format_one(line, sizeof line - 1, buf[i]);
		if (n >= (int)sizeof line - 1) n = sizeof line - 2;
		line[n++] = '\n';
		write(fd, line, n);
	}
}

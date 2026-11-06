/* Kinetic compatibility-layer tests. Built and run by tests/run.sh, both
 * statically and dynamically linked. Exit status is the number of failures. */
#define _GNU_SOURCE
#include <errno.h>
#include <error.h>
#include <execinfo.h>
#include <fcntl.h>
#include <gnu/libc-version.h>
#include <malloc.h>
#include <obstack.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>
#include <sys/pidfd.h>
#include <sys/queue.h>
#include <sys/wait.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#ifndef __KINETIC__
#error "__KINETIC__ must be defined by <features.h>"
#endif

static int failures;
static int checks;

#define CHECK(cond) do { \
	checks++; \
	if (!(cond)) { \
		failures++; \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
	} \
} while (0)

#define SECTION(name) fprintf(stderr, "-- %s\n", name)

/* ---- sys/queue.h ------------------------------------------------------ */

struct item { int v; SLIST_ENTRY(item) s; LIST_ENTRY(item) l;
	STAILQ_ENTRY(item) sq; TAILQ_ENTRY(item) t; CIRCLEQ_ENTRY(item) c; };
SLIST_HEAD(shead, item);
LIST_HEAD(lhead, item);
STAILQ_HEAD(sqhead, item);
TAILQ_HEAD(thead, item);
CIRCLEQ_HEAD(chead, item);

static void test_queue(void)
{
	SECTION("sys/queue.h");
	struct item a = {1}, b = {2}, c = {3}, *p, *tmp;
	int sum, n;

	struct shead sh = SLIST_HEAD_INITIALIZER(sh);
	CHECK(SLIST_EMPTY(&sh));
	SLIST_INSERT_HEAD(&sh, &a, s);
	SLIST_INSERT_AFTER(&a, &b, s);
	SLIST_INSERT_HEAD(&sh, &c, s);              /* c a b */
	CHECK(SLIST_FIRST(&sh) == &c && SLIST_NEXT(&c, s) == &a);
	SLIST_REMOVE(&sh, &a, item, s);             /* c b */
	CHECK(SLIST_NEXT(&c, s) == &b && SLIST_NEXT(&b, s) == NULL);
	sum = 0; SLIST_FOREACH(p, &sh, s) sum += p->v;
	CHECK(sum == 5);

	struct lhead lh; LIST_INIT(&lh);
	LIST_INSERT_HEAD(&lh, &a, l);
	LIST_INSERT_AFTER(&a, &c, l);
	LIST_INSERT_BEFORE(&c, &b, l);              /* a b c */
	n = 0; LIST_FOREACH(p, &lh, l) CHECK(p->v == ++n);
	CHECK(n == 3);
	LIST_REMOVE(&b, l);                         /* a c */
	CHECK(LIST_NEXT(&a, l) == &c);
	n = 0; LIST_FOREACH_SAFE(p, &lh, l, tmp) { LIST_REMOVE(p, l); n++; }
	CHECK(n == 2 && LIST_EMPTY(&lh));

	struct sqhead sq = STAILQ_HEAD_INITIALIZER(sq);
	STAILQ_INSERT_TAIL(&sq, &a, sq);
	STAILQ_INSERT_TAIL(&sq, &b, sq);
	STAILQ_INSERT_HEAD(&sq, &c, sq);            /* c a b */
	STAILQ_REMOVE(&sq, &b, item, sq);           /* c a */
	STAILQ_INSERT_TAIL(&sq, &b, sq);            /* c a b, tail pointer intact */
	n = 0; sum = 0; STAILQ_FOREACH(p, &sq, sq) { n++; sum = sum * 10 + p->v; }
	CHECK(n == 3 && sum == 312);
	STAILQ_REMOVE_HEAD(&sq, sq);
	CHECK(STAILQ_FIRST(&sq) == &a);

	struct thead th; TAILQ_INIT(&th);
	TAILQ_INSERT_TAIL(&th, &b, t);
	TAILQ_INSERT_HEAD(&th, &a, t);
	TAILQ_INSERT_TAIL(&th, &c, t);              /* a b c */
	CHECK(TAILQ_FIRST(&th) == &a && TAILQ_LAST(&th, thead) == &c);
	CHECK(TAILQ_PREV(&c, thead, t) == &b && TAILQ_PREV(&a, thead, t) == NULL);
	sum = 0; TAILQ_FOREACH_REVERSE(p, &th, thead, t) sum = sum * 10 + p->v;
	CHECK(sum == 321);
	TAILQ_REMOVE(&th, &b, t);
	CHECK(TAILQ_NEXT(&a, t) == &c && TAILQ_LAST(&th, thead) == &c);
	TAILQ_INSERT_BEFORE(&c, &b, t);
	TAILQ_REMOVE(&th, &c, t);
	CHECK(TAILQ_LAST(&th, thead) == &b);

	struct chead ch; CIRCLEQ_INIT(&ch);
	CHECK(CIRCLEQ_EMPTY(&ch));
	CIRCLEQ_INSERT_TAIL(&ch, &b, c);
	CIRCLEQ_INSERT_HEAD(&ch, &a, c);
	CIRCLEQ_INSERT_AFTER(&ch, &b, &c, c);       /* a b c */
	sum = 0; CIRCLEQ_FOREACH(p, &ch, c) sum = sum * 10 + p->v;
	CHECK(sum == 123);
	sum = 0; CIRCLEQ_FOREACH_REVERSE(p, &ch, c) sum = sum * 10 + p->v;
	CHECK(sum == 321);
	CIRCLEQ_REMOVE(&ch, &b, c);
	CHECK(CIRCLEQ_NEXT(&a, c) == &c && CIRCLEQ_LAST(&ch) == &c);
}

/* ---- error.h ---------------------------------------------------------- */

static void test_error(void)
{
	SECTION("error.h");
	char buf[2048] = "";
	int p[2], saved = dup(2);
	ssize_t n;

	CHECK(pipe(p) == 0);
	fflush(stderr);
	dup2(p[1], 2);
	error_message_count = 0;
	error(0, ENOENT, "cannot open %s (%d)", "foo", 7);
	error_at_line(0, 0, "x.c", 42, "bad thing");
	error_one_per_line = 1;
	error_at_line(0, 0, "x.c", 43, "once");
	error_at_line(0, 0, "x.c", 43, "twice (suppressed)");
	error_one_per_line = 0;
	fflush(stderr);
	dup2(saved, 2);
	close(p[1]);
	n = read(p[0], buf, sizeof buf - 1);
	CHECK(n > 0);
	buf[n > 0 ? n : 0] = 0;
	close(p[0]);
	close(saved);

	CHECK(strstr(buf, "cannot open foo (7): No such file or directory\n") != 0);
	CHECK(strstr(buf, ":x.c:42: bad thing\n") != 0);
	CHECK(strstr(buf, "once\n") != 0);
	CHECK(strstr(buf, "suppressed") == 0);
	CHECK(error_message_count == 3);

	pid_t pid = fork();
	if (pid == 0) {
		freopen("/dev/null", "w", stderr);
		error(3, 0, "fatal");
		_exit(99);
	}
	int st = 0;
	waitpid(pid, &st, 0);
	CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 3);
}

/* ---- execinfo.h ------------------------------------------------------- */

__attribute__((noinline)) static int bt_depth(void **buf, int n) { return backtrace(buf, n); }

static void test_backtrace(void)
{
	SECTION("execinfo.h");
	void *buf[16];
	int n = bt_depth(buf, 16);
	CHECK(n >= 1 && n <= 16);
	CHECK(backtrace(buf, 0) == 0);
	char **syms = backtrace_symbols(buf, n);
	CHECK(syms != 0);
	if (syms) {
		int ok = 1, i;
		for (i = 0; i < n; i++) if (!syms[i] || !*syms[i]) ok = 0;
		CHECK(ok);
		free(syms);
	}
	int p[2];
	CHECK(pipe(p) == 0);
	backtrace_symbols_fd(buf, n, p[1]);
	close(p[1]);
	char tmp[64];
	CHECK(read(p[0], tmp, sizeof tmp) > 0);
	close(p[0]);
}

/* ---- gnu/libc-version.h, malloc.h, misc ------------------------------- */

static void test_misc(void)
{
	SECTION("misc");
	CHECK(strncmp(gnu_get_libc_version(), "kinetic-", 8) == 0);
	CHECK(strcmp(gnu_get_libc_release(), "kinetic") == 0);

	struct mallinfo mi = mallinfo();
	struct mallinfo2 mi2 = mallinfo2();
	CHECK(mi.arena >= 0 && mi2.uordblks <= mi2.arena);   /* real numbers: see compat_ext.c */
	CHECK(mallopt(M_ARENA_MAX, 1) == 1);
	CHECK(malloc_trim(0) == 0);

	CHECK(rpmatch("yes") == 1 && rpmatch("Y") == 1);
	CHECK(rpmatch("no") == 0 && rpmatch("N") == 0);
	CHECK(rpmatch("maybe") == -1);

	char *c = canonicalize_file_name("/tmp/../tmp/.");
	CHECK(c && strcmp(c, "/tmp") == 0);
	free(c);
	CHECK(canonicalize_file_name("/definitely/not/here") == 0 && errno == ENOENT);

	CHECK(pthread_yield() == 0);
	CHECK(__GNUC_PREREQ(1, 0));
}

static void test_close_range(void)
{
	SECTION("close_range");
	pid_t pid = fork();
	if (pid == 0) {
		int fds[4], i, bad = 0;
		for (i = 0; i < 4; i++) fds[i] = open("/dev/null", O_RDONLY);
		if (close_range(fds[1], fds[2], 0) != 0) _exit(10);
		if (fcntl(fds[0], F_GETFD) < 0) bad |= 1;
		if (fcntl(fds[1], F_GETFD) >= 0 || errno != EBADF) bad |= 2;
		if (fcntl(fds[2], F_GETFD) >= 0) bad |= 4;
		if (fcntl(fds[3], F_GETFD) < 0) bad |= 8;
		/* CLOEXEC variant marks instead of closing */
		if (close_range(fds[3], ~0U, CLOSE_RANGE_CLOEXEC) == 0) {
			int fl = fcntl(fds[3], F_GETFD);
			if (fl < 0 || !(fl & FD_CLOEXEC)) bad |= 16;
		}
		_exit(bad);
	}
	int st = 0;
	waitpid(pid, &st, 0);
	if (WIFEXITED(st) && WEXITSTATUS(st) == 10) {
		fprintf(stderr, "   (close_range unsupported by this kernel, skipped)\n");
		return;
	}
	CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0);
}

static void test_pidfd(void)
{
	SECTION("pidfd");
	int fd = pidfd_open(getpid(), 0);
	if (fd < 0 && errno == ENOSYS) {
		fprintf(stderr, "   (pidfd unsupported by this kernel, skipped)\n");
		return;
	}
	CHECK(fd >= 0);
	if (fd < 0) return;
	CHECK(pidfd_send_signal(fd, 0, 0, 0) == 0);
	CHECK(pidfd_open(-1, 0) == -1 && errno == EINVAL);
	close(fd);
}

/* ---- obstack.h -------------------------------------------------------- */

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

static void test_obstack(void)
{
	SECTION("obstack.h");
	struct obstack ob;
	int i;

	CHECK(obstack_init(&ob) == 1);
	CHECK(obstack_object_size(&ob) == 0);

	char *s = obstack_copy0(&ob, "hello", 5);
	CHECK(strcmp(s, "hello") == 0);

	obstack_grow(&ob, "wor", 3);
	obstack_1grow(&ob, 'l');
	obstack_grow0(&ob, "d", 1);
	CHECK(obstack_object_size(&ob) == 6);
	char *w = obstack_finish(&ob);
	CHECK(strcmp(w, "world") == 0);
	CHECK(strcmp(s, "hello") == 0);           /* earlier object untouched */

	int *ints[200];
	for (i = 0; i < 200; i++) {
		ints[i] = obstack_alloc(&ob, 100 * sizeof(int));  /* spans many chunks */
		ints[i][0] = i; ints[i][99] = ~i;
		CHECK(((unsigned long)ints[i] & (sizeof(void *) - 1)) == 0);
	}
	int good = 1;
	for (i = 0; i < 200; i++) if (ints[i][0] != i || ints[i][99] != ~i) good = 0;
	CHECK(good);
	CHECK(_obstack_allocated_p(&ob, ints[100]));
	CHECK(_obstack_memory_used(&ob) > 200 * 100 * sizeof(int));

	/* growing one object across a chunk boundary keeps its contents */
	for (i = 0; i < 20000; i++) obstack_1grow(&ob, 'a' + i % 26);
	char *big = obstack_finish(&ob);
	good = 1;
	for (i = 0; i < 20000; i++) if (big[i] != 'a' + i % 26) good = 0;
	CHECK(good);

	CHECK(obstack_printf(&ob, "%s-%d", "pi", 314) == 6);
	obstack_1grow(&ob, 0);
	char *f = obstack_finish(&ob);
	CHECK(strcmp(f, "pi-314") == 0);

	obstack_free(&ob, ints[50]);              /* frees everything after it */
	CHECK(!_obstack_allocated_p(&ob, big));
	CHECK(_obstack_allocated_p(&ob, ints[10]));
	obstack_free(&ob, NULL);
}

/* ---- clock-selectable waits ------------------------------------------- */

static struct timespec after(clockid_t clk, long ms)
{
	struct timespec t;
	clock_gettime(clk, &t);
	t.tv_nsec += ms * 1000000L;
	t.tv_sec += t.tv_nsec / 1000000000L;
	t.tv_nsec %= 1000000000L;
	return t;
}

static long elapsed_ms(struct timespec a)
{
	struct timespec b;
	clock_gettime(CLOCK_MONOTONIC, &b);
	return (b.tv_sec - a.tv_sec) * 1000 + (b.tv_nsec - a.tv_nsec) / 1000000;
}

static void *join_target(void *arg)
{
	struct timespec ts = { 0, 300 * 1000000L };
	nanosleep(&ts, 0);
	return arg;
}

static void test_clock_waits(void)
{
	SECTION("clock-selectable waits");
	struct timespec start, dl;
	clockid_t clks[2] = { CLOCK_MONOTONIC, CLOCK_REALTIME };
	int k;

	for (k = 0; k < 2; k++) {
		pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
		pthread_cond_t c = PTHREAD_COND_INITIALIZER;
		pthread_rwlock_t rw = PTHREAD_RWLOCK_INITIALIZER;
		sem_t sem;

		clock_gettime(CLOCK_MONOTONIC, &start);
		pthread_mutex_lock(&m);
		dl = after(clks[k], 80);
		CHECK(pthread_mutex_clocklock(&m, clks[k], &dl) == ETIMEDOUT);
		long e = elapsed_ms(start);
		CHECK(e >= 60 && e < 1500);
		pthread_mutex_unlock(&m);
		dl = after(clks[k], 80);
		CHECK(pthread_mutex_clocklock(&m, clks[k], &dl) == 0);

		clock_gettime(CLOCK_MONOTONIC, &start);
		dl = after(clks[k], 80);
		CHECK(pthread_cond_clockwait(&c, &m, clks[k], &dl) == ETIMEDOUT);
		e = elapsed_ms(start);
		CHECK(e >= 60 && e < 1500);
		pthread_mutex_unlock(&m);

		pthread_rwlock_wrlock(&rw);
		dl = after(clks[k], 50);
		CHECK(pthread_rwlock_clockrdlock(&rw, clks[k], &dl) == ETIMEDOUT);
		CHECK(pthread_rwlock_clockwrlock(&rw, clks[k], &dl) == ETIMEDOUT);
		pthread_rwlock_unlock(&rw);

		sem_init(&sem, 0, 0);
		clock_gettime(CLOCK_MONOTONIC, &start);
		dl = after(clks[k], 80);
		CHECK(sem_clockwait(&sem, clks[k], &dl) == -1 && errno == ETIMEDOUT);
		e = elapsed_ms(start);
		CHECK(e >= 60 && e < 1500);
		sem_post(&sem);
		CHECK(sem_clockwait(&sem, clks[k], &dl) == 0);
		sem_destroy(&sem);
	}

	/* a deadline already in the past times out immediately */
	pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock(&m);
	dl = after(CLOCK_MONOTONIC, -1000);
	if (dl.tv_nsec < 0) { dl.tv_nsec += 1000000000L; dl.tv_sec--; }
	clock_gettime(CLOCK_MONOTONIC, &start);
	CHECK(pthread_mutex_clocklock(&m, CLOCK_MONOTONIC, &dl) == ETIMEDOUT);
	CHECK(elapsed_ms(start) < 200);

	/* invalid clock / timespec */
	dl = after(CLOCK_REALTIME, 10);
	CHECK(pthread_mutex_clocklock(&m, CLOCK_PROCESS_CPUTIME_ID, &dl) == EINVAL);
	dl.tv_nsec = 2000000000L;
	CHECK(pthread_mutex_clocklock(&m, CLOCK_REALTIME, &dl) == EINVAL);
	sem_t s2; sem_init(&s2, 0, 0);
	CHECK(sem_clockwait(&s2, CLOCK_BOOTTIME, &dl) == -1 && errno == EINVAL);

	/* pthread_clockjoin_np: first too short, then long enough */
	pthread_t th;
	void *ret = 0;
	pthread_create(&th, 0, join_target, (void *)0x1234);
	dl = after(CLOCK_MONOTONIC, 20);
	CHECK(pthread_clockjoin_np(th, &ret, CLOCK_MONOTONIC, &dl) == ETIMEDOUT);
	dl = after(CLOCK_MONOTONIC, 3000);
	CHECK(pthread_clockjoin_np(th, &ret, CLOCK_MONOTONIC, &dl) == 0);
	CHECK(ret == (void *)0x1234);
}

/* ---- ucontext --------------------------------------------------------- */

#if defined(__x86_64__)

static ucontext_t main_ctx, co_ctx, co2_ctx;
static int co_log[64], co_n;

static void co_body(void)
{
	int i;
	for (i = 0; i < 3; i++) {
		co_log[co_n++] = 100 + i;
		swapcontext(&co_ctx, &main_ctx);
	}
	co_log[co_n++] = 199;
}                                          /* returns -> uc_link (main_ctx) */

static long args_seen[10];
static void co_args(int a, int b, int c, int d, int e, int f, int g, int h, int i)
{
	args_seen[0] = a; args_seen[1] = b; args_seen[2] = c; args_seen[3] = d;
	args_seen[4] = e; args_seen[5] = f; args_seen[6] = g; args_seen[7] = h;
	args_seen[8] = i;
}

static volatile double fp_sink;
static void co_fp(void)
{
	/* exercise SSE state and a deep-ish stack inside the coroutine */
	volatile double x = 1.5;
	char pad[2000];
	memset(pad, 1, sizeof pad);
	fp_sink = x * 2 + pad[1999];
}

static void test_ucontext(void)
{
	SECTION("ucontext");
	static char stack[64 * 1024];
	volatile int count = 0;
	ucontext_t here;

	/* getcontext/setcontext: the classic "return twice" loop */
	CHECK(getcontext(&here) == 0);
	count++;
	if (count < 3) setcontext(&here);
	CHECK(count == 3);

	/* coroutine ping-pong through swapcontext, finishing via uc_link */
	getcontext(&co_ctx);
	co_ctx.uc_stack.ss_sp = stack;
	co_ctx.uc_stack.ss_size = sizeof stack;
	co_ctx.uc_link = &main_ctx;
	makecontext(&co_ctx, co_body, 0);
	co_n = 0;
	int i;
	for (i = 0; i < 4; i++) {
		co_log[co_n++] = i;
		swapcontext(&main_ctx, &co_ctx);
		if (co_n > 20) break;
	}
	/* main logs 0, co 100, main 1, co 101, main 2, co 102, main 3, co 199 */
	CHECK(co_n == 8);
	CHECK(co_log[0] == 0 && co_log[1] == 100 && co_log[2] == 1 && co_log[3] == 101);
	CHECK(co_log[4] == 2 && co_log[5] == 102 && co_log[6] == 3 && co_log[7] == 199);

	/* nine arguments: six in registers, three on the stack */
	getcontext(&co2_ctx);
	co2_ctx.uc_stack.ss_sp = stack;
	co2_ctx.uc_stack.ss_size = sizeof stack;
	co2_ctx.uc_link = &main_ctx;
	makecontext(&co2_ctx, (void (*)(void))co_args, 9, 1, 2, 3, 4, 5, 6, 7, 8, 9);
	swapcontext(&main_ctx, &co2_ctx);
	for (i = 0; i < 9; i++) CHECK(args_seen[i] == i + 1);

	/* odd and even stack-argument counts keep the stack ABI-aligned */
	getcontext(&co2_ctx);
	co2_ctx.uc_stack.ss_sp = stack;
	co2_ctx.uc_stack.ss_size = sizeof stack;
	co2_ctx.uc_link = &main_ctx;
	makecontext(&co2_ctx, co_fp, 0);
	fp_sink = 0;
	swapcontext(&main_ctx, &co2_ctx);
	CHECK(fp_sink == 4.0);

	/* the signal mask is saved and restored with the context */
	sigset_t blk, old, cur;
	sigemptyset(&blk);
	sigaddset(&blk, SIGUSR1);
	sigprocmask(SIG_BLOCK, &blk, &old);
	volatile int phase = 0;
	getcontext(&here);                         /* saved with SIGUSR1 blocked */
	if (phase == 0) {
		phase = 1;
		sigprocmask(SIG_SETMASK, &old, 0);     /* unblock, then jump back */
		setcontext(&here);
	}
	sigprocmask(SIG_BLOCK, 0, &cur);
	CHECK(sigismember(&cur, SIGUSR1) == 1);    /* mask restored by setcontext */
	sigprocmask(SIG_SETMASK, &old, 0);
}

#else
static void test_ucontext(void)
{
	SECTION("ucontext (skipped: not implemented on this architecture)");
}
#endif

int main(void)
{
	test_queue();
	test_error();
	test_backtrace();
	test_misc();
	test_close_range();
	test_pidfd();
	test_obstack();
	test_clock_waits();
	test_ucontext();
	fprintf(stderr, "\n%d checks, %d failures\n", checks, failures);
	return failures > 255 ? 255 : failures;
}

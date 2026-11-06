/* Kinetic stress test: concurrency and allocator integrity under load.
 * Usage: stress [seconds]   Exit status 0 = no corruption detected.
 *
 * - N threads allocate, fill with a checksum pattern, realloc, and free
 *   blocks of mixed sizes (including large mmap-backed ones), verifying the
 *   pattern on every release. Any corruption aborts with a message.
 * - A mutex/condvar ping-pong checks wakeups are neither lost nor doubled.
 * - Repeated fork()+pipe exercises process creation under thread load.
 * - A SIGALRM-driven counter checks async signal delivery.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <malloc.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define NTHREADS 8
#define SLOTS 96

static volatile int stop;
static volatile int corrupt;
static unsigned long long total_allocs[NTHREADS];

static uint32_t rng(uint32_t *s)
{
	*s ^= *s << 13;
	*s ^= *s >> 17;
	*s ^= *s << 5;
	return *s;
}

struct blk {
	unsigned char *p;
	size_t n;
	uint32_t seed;
};

static void fill(struct blk *b)
{
	uint32_t s = b->seed ? b->seed : 1;
	size_t i;
	for (i = 0; i < b->n; i++) b->p[i] = (unsigned char)rng(&s);
}

static int check(const struct blk *b)
{
	uint32_t s = b->seed ? b->seed : 1;
	size_t i;
	for (i = 0; i < b->n; i++)
		if (b->p[i] != (unsigned char)rng(&s)) return 0;
	return 1;
}

static size_t pick_size(uint32_t *s)
{
	uint32_t r = rng(s) % 100;
	if (r < 70) return 1 + rng(s) % 256;
	if (r < 95) return 1 + rng(s) % 8192;
	if (r < 99) return 1 + rng(s) % 100000;
	return 131072 + rng(s) % 300000;           /* beyond the mmap threshold */
}

static void *worker(void *arg)
{
	long id = (long)arg;
	uint32_t s = 0x9e3779b9u * (id + 1);
	struct blk slots[SLOTS];
	memset(slots, 0, sizeof slots);

	while (!stop && !corrupt) {
		struct blk *b = &slots[rng(&s) % SLOTS];
		if (b->p) {
			if (!check(b)) {
				fprintf(stderr, "CORRUPTION thread %ld size %zu\n", id, b->n);
				corrupt = 1;
				break;
			}
			if (rng(&s) % 4 == 0) {            /* grow/shrink in place of a free */
				size_t nn = pick_size(&s);
				unsigned char *np = realloc(b->p, nn);
				if (!np) { fprintf(stderr, "realloc failed\n"); corrupt = 1; break; }
				/* the common prefix must have survived */
				struct blk old = { np, b->n < nn ? b->n : nn, b->seed };
				if (!check(&old)) {
					/* prefix of a pattern is the same stream, so this must hold */
					fprintf(stderr, "CORRUPTION after realloc thread %ld\n", id);
					corrupt = 1;
					break;
				}
				b->p = np;
				b->n = nn;
				b->seed = rng(&s);
				fill(b);
			} else {
				free(b->p);
				b->p = 0;
			}
		} else {
			b->n = pick_size(&s);
			b->p = malloc(b->n);
			if (!b->p) { fprintf(stderr, "malloc(%zu) failed\n", b->n); corrupt = 1; break; }
			b->seed = rng(&s);
			fill(b);
			total_allocs[id]++;
		}
	}
	for (int i = 0; i < SLOTS; i++) {
		if (slots[i].p && !check(&slots[i])) corrupt = 1;
		free(slots[i].p);
	}
	return 0;
}

/* ---- condvar ping-pong ------------------------------------------------- */
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static int turn;                               /* 0: ping's turn, 1: pong's */
static long rounds_done;

static void *pinger(void *arg)
{
	int me = (int)(long)arg;
	long i;
	for (i = 0; i < 20000; i++) {
		pthread_mutex_lock(&mu);
		while (turn != me) pthread_cond_wait(&cv, &mu);
		if (me == 0) rounds_done++;
		turn = !me;
		pthread_cond_broadcast(&cv);
		pthread_mutex_unlock(&mu);
	}
	return 0;
}

/* ---- signals ----------------------------------------------------------- */
static volatile sig_atomic_t ticks;
static void on_alarm(int sig) { (void)sig; ticks++; }

int main(int argc, char **argv)
{
	int secs = argc > 1 ? atoi(argv[1]) : 5;
	pthread_t th[NTHREADS], pp[2];
	long i;
	unsigned long long total = 0;
	int failed = 0;

	/* signals: ~100 SIGALRMs while everything else runs */
	struct sigaction sa;
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_alarm;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGALRM, &sa, 0);
	struct itimerval it = { { 0, 20000 }, { 0, 20000 } };
	setitimer(ITIMER_REAL, &it, 0);

	for (i = 0; i < NTHREADS; i++) pthread_create(&th[i], 0, worker, (void *)i);
	pthread_create(&pp[0], 0, pinger, (void *)0);
	pthread_create(&pp[1], 0, pinger, (void *)1);

	/* fork storm from the main thread while workers allocate */
	time_t end = time(0) + secs;
	int forks = 0, fork_bad = 0;
	while (time(0) < end && !corrupt) {
		int p[2];
		if (pipe(p)) break;
		pid_t pid = fork();
		if (pid == 0) {
			char *m = malloc(1000 + forks);
			memset(m, 0x5a, 1000 + forks);
			char c = m[999];
			free(m);
			write(p[1], &c, 1);
			_exit(c == 0x5a ? 0 : 3);
		}
		close(p[1]);
		char c = 0;
		ssize_t n = read(p[0], &c, 1);
		close(p[0]);
		int st = 0;
		waitpid(pid, &st, 0);
		if (n != 1 || c != 0x5a || !WIFEXITED(st) || WEXITSTATUS(st) != 0) fork_bad++;
		forks++;
		struct timespec ts = { 0, 2000000 };
		nanosleep(&ts, 0);
	}
	stop = 1;

	for (i = 0; i < NTHREADS; i++) pthread_join(th[i], 0);
	pthread_join(pp[0], 0);
	pthread_join(pp[1], 0);
	struct itimerval off = { { 0, 0 }, { 0, 0 } };
	setitimer(ITIMER_REAL, &off, 0);

	for (i = 0; i < NTHREADS; i++) total += total_allocs[i];
	printf("allocations: %llu across %d threads\n", total, NTHREADS);
	printf("ping-pong rounds: %ld (expect 20000)\n", rounds_done);
	printf("forks: %d, bad children: %d\n", forks, fork_bad);
	printf("SIGALRM ticks: %d\n", (int)ticks);
	{
		struct mallinfo2 mi = mallinfo2();
		printf("mallinfo2 after run: arena=%zu in-use=%zu\n", mi.arena, mi.uordblks);
	}

	if (corrupt) { printf("FAIL: memory corruption detected\n"); failed = 1; }
	if (rounds_done != 20000) { printf("FAIL: lost/duplicated condvar wakeups\n"); failed = 1; }
	if (fork_bad) { printf("FAIL: fork children misbehaved\n"); failed = 1; }
	if (ticks < 10) { printf("FAIL: too few signals delivered\n"); failed = 1; }
	if (total < 1000) { printf("FAIL: suspiciously few allocations\n"); failed = 1; }
	printf(failed ? "STRESS: FAIL\n" : "STRESS: PASS\n");
	return failed;
}

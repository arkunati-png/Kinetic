#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include "pthread_impl.h"
#include "../internal/clockconv.h"

int pthread_mutex_clocklock(pthread_mutex_t *restrict m, clockid_t clk,
	const struct timespec *restrict at)
{
	struct timespec t;
	int r = __kinetic_clock_convert(clk, CLOCK_REALTIME, at, &t);
	return r ? r : pthread_mutex_timedlock(m, &t);
}

int pthread_cond_clockwait(pthread_cond_t *restrict c, pthread_mutex_t *restrict m,
	clockid_t clk, const struct timespec *restrict at)
{
	struct timespec t;
	int r = __kinetic_clock_convert(clk, c->_c_clock, at, &t);
	return r ? r : pthread_cond_timedwait(c, m, &t);
}

int pthread_rwlock_clockrdlock(pthread_rwlock_t *restrict rw, clockid_t clk,
	const struct timespec *restrict at)
{
	struct timespec t;
	int r = __kinetic_clock_convert(clk, CLOCK_REALTIME, at, &t);
	return r ? r : pthread_rwlock_timedrdlock(rw, &t);
}

int pthread_rwlock_clockwrlock(pthread_rwlock_t *restrict rw, clockid_t clk,
	const struct timespec *restrict at)
{
	struct timespec t;
	int r = __kinetic_clock_convert(clk, CLOCK_REALTIME, at, &t);
	return r ? r : pthread_rwlock_timedwrlock(rw, &t);
}

int pthread_clockjoin_np(pthread_t th, void **res, clockid_t clk,
	const struct timespec *at)
{
	struct timespec t;
	int r = __kinetic_clock_convert(clk, CLOCK_REALTIME, at, &t);
	return r ? r : pthread_timedjoin_np(th, res, &t);
}

int pthread_yield(void)
{
	return sched_yield();
}

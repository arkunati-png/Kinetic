#ifndef KINETIC_CLOCKCONV_H
#define KINETIC_CLOCKCONV_H

#include <errno.h>
#include <time.h>

/* Re-express an absolute deadline measured on clock `from` as an absolute
 * deadline on clock `to`, so the clock-selectable glibc waits (clocklock,
 * clockwait, ...) can sit on top of musl's single-clock primitives.
 * Only CLOCK_REALTIME and CLOCK_MONOTONIC are valid, as in glibc. */
static inline int __kinetic_clock_convert(clockid_t from, clockid_t to,
	const struct timespec *abs, struct timespec *out)
{
	struct timespec a, b;
	time_t sec;
	long nsec;

	if (abs->tv_nsec < 0 || abs->tv_nsec >= 1000000000L) return EINVAL;
	if (from != CLOCK_REALTIME && from != CLOCK_MONOTONIC) return EINVAL;
	if (to != CLOCK_REALTIME && to != CLOCK_MONOTONIC) return EINVAL;
	if (from == to) {
		*out = *abs;
		return 0;
	}

	clock_gettime(from, &a);
	clock_gettime(to, &b);
	sec = abs->tv_sec - a.tv_sec;
	nsec = abs->tv_nsec - a.tv_nsec;
	if (nsec < 0) {
		nsec += 1000000000L;
		sec--;
	}
	if (sec < 0) sec = nsec = 0; /* deadline already passed */

	/* Saturate rather than overflow for absurdly distant deadlines. */
	if (sec > (time_t)(((unsigned long long)-1 >> 2) - (unsigned long long)b.tv_sec))
		sec = (time_t)((unsigned long long)-1 >> 2) - b.tv_sec;

	out->tv_sec = b.tv_sec + sec;
	out->tv_nsec = b.tv_nsec + nsec;
	if (out->tv_nsec >= 1000000000L) {
		out->tv_nsec -= 1000000000L;
		out->tv_sec++;
	}
	return 0;
}

#endif

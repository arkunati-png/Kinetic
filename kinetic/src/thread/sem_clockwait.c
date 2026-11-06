#define _GNU_SOURCE
#include <semaphore.h>
#include "../internal/clockconv.h"

int sem_clockwait(sem_t *restrict sem, clockid_t clk, const struct timespec *restrict at)
{
	struct timespec t;
	int r = __kinetic_clock_convert(clk, CLOCK_REALTIME, at, &t);
	if (r) {
		errno = r;
		return -1;
	}
	return sem_timedwait(sem, &t);
}

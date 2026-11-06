#define _GNU_SOURCE
#include <malloc.h>

/* Tuning knobs are accepted and ignored; the allocator tunes itself. */

int mallopt(int param, int value)
{
	(void)param; (void)value;
	return 1;
}

/* The allocator returns memory to the system as groups empty out, so there
 * is never a trimmable top-of-heap region. 0 means "nothing released". */
int malloc_trim(size_t pad)
{
	(void)pad;
	return 0;
}

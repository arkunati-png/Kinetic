#define _GNU_SOURCE
#include <malloc.h>
#include <string.h>

/* The allocator keeps no global statistics: report zeros. */

struct mallinfo2 mallinfo2(void)
{
	struct mallinfo2 mi;
	memset(&mi, 0, sizeof mi);
	return mi;
}

struct mallinfo mallinfo(void)
{
	struct mallinfo mi;
	memset(&mi, 0, sizeof mi);
	return mi;
}

int mallopt(int param, int value)
{
	(void)param; (void)value;
	return 1;
}

int malloc_trim(size_t pad)
{
	(void)pad;
	return 0;
}

void malloc_stats(void)
{
}

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

int close_range(unsigned int first, unsigned int last, int flags)
{
	return syscall(SYS_close_range, first, last, flags);
}

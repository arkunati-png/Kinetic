#define _GNU_SOURCE
#include <sys/pidfd.h>
#include <unistd.h>
#include <sys/syscall.h>

int pidfd_open(pid_t pid, unsigned int flags)
{
	return syscall(SYS_pidfd_open, pid, flags);
}

int pidfd_getfd(int pidfd, int targetfd, unsigned int flags)
{
	return syscall(SYS_pidfd_getfd, pidfd, targetfd, flags);
}

int pidfd_send_signal(int pidfd, int sig, siginfo_t *info, unsigned int flags)
{
	return syscall(SYS_pidfd_send_signal, pidfd, sig, info, flags);
}

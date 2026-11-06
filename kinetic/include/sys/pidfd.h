#ifndef _SYS_PIDFD_H
#define _SYS_PIDFD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <features.h>
#include <signal.h>

#define __NEED_pid_t
#include <bits/alltypes.h>

int pidfd_open(pid_t, unsigned int);
int pidfd_getfd(int, int, unsigned int);
int pidfd_send_signal(int, int, siginfo_t *, unsigned int);

#ifdef __cplusplus
}
#endif

#endif

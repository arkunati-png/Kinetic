/* Helpers shared by the per-architecture makecontext.c files. Included
 * (not compiled on its own) so that architectures without an assembly
 * implementation build nothing at all. */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <ucontext.h>
#include "syscall.h"
#include "libc.h"

#define KN_CHECK(name, cond) typedef char kn_check_##name[(cond) ? 1 : -1]

hidden _Noreturn void __kinetic_context_return(ucontext_t *link)
{
	if (link) setcontext(link);
	exit(0);
}

/* Shared error exit for the assembly: translate a raw -errno. */
hidden int __kinetic_ctx_ret(long r)
{
	return __syscall_ret(r);
}

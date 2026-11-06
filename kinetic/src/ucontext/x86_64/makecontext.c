#define _GNU_SOURCE
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <ucontext.h>
#include "syscall.h"
#include "libc.h"

/* The assembly in this directory hardcodes these ucontext_t offsets. */
#define KN_CHECK(name, cond) typedef char kn_check_##name[(cond) ? 1 : -1]
KN_CHECK(gregs, offsetof(ucontext_t, uc_mcontext.gregs) == 40);
KN_CHECK(fpregs, offsetof(ucontext_t, uc_mcontext.fpregs) == 224);
KN_CHECK(sigmask, offsetof(ucontext_t, uc_sigmask) == 296);
KN_CHECK(fpmem, offsetof(ucontext_t, __fpregs_mem) == 424);

extern void __kinetic_start_context(void) hidden;

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

void makecontext(ucontext_t *uc, void (*fn)(void), int argc, ...)
{
	greg_t *g = uc->uc_mcontext.gregs;
	static const int regs[6] = { REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9 };
	uintptr_t top = ((uintptr_t)uc->uc_stack.ss_sp + uc->uc_stack.ss_size) & ~(uintptr_t)15;
	uintptr_t link_slot = top - 8;
	uintptr_t sp = link_slot - 8 * (argc > 6 ? argc - 6 : 0) - 8;
	uint64_t *stack_args;
	va_list ap;
	int i;

	if ((sp & 15) != 8) sp -= 8;
	stack_args = (uint64_t *)(sp + 8);

	*(uintptr_t *)link_slot = (uintptr_t)uc->uc_link;
	*(uintptr_t *)sp = (uintptr_t)__kinetic_start_context;

	g[REG_RIP] = (greg_t)(uintptr_t)fn;
	g[REG_RSP] = (greg_t)sp;
	g[REG_RBX] = (greg_t)link_slot;

	/* Arguments are read as 64-bit values, as glibc does, so that
	 * pointer arguments survive. */
	va_start(ap, argc);
	for (i = 0; i < argc; i++) {
		greg_t v = va_arg(ap, greg_t);
		if (i < 6) g[regs[i]] = v;
		else stack_args[i - 6] = (uint64_t)v;
	}
	va_end(ap);
}

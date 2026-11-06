#define _GNU_SOURCE
#include <stdarg.h>
#include "../ctxcommon.h"

/* The assembly in this directory hardcodes these ucontext_t offsets. */
KN_CHECK(sigmask, offsetof(ucontext_t, uc_sigmask) == 40);
KN_CHECK(regs, offsetof(ucontext_t, uc_mcontext.regs) == 184);
KN_CHECK(sp, offsetof(ucontext_t, uc_mcontext.sp) == 432);
KN_CHECK(pc, offsetof(ucontext_t, uc_mcontext.pc) == 440);
KN_CHECK(resv, offsetof(ucontext_t, uc_mcontext.__reserved) == 464);

extern void __kinetic_start_context(void) hidden;

void makecontext(ucontext_t *uc, void (*fn)(void), int argc, ...)
{
	unsigned long *r = uc->uc_mcontext.regs;
	uintptr_t top = ((uintptr_t)uc->uc_stack.ss_sp + uc->uc_stack.ss_size) & ~(uintptr_t)15;
	int nstack = argc > 8 ? argc - 8 : 0;
	uintptr_t sp = (top - 8 * nstack) & ~(uintptr_t)15;
	uint64_t *stack_args = (uint64_t *)sp;
	va_list ap;
	int i;

	r[19] = (unsigned long)uc->uc_link;
	r[29] = 0; /* end of the frame-pointer chain for unwinders */
	r[30] = (unsigned long)__kinetic_start_context;
	uc->uc_mcontext.sp = sp;
	uc->uc_mcontext.pc = (unsigned long)fn;

	/* The first eight arguments go in x0-x7, the rest on the stack. They
	 * are read as 64-bit values so pointer arguments survive. */
	va_start(ap, argc);
	for (i = 0; i < argc; i++) {
		unsigned long long v = va_arg(ap, unsigned long long);
		if (i < 8) r[i] = v;
		else stack_args[i - 8] = v;
	}
	va_end(ap);
}

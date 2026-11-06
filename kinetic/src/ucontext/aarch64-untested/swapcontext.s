/* int swapcontext(ucontext_t *oucp, const ucontext_t *ucp)
 * Save into oucp (as getcontext does), then switch to ucp (as setcontext
 * does) using one rt_sigprocmask that stores the old mask and installs the
 * new one. */
.text
.global swapcontext
.type swapcontext,%function
swapcontext:
	mov x9, x1
	stp x19, x20, [x0, #336]
	stp x21, x22, [x0, #352]
	stp x23, x24, [x0, #368]
	stp x25, x26, [x0, #384]
	stp x27, x28, [x0, #400]
	stp x29, x30, [x0, #416]
	str xzr, [x0, #184]
	mov x2, sp
	str x2, [x0, #432]
	str x30, [x0, #440]

	add x2, x0, #464
	movz w3, #0x8001
	movk w3, #0x4650, lsl #16
	mov w4, #528
	stp w3, w4, [x2]
	mrs x3, fpsr
	mrs x4, fpcr
	stp w3, w4, [x2, #8]
	str d8, [x2, #144]
	str d9, [x2, #160]
	str d10, [x2, #176]
	str d11, [x2, #192]
	str d12, [x2, #208]
	str d13, [x2, #224]
	str d14, [x2, #240]
	str d15, [x2, #256]
	str xzr, [x2, #528]

	/* rt_sigprocmask(SIG_SETMASK, &ucp->uc_sigmask, &oucp->uc_sigmask, 8) */
	add x2, x0, #40
	add x1, x9, #40
	mov x0, #2
	mov x3, #8
	mov x8, #135
	svc #0
	cbnz x0, 2f

	add x2, x9, #464
	ldr w3, [x2]
	movz w4, #0x8001
	movk w4, #0x4650, lsl #16
	cmp w3, w4
	b.ne 1f
	ldp w3, w4, [x2, #8]
	msr fpsr, x3
	msr fpcr, x4
	ldr d8, [x2, #144]
	ldr d9, [x2, #160]
	ldr d10, [x2, #176]
	ldr d11, [x2, #192]
	ldr d12, [x2, #208]
	ldr d13, [x2, #224]
	ldr d14, [x2, #240]
	ldr d15, [x2, #256]

1:	ldp x0, x1, [x9, #184]
	ldp x2, x3, [x9, #200]
	ldp x4, x5, [x9, #216]
	ldp x6, x7, [x9, #232]
	ldp x19, x20, [x9, #336]
	ldp x21, x22, [x9, #352]
	ldp x23, x24, [x9, #368]
	ldp x25, x26, [x9, #384]
	ldp x27, x28, [x9, #400]
	ldp x29, x30, [x9, #416]
	ldr x16, [x9, #432]
	mov sp, x16
	ldr x16, [x9, #440]
	br x16
2:	b __kinetic_ctx_ret
.size swapcontext,.-swapcontext

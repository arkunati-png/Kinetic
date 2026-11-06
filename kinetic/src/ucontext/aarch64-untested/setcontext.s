/* int setcontext(const ucontext_t *ucp) -- see getcontext.s for offsets */
.text
.global setcontext
.type setcontext,%function
setcontext:
	mov x9, x0
	/* rt_sigprocmask(SIG_SETMASK, &ucp->uc_sigmask, NULL, 8) */
	add x1, x9, #40
	mov x0, #2
	mov x2, #0
	mov x3, #8
	mov x8, #135
	svc #0
	cbnz x0, 2f

	/* floating-point state, only if it is a fpsimd_context record */
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
.size setcontext,.-setcontext

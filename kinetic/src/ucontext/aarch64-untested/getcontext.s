/* int getcontext(ucontext_t *ucp)
 *
 * ucontext_t offsets (aarch64, checked at build time in makecontext.c):
 *   uc_sigmask = 40
 *   mcontext: regs[n] = 184+8*n, sp = 432, pc = 440
 *   __reserved (fpsimd_context, kernel layout) = 464:
 *       +0 magic  +4 size  +8 fpsr  +12 fpcr  +16+16*n vreg n
 * Only state that survives a call is saved: x19-x30, sp, d8-d15, fpsr and
 * fpcr. The saved pc is the return address and x0 is 0, so resuming the
 * context makes getcontext() return 0 a second time.
 */
.text
.global getcontext
.type getcontext,%function
getcontext:
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

	/* rt_sigprocmask(SIG_BLOCK, NULL, &ucp->uc_sigmask, 8) */
	add x2, x0, #40
	mov x0, #0
	mov x1, #0
	mov x3, #8
	mov x8, #135
	svc #0
	cbnz x0, 1f
	ret
1:	b __kinetic_ctx_ret
.size getcontext,.-getcontext

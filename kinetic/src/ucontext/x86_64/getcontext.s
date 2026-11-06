/* int getcontext(ucontext_t *ucp)
 *
 * ucontext_t offsets (x86_64, checked at build time in makecontext.c):
 *   gregs[i] = 40+8*i  (R8=0 R9=1 R10=2 R11=3 R12=4 R13=5 R14=6 R15=7
 *                       RDI=8 RSI=9 RBP=10 RBX=11 RDX=12 RAX=13 RCX=14
 *                       RSP=15 RIP=16)
 *   fpregs pointer = 224, uc_sigmask = 296, __fpregs_mem = 424
 * Saved RSP/RIP describe the state just after getcontext() returns.
 */
.text
.global getcontext
.type getcontext,@function
getcontext:
	mov %rbx, 128(%rdi)
	mov %rbp, 120(%rdi)
	mov %r12, 72(%rdi)
	mov %r13, 80(%rdi)
	mov %r14, 88(%rdi)
	mov %r15, 96(%rdi)
	mov %rdi, 104(%rdi)
	mov %rsi, 112(%rdi)
	mov %rdx, 136(%rdi)
	mov %rcx, 152(%rdi)
	mov %r8, 40(%rdi)
	mov %r9, 48(%rdi)
	mov (%rsp), %rcx
	mov %rcx, 168(%rdi)
	lea 8(%rsp), %rcx
	mov %rcx, 160(%rdi)
	lea 424(%rdi), %rcx
	mov %rcx, 224(%rdi)
	fnstenv (%rcx)
	fldenv (%rcx)
	stmxcsr 24(%rcx)
	/* rt_sigprocmask(SIG_BLOCK, NULL, &ucp->uc_sigmask, 8) */
	lea 296(%rdi), %rdx
	xor %esi, %esi
	xor %edi, %edi
	mov $8, %r10d
	mov $14, %eax
	syscall
	test %rax, %rax
	jnz 1f
	ret
1:	mov %rax, %rdi
	jmp __kinetic_ctx_ret
.size getcontext,.-getcontext

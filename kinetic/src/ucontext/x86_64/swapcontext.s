/* int swapcontext(ucontext_t *oucp, const ucontext_t *ucp)
 * Save into oucp (as getcontext does), then switch to ucp (as setcontext
 * does) using a single rt_sigprocmask that stores the old mask and
 * installs the new one. */
.text
.global swapcontext
.type swapcontext,@function
swapcontext:
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
	/* rt_sigprocmask(SIG_SETMASK, &ucp->uc_sigmask, &oucp->uc_sigmask, 8) */
	mov %rsi, %r9
	lea 296(%rdi), %rdx
	lea 296(%r9), %rsi
	mov $2, %edi
	mov $8, %r10d
	mov $14, %eax
	syscall
	test %rax, %rax
	jnz 2f
	mov 224(%r9), %rcx
	lea 424(%r9), %rax
	cmp %rax, %rcx
	jne 1f
	fldenv (%rcx)
	ldmxcsr 24(%rcx)
1:	mov 160(%r9), %rsp
	mov 128(%r9), %rbx
	mov 120(%r9), %rbp
	mov 72(%r9), %r12
	mov 80(%r9), %r13
	mov 88(%r9), %r14
	mov 96(%r9), %r15
	mov 168(%r9), %rcx
	push %rcx
	mov 104(%r9), %rdi
	mov 112(%r9), %rsi
	mov 136(%r9), %rdx
	mov 152(%r9), %rcx
	mov 40(%r9), %r8
	mov 48(%r9), %r9
	xor %eax, %eax
	ret
2:	mov %rax, %rdi
	jmp __kinetic_ctx_ret
.size swapcontext,.-swapcontext

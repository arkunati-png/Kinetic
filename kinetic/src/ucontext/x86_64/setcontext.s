/* int setcontext(const ucontext_t *ucp) -- see getcontext.s for offsets */
.text
.global setcontext
.type setcontext,@function
setcontext:
	mov %rdi, %r9
	/* rt_sigprocmask(SIG_SETMASK, &ucp->uc_sigmask, NULL, 8) */
	lea 296(%r9), %rsi
	mov $2, %edi
	xor %edx, %edx
	mov $8, %r10d
	mov $14, %eax
	syscall
	test %rax, %rax
	jnz 2f
	/* Restore x87/SSE control state only if it is in our own format. */
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
.size setcontext,.-setcontext

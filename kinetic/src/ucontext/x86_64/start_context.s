/* Entered when a makecontext() entry function returns. %rbx (callee-saved,
 * so intact) points at the stack slot holding the saved uc_link. */
.text
.global __kinetic_start_context
.hidden __kinetic_start_context
.type __kinetic_start_context,@function
__kinetic_start_context:
	.cfi_startproc
	.cfi_undefined rip
	mov (%rbx), %rdi
	call __kinetic_context_return
	hlt
	.cfi_endproc
.size __kinetic_start_context,.-__kinetic_start_context

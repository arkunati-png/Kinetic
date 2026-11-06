/* Entered when a makecontext() entry function returns (it was set up as the
 * link register). x19, which is callee-saved, holds the saved uc_link. */
.text
.global __kinetic_start_context
.hidden __kinetic_start_context
.type __kinetic_start_context,%function
__kinetic_start_context:
	.cfi_startproc
	.cfi_undefined x30
	mov x0, x19
	bl __kinetic_context_return
	brk #0
	.cfi_endproc
.size __kinetic_start_context,.-__kinetic_start_context

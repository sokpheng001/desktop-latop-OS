/*
 * Project: OS-001
 * Date: 2026-11-26
 * Authors: kwan, oda, sokpheng001
 *
 * Thread context switch. A switch is an ordinary function call, so only the
 * registers the Procedure Call Standard for the Arm 64-bit Architecture (AAPCS64,
 * section 6.1.1) requires a callee to preserve are saved: x19-x28, the frame pointer
 * x29 and the return address x30, plus the stack pointer. The kernel is built with
 * -mgeneral-regs-only, so no FP/SIMD registers are live. The registers are pushed
 * onto the outgoing thread's own stack, so a thread's whole saved state is one
 * stack pointer.
 */

/* x19-x30: 12 registers, 96 bytes, keeps SP 16-byte aligned. */
#define CTX_SIZE 96

	.text

/* void arch_context_switch(uptr *save_sp, uptr new_sp) */
	.global arch_context_switch
	.type arch_context_switch, %function
arch_context_switch:
	sub	sp, sp, #CTX_SIZE
	stp	x19, x20, [sp, #0]
	stp	x21, x22, [sp, #16]
	stp	x23, x24, [sp, #32]
	stp	x25, x26, [sp, #48]
	stp	x27, x28, [sp, #64]
	stp	x29, x30, [sp, #80]
	mov	x2, sp
	str	x2, [x0]

	mov	sp, x1
	ldp	x19, x20, [sp, #0]
	ldp	x21, x22, [sp, #16]
	ldp	x23, x24, [sp, #32]
	ldp	x25, x26, [sp, #48]
	ldp	x27, x28, [sp, #64]
	ldp	x29, x30, [sp, #80]
	add	sp, sp, #CTX_SIZE
	ret
	.size arch_context_switch, . - arch_context_switch

/*
 * First code a new thread runs: arch_context_init put start in x19 and its argument
 * in x20. start must never return; if it does, trap.
 */
	.type arch_thread_trampoline, %function
arch_thread_trampoline:
	mov	x0, x20
	blr	x19
	brk	#0
	.size arch_thread_trampoline, . - arch_thread_trampoline

/* uptr arch_context_init(uptr stack_top, void (*start)(void *), void *arg) */
	.global arch_context_init
	.type arch_context_init, %function
arch_context_init:
	and	x0, x0, #~15		/* AAPCS64: SP must be 16-byte aligned */
	sub	x0, x0, #CTX_SIZE
	stp	x1, x2, [x0, #0]	/* x19 = start, x20 = arg */
	stp	xzr, xzr, [x0, #16]
	stp	xzr, xzr, [x0, #32]
	stp	xzr, xzr, [x0, #48]
	stp	xzr, xzr, [x0, #64]
	adr	x3, arch_thread_trampoline
	stp	xzr, x3, [x0, #80]	/* x29 = 0 ends frame-pointer chains, x30 = trampoline */
	ret
	.size arch_context_init, . - arch_context_init

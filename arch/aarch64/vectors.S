/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * EL1 exception vector table. Every entry saves the interrupted state into an
 * exc_frame on the stack, calls arch_exception_handler, then restores it and
 * returns with eret. The frame lives on the interrupted thread's kernel stack, so
 * a thread switch inside the handler keeps it until that thread returns.
 */

#include "arch/aarch64/exception.h"

/* One 128-byte table slot: free x0/x1, record which slot fired, join the common path. */
.macro vector_entry kind
	.balign 0x80
	sub	sp, sp, #EXC_FRAME_SIZE
	stp	x0, x1, [sp, #EXC_FRAME_X0]
	mov	x1, #\kind
	b	exc_common
.endm

	.text
	/* VBAR_EL1 bits [10:0] are RES0: the table must be 2 KB aligned. */
	.balign 0x800
	.global exception_vectors
exception_vectors:
	vector_entry 0
	vector_entry 1
	vector_entry 2
	vector_entry 3
	vector_entry 4
	vector_entry 5
	vector_entry 6
	vector_entry 7
	vector_entry 8
	vector_entry 9
	vector_entry 10
	vector_entry 11
	vector_entry 12
	vector_entry 13
	vector_entry 14
	vector_entry 15

exc_common:
	stp	x2, x3, [sp, #16]
	stp	x4, x5, [sp, #32]
	stp	x6, x7, [sp, #48]
	stp	x8, x9, [sp, #64]
	stp	x10, x11, [sp, #80]
	stp	x12, x13, [sp, #96]
	stp	x14, x15, [sp, #112]
	stp	x16, x17, [sp, #128]
	stp	x18, x19, [sp, #144]
	stp	x20, x21, [sp, #160]
	stp	x22, x23, [sp, #176]
	stp	x24, x25, [sp, #192]
	stp	x26, x27, [sp, #208]
	stp	x28, x29, [sp, #224]
	mrs	x2, elr_el1
	stp	x30, x2, [sp, #EXC_FRAME_X30]
	mrs	x2, spsr_el1
	mrs	x3, esr_el1
	stp	x2, x3, [sp, #EXC_FRAME_SPSR]
	mrs	x2, far_el1
	mrs	x3, sp_el0
	stp	x2, x3, [sp, #EXC_FRAME_FAR]

	mov	x0, sp
	bl	arch_exception_handler

	/* The handler may have changed ELR/SPSR in the frame (e.g. to skip an instruction). */
	ldr	x2, [sp, #EXC_FRAME_ELR]
	ldr	x3, [sp, #EXC_FRAME_SPSR]
	msr	elr_el1, x2
	msr	spsr_el1, x3
	/* Each thread's user stack pointer travels in its frame across thread switches. */
	ldr	x2, [sp, #EXC_FRAME_SP0]
	msr	sp_el0, x2
	ldp	x0, x1, [sp, #0]
	ldp	x2, x3, [sp, #16]
	ldp	x4, x5, [sp, #32]
	ldp	x6, x7, [sp, #48]
	ldp	x8, x9, [sp, #64]
	ldp	x10, x11, [sp, #80]
	ldp	x12, x13, [sp, #96]
	ldp	x14, x15, [sp, #112]
	ldp	x16, x17, [sp, #128]
	ldp	x18, x19, [sp, #144]
	ldp	x20, x21, [sp, #160]
	ldp	x22, x23, [sp, #176]
	ldp	x24, x25, [sp, #192]
	ldp	x26, x27, [sp, #208]
	ldp	x28, x29, [sp, #224]
	ldr	x30, [sp, #EXC_FRAME_X30]
	add	sp, sp, #EXC_FRAME_SIZE
	eret

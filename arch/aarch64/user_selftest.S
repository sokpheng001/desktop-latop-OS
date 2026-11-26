/*
 * Project: OS-001
 * Date: 2026-11-26
 * Authors: kwan, oda, sokpheng001
 *
 * User programs for the user-mode self-test (make KERNEL_TEST=user). They are only
 * data inside the kernel image (in .rodata, never executed there): user_spawn copies
 * them into user pages, so they must be position-independent (adr, no absolute
 * addresses) and reach the kernel only through svc.
 */

#include "syscall.h"

#if defined(KERNEL_TEST_USER)

.macro sys nr
	mov	x8, #\nr
	svc	#0
.endm

.macro print label, len
	adr	x0, \label
	mov	x1, #\len
	sys	SYS_WRITE
.endm

	.section .rodata.user, "a"
	.balign 4

/* Says hello, yields, checks the kernel refuses a bad pointer and an unknown call, exits 7. */
	.global user_hello_start, user_hello_end
user_hello_start:
	print	hello_msg, hello_len
	sys	SYS_YIELD

	/* Address 0 is not this program's memory: write must fail with -EFAULT. */
	mov	x0, #0
	mov	x1, #16
	sys	SYS_WRITE
	cmn	x0, #SYS_EFAULT
	b.ne	1f
	print	efault_msg, efault_len
1:
	/* No system call 99: must fail with -ENOSYS. */
	sys	99
	cmn	x0, #SYS_ENOSYS
	b.ne	2f
	print	enosys_msg, enosys_len
2:
	/* The stack is writable: store and read back through it. */
	mov	x2, #0x5a
	str	x2, [sp, #-16]!
	ldr	x3, [sp], #16
	cmp	x2, x3
	b.ne	3f
	print	stack_msg, stack_len
3:
	mov	x0, #7
	sys	SYS_EXIT
	brk	#0

hello_msg:	.ascii "[user] hello from EL0\n"
	.equ	hello_len, . - hello_msg
efault_msg:	.ascii "[user] bad pointer refused\n"
	.equ	efault_len, . - efault_msg
enosys_msg:	.ascii "[user] unknown call refused\n"
	.equ	enosys_len, . - enosys_msg
stack_msg:	.ascii "[user] stack works\n"
	.equ	stack_len, . - stack_msg
	.balign 4
user_hello_end:

/* Writes over its own read-only code: must be killed, with the kernel still running. */
	.global user_bad_start, user_bad_end
user_bad_start:
	print	bad_msg, bad_len
	adr	x1, user_bad_start
	str	xzr, [x1]
	print	alive_msg, alive_len
	mov	x0, #0
	sys	SYS_EXIT

bad_msg:	.ascii "[user] writing to my own code\n"
	.equ	bad_len, . - bad_msg
alive_msg:	.ascii "[user] write to code was allowed\n"
	.equ	alive_len, . - alive_msg
	.balign 4
user_bad_end:

/*
 * Twins: two copies run at the same addresses in different address spaces. Each keeps
 * its argument in its own stack page across several switches, then exits with what it
 * reads back. Shared memory would make one twin return the other's value.
 */
	.global user_twin_start, user_twin_end
user_twin_start:
	mov	x19, x0
	str	x0, [sp, #-16]!
	mov	x20, #5
1:	sys	SYS_YIELD
	subs	x20, x20, #1
	b.ne	1b
	ldr	x21, [sp], #16
	cmp	x21, x19
	b.ne	2f
	print	twin_msg, twin_len
2:	mov	x0, x21
	sys	SYS_EXIT
	brk	#0

twin_msg:	.ascii "[user] twin kept its own memory\n"
	.equ	twin_len, . - twin_msg
	.balign 4
user_twin_end:

/* Tries to read kernel memory (the kernel image): must be killed. */
	.global user_peek_start, user_peek_end
user_peek_start:
	ldr	x1, peek_addr
	ldr	x2, [x1]
	mov	x0, #0
	sys	SYS_EXIT
	.balign 8
	.global user_peek_addr_offset
peek_addr:	.quad 0
user_peek_end:
	.equ	user_peek_addr_offset, peek_addr - user_peek_start

#endif

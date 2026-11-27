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

#include "perm.h"
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

/*
 * Started holding only PERM_CONSOLE (plus an unknown bit the kernel must ignore). Gives
 * console access up, then must be refused printing and unable to get it back. Exits 9
 * if every check passed, else the number of the check that failed.
 */
	.global user_perm_start, user_perm_end
user_perm_start:
	sys	SYS_PERM_GET
	cmp	x0, #PERM_CONSOLE
	b.ne	1f
	print	perm_msg, perm_len

	mov	x0, #PERM_CONSOLE
	sys	SYS_PERM_DROP
	cbnz	x0, 2f

	print	perm_fail_msg, perm_fail_len
	cmn	x0, #SYS_EPERM
	b.ne	3f

	/* Dropping nothing or everything must not bring any permission back. */
	mov	x0, #0
	sys	SYS_PERM_DROP
	cbnz	x0, 4f
	mov	x0, #-1
	sys	SYS_PERM_DROP
	cbnz	x0, 4f
	sys	SYS_PERM_GET
	cbnz	x0, 4f

	mov	x0, #9
	sys	SYS_EXIT
1:	mov	x0, #1
	sys	SYS_EXIT
2:	mov	x0, #2
	sys	SYS_EXIT
3:	mov	x0, #3
	sys	SYS_EXIT
4:	mov	x0, #4
	sys	SYS_EXIT
	brk	#0

perm_msg:	.ascii "[user] holding the console permission only\n"
	.equ	perm_len, . - perm_msg
perm_fail_msg:	.ascii "[user] printed after giving up the console\n"
	.equ	perm_fail_len, . - perm_fail_msg
	.balign 4
user_perm_end:

/*
 * Message passing. The server answers arg requests: each reply goes back to the
 * sender the kernel named and carries the first word plus one and the second word
 * unchanged. It also checks that a send to itself and to pid 0 are refused, then
 * exits with the number of requests answered (else 100 + the failed check).
 */
	.global user_server_start, user_server_end
user_server_start:
	mov	x19, x0
	mov	x20, #0
1:	cmp	x20, x19
	b.eq	2f
	sys	SYS_RECV
	add	x1, x1, #1
	sys	SYS_SEND
	cbnz	x0, 8f
	add	x20, x20, #1
	b	1b
2:	sys	SYS_GETPID
	sys	SYS_SEND
	cmn	x0, #SYS_EDEADLK
	b.ne	9f
	mov	x0, #0
	sys	SYS_SEND
	cmn	x0, #SYS_ESRCH
	b.ne	9f
	print	server_msg, server_len
	mov	x0, x20
	sys	SYS_EXIT
8:	mov	x0, #101
	sys	SYS_EXIT
9:	mov	x0, #102
	sys	SYS_EXIT
	brk	#0

server_msg:	.ascii "[user] server answered every request\n"
	.equ	server_len, . - server_msg
	.balign 4
user_server_end:

/*
 * Client: sends three requests (i, own pid) to the server whose pid is arg, and checks
 * each reply comes from the server and carries (i + 1, own pid), so replies never go
 * to the wrong client. Exits 5, else the number of the failed check.
 */
	.global user_client_start, user_client_end
user_client_start:
	mov	x19, x0
	sys	SYS_GETPID
	mov	x21, x0
	mov	x20, #1
1:	mov	x0, x19
	mov	x1, x20
	mov	x2, x21
	sys	SYS_SEND
	cbnz	x0, 7f
	sys	SYS_RECV
	cmp	x0, x19
	b.ne	8f
	add	x3, x20, #1
	cmp	x1, x3
	b.ne	9f
	cmp	x2, x21
	b.ne	9f
	add	x20, x20, #1
	cmp	x20, #3
	b.ls	1b
	print	client_msg, client_len
	mov	x0, #5
	sys	SYS_EXIT
7:	mov	x0, #1
	sys	SYS_EXIT
8:	mov	x0, #2
	sys	SYS_EXIT
9:	mov	x0, #3
	sys	SYS_EXIT
	brk	#0

client_msg:	.ascii "[user] client got every reply\n"
	.equ	client_len, . - client_msg
	.balign 4
user_client_end:

/* Yields a few times without ever receiving, then exits 0. */
	.global user_quitter_start, user_quitter_end
user_quitter_start:
	mov	x19, #5
1:	sys	SYS_YIELD
	subs	x19, x19, #1
	b.ne	1b
	mov	x0, #0
	sys	SYS_EXIT
	brk	#0
	.balign 4
user_quitter_end:

/* Sends to the quitter (pid in arg), which never receives: must fail, exit 6. */
	.global user_lonely_start, user_lonely_end
user_lonely_start:
	mov	x1, #1
	mov	x2, #2
	sys	SYS_SEND
	cmn	x0, #SYS_ESRCH
	b.ne	1f
	print	lonely_msg, lonely_len
	mov	x0, #6
	sys	SYS_EXIT
1:	mov	x0, #1
	sys	SYS_EXIT
	brk	#0

lonely_msg:	.ascii "[user] send to an ended program failed\n"
	.equ	lonely_len, . - lonely_msg
	.balign 4
user_lonely_end:

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

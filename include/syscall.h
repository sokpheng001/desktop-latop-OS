/*
 * Project: OS-001
 * Date: 2026-11-26
 * Authors: kwan, oda, sokpheng001
 *
 * System call numbers and error codes, shared by the kernel and user programs (safe
 * to include from assembly). AArch64 convention: number in x8, arguments in x0-x5,
 * `svc #0`, result in x0 (negative = error).
 */

#ifndef OS001_SYSCALL_H
#define OS001_SYSCALL_H

/*
 * write(buf, len): print len bytes of the caller's memory on the console; returns len.
 * Needs PERM_CONSOLE.
 */
#define SYS_WRITE     0
/* exit(code): end the calling program. Does not return. */
#define SYS_EXIT      1
/* yield(): let other threads run. Returns 0. */
#define SYS_YIELD     2
/* perm_get(): the caller's permission set (perm.h). */
#define SYS_PERM_GET  3
/* perm_drop(set): give up the permissions in set for good; returns what is left. */
#define SYS_PERM_DROP 4

/* Errors, returned negated. */
#define SYS_EPERM  1  /* the caller does not hold the permission */
#define SYS_EFAULT 14 /* a pointer outside the caller's own memory */
#define SYS_ENOSYS 38 /* no such system call */

#endif /* OS001_SYSCALL_H */

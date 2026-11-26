/*
 * Project: OS-001
 * Date: 2026-11-26
 * Authors: kwan, oda, sokpheng001
 *
 * User programs: code that runs in user mode (EL0) in its own pages and reaches the
 * kernel only through system calls (syscall.h).
 */

#ifndef OS001_USER_H
#define OS001_USER_H

#include "compiler.h"
#include "types.h"

#ifndef __ASSEMBLY__

/*
 * Every program has its own address space with the same layout: code at USER_BASE,
 * a one-page stack ending at USER_BASE + USER_SIZE, unmapped guard space between.
 * Another program's pages are simply not in its page tables.
 */
#define USER_BASE	 0x4000000000ul /* 256 GB: above RAM and devices on every board */
#define USER_SIZE	 0x40000000ul	/* 1 GB: one level 1 entry of the space */
#define USER_MAX_PROCS	 64u		/* programs alive at once (one ASID each) */
#define USER_MAX_PAGES	 4u		/* code pages per program */
#define USER_EXIT_KILLED (-1)

struct user_proc {
	u32 asid;
	uptr space;
	u32 code_pages;
	uptr code_pa[USER_MAX_PAGES];
	uptr stack_pa;
	u64 arg; /* passed to the program in x0 */
	int exit_code;
	bool exited;
};

/*
 * Start a user program in a new address space: copy size bytes of position-independent
 * code into fresh pages mapped read+execute at USER_BASE, give it a stack, and run it
 * in a new thread with arg in x0. Returns the program, which stays valid after it ends
 * (exit_code), or NULL.
 */
struct user_proc *user_spawn(const char *name, const void *code, usize size, u64 arg);

/* Release a finished program's bookkeeping (its pages were freed when it ended). */
void user_release(struct user_proc *p);

/* System call from the running user program: number nr, arguments a0-a2. */
long user_syscall(u64 nr, u64 a0, u64 a1, u64 a2);

/* The running user program caused an exception: report it and end the program only. */
void user_kill(const char *what, uptr pc, uptr addr) ATTR_NORETURN;

#endif /* !__ASSEMBLY__ */

#endif /* OS001_USER_H */

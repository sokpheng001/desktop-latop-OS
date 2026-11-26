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
 * Each program gets a 2 MB window of user addresses: code at the bottom, a one-page
 * stack at the top, unmapped guard space in between. One window per running program
 * until each process has its own page tables.
 */
#define USER_BASE	 0x4000000000ul /* 256 GB: above RAM and devices on every board */
#define USER_SLOT_SIZE	 0x200000ul
#define USER_SLOTS	 16u
#define USER_MAX_PAGES	 4u /* code pages per program */
#define USER_EXIT_KILLED (-1)

struct user_proc {
	u32 slot;
	u32 code_pages;
	uptr code_pa[USER_MAX_PAGES];
	uptr stack_pa;
	uptr base; /* first user address of the slot */
	int exit_code;
	bool exited;
};

/*
 * Start a user program: copy size bytes of position-independent code into fresh pages,
 * map them read+execute for the program, give it a stack, and run it in a new thread.
 * Returns the program, which stays valid after it ends (exit_code), or NULL.
 */
struct user_proc *user_spawn(const char *name, const void *code, usize size);

/* Release a finished program's bookkeeping (its pages were freed when it ended). */
void user_release(struct user_proc *p);

/* System call from the running user program: number nr, arguments a0-a2. */
long user_syscall(u64 nr, u64 a0, u64 a1, u64 a2);

/* The running user program caused an exception: report it and end the program only. */
void user_kill(const char *what, uptr pc, uptr addr) ATTR_NORETURN;

#endif /* !__ASSEMBLY__ */

#endif /* OS001_USER_H */

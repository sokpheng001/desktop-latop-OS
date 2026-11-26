/*
 * Project: OS-001
 * Date: 2026-11-26
 * Authors: kwan, oda, sokpheng001
 *
 * Kernel threads: independent flows of control inside the kernel, each with its
 * own stack. Switching is cooperative for now (thread_yield); the timer-driven
 * scheduler comes next.
 */

#ifndef OS001_THREAD_H
#define OS001_THREAD_H

#include "compiler.h"
#include "types.h"

#ifndef __ASSEMBLY__

/* Stack per thread: four pages. */
#define THREAD_STACK_SIZE (16u * 1024u)

enum thread_state {
	THREAD_RUNNING,
	THREAD_READY,
	THREAD_DEAD,
};

struct thread {
	uptr sp; /* saved stack pointer while not running */
	u32 id;
	enum thread_state state;
	const char *name;
	void (*entry)(void *);
	void *arg;
	void *stack;	     /* NULL for the boot thread, which keeps the boot stack */
	struct thread *next; /* run queue or dead list link */
};

/* Turn the code running kernel_main into thread 0 ("main"). Call once, after heap_init. */
void thread_init(void);

/* Start entry(arg) in a new thread, queued to run after those already ready. NULL if out of memory.
 */
struct thread *thread_create(const char *name, void (*entry)(void *), void *arg);

/* Let the next ready thread run; returns when this thread's turn comes round again. */
void thread_yield(void);

/* End the calling thread; its stack is freed once another thread runs. Not for thread 0. */
void thread_exit(void) ATTR_NORETURN;

struct thread *thread_current(void);

/* Threads alive: running, ready or not yet reaped. */
usize thread_count(void);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_THREAD_H */

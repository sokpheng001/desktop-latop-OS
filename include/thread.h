/*
 * Project: OS-001
 * Date: 2026-11-26
 * Authors: kwan, oda, sokpheng001
 *
 * Kernel threads: independent flows of control inside the kernel, each with its
 * own stack. Round-robin: a thread runs until it yields or its time slice of
 * THREAD_SLICE_TICKS timer ticks runs out, then the next ready thread runs.
 */

#ifndef OS001_THREAD_H
#define OS001_THREAD_H

#include "compiler.h"
#include "types.h"

#ifndef __ASSEMBLY__

/* Time slice: timer ticks a thread may run before it is preempted (20 ms at 100 Hz). */
#define THREAD_SLICE_TICKS 2u

/* Stack per thread: four pages. */
#define THREAD_STACK_SIZE (16u * 1024u)

enum thread_state {
	THREAD_RUNNING,
	THREAD_READY,
	THREAD_BLOCKED, /* waiting for thread_wake; on no queue */
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
	u64 runs;	     /* times this thread has been switched to */
	void *user;	     /* struct user_proc for a user program, NULL for kernel threads */
	uptr space;	     /* address space (arch_mmu_space_create), 0 for the kernel's */
	struct thread *next; /* run queue or dead list link */
};

/* Turn the code running kernel_main into thread 0 ("main"). Call once, after heap_init. */
void thread_init(void);

/* Start entry(arg) in a new thread, queued to run after those already ready. NULL if out of memory.
 */
struct thread *thread_create(const char *name, void (*entry)(void *), void *arg);

/* thread_create for a user program: user and space are set before the thread can run. */
struct thread *thread_create_user(const char *name, void (*entry)(void *), void *arg, void *user,
				  uptr space);

/* Let the next ready thread run; returns when this thread's turn comes round again. */
void thread_yield(void);

/*
 * Stop running until another thread calls thread_wake on this one. The caller masks
 * IRQs (arch_irq_save) around checking its wait condition and calling this, so a wake
 * cannot slip in between. Not for thread 0, which must always be able to run.
 */
void thread_block(void);

/* Make a blocked thread ready again (queued last); does nothing if it is not blocked. */
void thread_wake(struct thread *t);

/* End the calling thread; its stack is freed once another thread runs. Not for thread 0. */
void thread_exit(void) ATTR_NORETURN;

/* From the timer interrupt: charge the running thread one tick of its slice. */
void thread_tick(void);

/*
 * From the end of interrupt handling, after every interrupt is completed: switch
 * threads if the running one's slice ran out. The interrupted thread resumes later
 * exactly where the interrupt hit it.
 */
void thread_preempt(void);

struct thread *thread_current(void);

/* Threads alive: running, ready, blocked or not yet reaped. */
usize thread_count(void);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_THREAD_H */

/*
 * Project: OS-001
 * Date: 2026-11-26
 * Authors: kwan, oda, sokpheng001
 *
 * Kernel threads with a round-robin scheduler: a FIFO run queue, and a time slice
 * charged by the timer tick. When the slice runs out the switch happens at the end
 * of the interrupt, on the interrupted thread's own stack, so its interrupt frame
 * stays put until it is resumed and returns from the exception normally.
 * The running thread is never on the queue.
 * A finished thread cannot free the stack it is still running on, so it goes on a
 * dead list and the next thread to run frees it.
 */

#include "arch.h"
#include "heap.h"
#include "lib/kprintf.h"
#include "thread.h"

static struct thread boot_thread;
static struct thread *current;
static struct thread *ready_head;
static struct thread *ready_tail;
static struct thread *dead;
static u32 next_id;
static usize alive;
static u32 slice_left;
static volatile bool need_resched;

static void ready_push(struct thread *t)
{
	t->state = THREAD_READY;
	t->next = NULL;
	if (ready_tail)
		ready_tail->next = t;
	else
		ready_head = t;
	ready_tail = t;
}

static struct thread *ready_pop(void)
{
	struct thread *t = ready_head;

	if (t) {
		ready_head = t->next;
		if (!ready_head)
			ready_tail = NULL;
		t->next = NULL;
	}
	return t;
}

/* Free threads that finished; never the one running. Called with IRQs masked. */
static void reap(void)
{
	while (dead) {
		struct thread *t = dead;

		dead = t->next;
		kfree(t->stack);
		kfree(t);
		alive--;
	}
}

/* Switch from the running thread to next. Called with IRQs masked. */
static void switch_to(struct thread *next)
{
	struct thread *prev = current;

	current = next;
	next->state = THREAD_RUNNING;
	next->runs++;
	if (next->space != prev->space)
		arch_mmu_switch(next->space);
	slice_left = THREAD_SLICE_TICKS;
	need_resched = false;
	arch_context_switch(&prev->sp, next->sp);
	/* Running again as prev: whichever thread switched here may have left a dead one. */
	reap();
}

/* Every new thread starts here (via arch_context_init) with IRQs still masked. */
static void thread_start(void *arg)
{
	struct thread *self = arg;

	reap();
	arch_irq_enable();
	self->entry(self->arg);
	thread_exit();
}

void thread_init(void)
{
	boot_thread.id = next_id++;
	boot_thread.state = THREAD_RUNNING;
	boot_thread.name = "main";
	boot_thread.runs = 1;
	current = &boot_thread;
	slice_left = THREAD_SLICE_TICKS;
	alive = 1;
}

struct thread *thread_create(const char *name, void (*entry)(void *), void *arg)
{
	return thread_create_user(name, entry, arg, NULL, 0);
}

struct thread *thread_create_user(const char *name, void (*entry)(void *), void *arg, void *user,
				  uptr space)
{
	struct thread *t = kzalloc(sizeof(*t));
	void *stack = kmalloc(THREAD_STACK_SIZE);

	if (!t || !stack) {
		kfree(t);
		kfree(stack);
		return NULL;
	}
	t->name = name;
	t->entry = entry;
	t->arg = arg;
	t->stack = stack;
	t->user = user;
	t->space = space;
	t->sp = arch_context_init((uptr)stack + THREAD_STACK_SIZE, thread_start, t);

	unsigned long flags = arch_irq_save();

	t->id = next_id++;
	alive++;
	ready_push(t);
	arch_irq_restore(flags);
	return t;
}

void thread_yield(void)
{
	unsigned long flags = arch_irq_save();
	struct thread *next = ready_pop();

	if (next) {
		ready_push(current);
		switch_to(next);
	} else {
		/* Nobody else wants the CPU: start a fresh slice. */
		slice_left = THREAD_SLICE_TICKS;
		need_resched = false;
	}
	arch_irq_restore(flags);
}

void thread_block(void)
{
	unsigned long flags = arch_irq_save();

	if (current == &boot_thread) {
		kprintf("thread: the main thread cannot block\n");
		arch_halt();
	}

	/* Thread 0 never blocks, so it is ready whenever another thread runs. */
	struct thread *next = ready_pop();

	if (!next) {
		kprintf("thread: no thread left to run\n");
		arch_halt();
	}
	current->state = THREAD_BLOCKED;
	switch_to(next);
	arch_irq_restore(flags);
}

void thread_wake(struct thread *t)
{
	unsigned long flags = arch_irq_save();

	if (t->state == THREAD_BLOCKED)
		ready_push(t);
	arch_irq_restore(flags);
}

void thread_exit(void)
{
	arch_irq_save();
	if (current == &boot_thread) {
		kprintf("thread: the main thread cannot exit\n");
		arch_halt();
	}

	/* Thread 0 never exits, so someone is always ready or will become so. */
	struct thread *next = ready_pop();

	current->state = THREAD_DEAD;
	current->next = dead;
	dead = current;
	if (!next) {
		kprintf("thread: no thread left to run\n");
		arch_halt();
	}
	switch_to(next);
	UNREACHABLE();
}

void thread_tick(void)
{
	if (current && slice_left && --slice_left == 0)
		need_resched = true;
}

void thread_preempt(void)
{
	if (current && need_resched)
		thread_yield();
}

struct thread *thread_current(void)
{
	return current;
}

usize thread_count(void)
{
	return alive;
}

/*
 * Project: OS-001
 * Date: 2026-11-27
 * Authors: kwan, oda, sokpheng001
 *
 * Synchronous message passing (rendezvous): a send completes only when the receiver
 * takes the message, so the kernel never buffers messages and never allocates memory
 * for them. Messages are a few words in registers, copied straight from the sender's
 * saved registers to the receiver's, and there are no timeouts (Liedtke, "Improving
 * IPC by Kernel Design", SOSP 1993; Heiser & Elphinstone, "L4 Microkernels: The
 * Lessons from 20 Years of Research and Deployment", ACM TOCS 2016).
 *
 * The kernel stamps every message with the sender's id, so a receiver always knows
 * who is talking to it and the id cannot be forged.
 *
 * Everything below runs with IRQs masked: on one core that makes each step atomic, so
 * a program cannot be preempted between looking for its partner and blocking.
 */

#include "arch.h"
#include "ipc.h"
#include "syscall.h"
#include "thread.h"
#include "user.h"

static void copy_msg(u64 *to, const u64 *from)
{
	for (int i = 0; i < IPC_WORDS; i++)
		to[i] = from[i];
}

long ipc_send(struct user_proc *self, u64 dest, const u64 msg[IPC_WORDS])
{
	unsigned long flags = arch_irq_save();
	struct user_proc *d = user_find(dest);
	long r;

	if (!d) {
		r = -SYS_ESRCH;
	} else if (d == self) {
		r = -SYS_EDEADLK;
	} else if (d->ipc_receiving) {
		/* The receiver is already waiting: hand the message over and wake it. */
		copy_msg(d->ipc_msg, msg);
		d->ipc_from = self->pid;
		d->ipc_receiving = false;
		thread_wake(d->thread);
		r = 0;
	} else {
		/* Queue behind earlier senders (first come, first served) and wait. */
		struct user_proc **link = &d->ipc_senders;

		while (*link)
			link = &(*link)->ipc_next;
		copy_msg(self->ipc_msg, msg);
		self->ipc_next = NULL;
		*link = self;
		thread_block();
		r = self->ipc_result;
	}
	arch_irq_restore(flags);
	return r;
}

long ipc_recv(struct user_proc *self, u64 msg[IPC_WORDS])
{
	unsigned long flags = arch_irq_save();
	struct user_proc *s = self->ipc_senders;
	long from;

	if (s) {
		/* A sender is already waiting: take its message and let it go on. */
		self->ipc_senders = s->ipc_next;
		s->ipc_next = NULL;
		copy_msg(msg, s->ipc_msg);
		from = s->pid;
		s->ipc_result = 0;
		thread_wake(s->thread);
	} else {
		self->ipc_receiving = true;
		thread_block();
		/* Woken by ipc_send, which filled in the message. */
		copy_msg(msg, self->ipc_msg);
		from = self->ipc_from;
	}
	arch_irq_restore(flags);
	return from;
}

void ipc_exit(struct user_proc *self)
{
	while (self->ipc_senders) {
		struct user_proc *s = self->ipc_senders;

		self->ipc_senders = s->ipc_next;
		s->ipc_next = NULL;
		s->ipc_result = -SYS_ESRCH;
		thread_wake(s->thread);
	}
}

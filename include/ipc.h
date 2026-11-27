/*
 * Project: OS-001
 * Date: 2026-11-27
 * Authors: kwan, oda, sokpheng001
 *
 * Message passing between user programs: synchronous, with short messages that
 * travel in registers.
 */

#ifndef OS001_IPC_H
#define OS001_IPC_H

#include "types.h"

/* Words in one message: passed in x1-x2 by send and returned in x1-x2 by recv. */
#define IPC_WORDS 2

#ifndef __ASSEMBLY__

struct user_proc;

/*
 * Send msg to the program with id dest and wait until it has received it. Returns 0,
 * or -SYS_ESRCH if there is no such program or it ends before receiving, or
 * -SYS_EDEADLK for a send to itself (it could never receive it).
 */
long ipc_send(struct user_proc *self, u64 dest, const u64 msg[IPC_WORDS]);

/* Wait for a message, store it in msg and return the sender's id (always above 0). */
long ipc_recv(struct user_proc *self, u64 msg[IPC_WORDS]);

/*
 * self is ending and can no longer be found by its id: fail every send still waiting
 * for it. Called with IRQs masked.
 */
void ipc_exit(struct user_proc *self);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_IPC_H */

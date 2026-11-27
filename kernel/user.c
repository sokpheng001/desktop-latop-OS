/*
 * Project: OS-001
 * Date: 2026-11-26
 * Authors: kwan, oda, sokpheng001
 *
 * User programs and system calls. Each program has its own address space, so other
 * programs' pages do not exist for it; every pointer it passes in is checked against
 * its own pages before the kernel touches it, and a program that faults is ended
 * without harming the kernel or other programs. System calls that reach anything
 * outside the program's own memory are checked against its permission set (perm.h).
 */

#include "arch.h"
#include "console.h"
#include "heap.h"
#include "ipc.h"
#include "lib/kprintf.h"
#include "perm.h"
#include "pmm.h"
#include "syscall.h"
#include "thread.h"
#include "user.h"

/* ASIDs in use; index 0 stands for ASID 1. */
static bool asid_used[USER_MAX_PROCS];
/* Running programs that can be found by id, by ASID; NULL once a program is ending. */
static struct user_proc *procs[USER_MAX_PROCS];
static u32 next_pid = 1;

struct user_proc *user_find(u64 pid)
{
	for (u32 i = 0; pid && i < USER_MAX_PROCS; i++)
		if (procs[i] && procs[i]->pid == pid)
			return procs[i];
	return NULL;
}

static uptr stack_va(void)
{
	return USER_BASE + USER_SIZE - PMM_PAGE_SIZE;
}

/*
 * Unmap and free every page of p, then its page tables and ASID. Runs in the kernel's
 * own space (the caller switches first), so p's tables are not in use.
 */
static void free_process(struct user_proc *p)
{
	for (u32 i = 0; i < p->code_pages; i++) {
		arch_mmu_unmap_user(p->space, USER_BASE + i * PMM_PAGE_SIZE);
		pmm_free_page(p->code_pa[i]);
	}
	p->code_pages = 0;
	if (p->stack_pa) {
		arch_mmu_unmap_user(p->space, stack_va());
		pmm_free_page(p->stack_pa);
		p->stack_pa = 0;
	}
	if (p->space) {
		arch_mmu_space_destroy(p->space);
		p->space = 0;
	}
	if (p->asid) {
		procs[p->asid - 1] = NULL;
		asid_used[p->asid - 1] = false;
	}
	p->asid = 0;
}

static void user_thread(void *arg)
{
	struct user_proc *p = arg;

	/* Set here, before the program can send or receive, so IPC can always wake it. */
	p->thread = thread_current();
	arch_enter_user(USER_BASE, USER_BASE + USER_SIZE, p->arg);
}

struct user_proc *user_spawn(const char *name, const void *code, usize size, u64 arg, u32 perms)
{
	u32 npages = (u32)((size + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE);
	struct user_proc *p = kzalloc(sizeof(*p));
	u32 a = 0;

	if (!p || size == 0 || npages > USER_MAX_PAGES) {
		kfree(p);
		return NULL;
	}
	while (a < USER_MAX_PROCS && asid_used[a])
		a++;
	if (a == USER_MAX_PROCS) {
		kfree(p);
		return NULL;
	}
	asid_used[a] = true;
	p->asid = a + 1;
	p->arg = arg;
	p->perms = perms & PERM_ALL;
	p->space = arch_mmu_space_create(p->asid);

	const u8 *src = code;
	bool ok = p->space != 0;

	for (u32 i = 0; ok && i < npages; i++) {
		u8 *page = (u8 *)pmm_alloc_page();

		ok = page != NULL;
		if (!ok)
			break;
		/* Copy through the kernel's own mapping, zero-filling the rest of the page. */
		for (usize b = 0; b < PMM_PAGE_SIZE; b++)
			page[b] = i * PMM_PAGE_SIZE + b < size ? src[i * PMM_PAGE_SIZE + b] : 0;
		arch_sync_icache((uptr)page, PMM_PAGE_SIZE);
		p->code_pa[i] = (uptr)page;
		p->code_pages = i + 1;
		ok = arch_mmu_map_user(p->space, USER_BASE + i * PMM_PAGE_SIZE, (uptr)page,
				       ARCH_USER_CODE);
	}
	if (ok) {
		u64 *stack = (u64 *)pmm_alloc_page();

		ok = stack != NULL;
		for (usize w = 0; ok && w < PMM_PAGE_SIZE / sizeof(u64); w++)
			stack[w] = 0;
		p->stack_pa = (uptr)stack;
		ok = ok && arch_mmu_map_user(p->space, stack_va(), (uptr)stack, ARCH_USER_DATA);
	}

	/* IRQs masked: the program becomes findable only once its thread exists, and
	 * before that thread can run (and end). */
	unsigned long flags = arch_irq_save();

	p->pid = next_pid++;
	ok = ok && thread_create_user(name, user_thread, p, p, p->space);
	if (ok)
		procs[p->asid - 1] = p;
	arch_irq_restore(flags);
	if (!ok) {
		free_process(p);
		kfree(p);
		return NULL;
	}
	return p;
}

void user_release(struct user_proc *p)
{
	kfree(p);
}

/* End the running program with code: free its memory, then the thread. */
static void user_end(int code) ATTR_NORETURN;

static void user_end(int code)
{
	struct thread *t = thread_current();
	struct user_proc *p = t->user;

	/* Leave p's space before freeing it; kernel code and stack are mapped in every space. */
	unsigned long flags = arch_irq_save();

	/* No longer findable, and nobody may stay queued waiting to send to it. */
	procs[p->asid - 1] = NULL;
	ipc_exit(p);
	t->space = 0;
	arch_mmu_switch(0);
	arch_irq_restore(flags);
	free_process(p);
	p->exit_code = code;
	p->exited = true;
	thread_exit();
}

/* True if [va, va + len) lies inside memory p may read: its code or its stack. */
static bool user_range_ok(const struct user_proc *p, u64 va, u64 len)
{
	u64 code_end = USER_BASE + (u64)p->code_pages * PMM_PAGE_SIZE;
	u64 stack = stack_va();

	if (va + len < va)
		return false;
	return (va >= USER_BASE && va + len <= code_end) ||
	       (va >= stack && va + len <= stack + PMM_PAGE_SIZE);
}

/*
 * True if the running program p holds perm. A refusal is reported on the console, so
 * the user can see what a program tried to reach.
 */
static bool user_may(const struct user_proc *p, u32 perm)
{
	if (perm_has(p->perms, perm))
		return true;
	struct thread *t = thread_current();

	kprintf("perm: thread %u (%s) denied %s\n", t->id, t->name, perm_name(perm));
	return false;
}

long user_syscall(u64 nr, u64 *regs)
{
	struct user_proc *p = thread_current()->user;
	u64 a0 = regs[0], a1 = regs[1];

	switch (nr) {
	case SYS_WRITE:
		if (!user_may(p, PERM_CONSOLE))
			return -SYS_EPERM;
		if (!user_range_ok(p, a0, a1))
			return -SYS_EFAULT;
		console_write((const char *)(uptr)a0, (usize)a1);
		return (long)a1;
	case SYS_EXIT:
		user_end((int)a0);
	case SYS_YIELD:
		thread_yield();
		return 0;
	case SYS_PERM_GET:
		return (long)p->perms;
	case SYS_PERM_DROP:
		/* Only ever clears bits: there is no call that sets one. */
		p->perms &= ~(u32)a0;
		return (long)p->perms;
	case SYS_SEND:
		return ipc_send(p, a0, &regs[1]);
	case SYS_RECV:
		return ipc_recv(p, &regs[1]);
	case SYS_GETPID:
		return (long)p->pid;
	default:
		return -SYS_ENOSYS;
	}
}

void user_kill(const char *what, uptr pc, uptr addr)
{
	struct thread *t = thread_current();

	kprintf("user: thread %u (%s) killed: %s at pc %p, address %p\n", t->id, t->name, what,
		(void *)pc, (void *)addr);
	user_end(USER_EXIT_KILLED);
}

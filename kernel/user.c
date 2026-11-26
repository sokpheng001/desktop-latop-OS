/*
 * Project: OS-001
 * Date: 2026-11-26
 * Authors: kwan, oda, sokpheng001
 *
 * User programs and system calls. A program sees only its own window of user
 * addresses; every pointer it passes in is checked against that window before the
 * kernel touches it, and a program that faults is ended without harming the kernel
 * or other programs.
 */

#include "arch.h"
#include "console.h"
#include "heap.h"
#include "lib/kprintf.h"
#include "pmm.h"
#include "syscall.h"
#include "thread.h"
#include "user.h"

static bool slot_used[USER_SLOTS];

static uptr stack_va(const struct user_proc *p)
{
	return p->base + USER_SLOT_SIZE - PMM_PAGE_SIZE;
}

/* Unmap and free every page of p. Safe to call more than once. */
static void free_pages(struct user_proc *p)
{
	for (u32 i = 0; i < p->code_pages; i++) {
		arch_mmu_unmap_user(p->base + i * PMM_PAGE_SIZE);
		pmm_free_page(p->code_pa[i]);
	}
	p->code_pages = 0;
	if (p->stack_pa) {
		arch_mmu_unmap_user(stack_va(p));
		pmm_free_page(p->stack_pa);
		p->stack_pa = 0;
	}
	slot_used[p->slot] = false;
}

static void user_thread(void *arg)
{
	struct user_proc *p = arg;

	arch_enter_user(p->base, p->base + USER_SLOT_SIZE);
}

struct user_proc *user_spawn(const char *name, const void *code, usize size)
{
	u32 npages = (u32)((size + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE);
	struct user_proc *p = kzalloc(sizeof(*p));
	u32 slot = 0;

	if (!p || size == 0 || npages > USER_MAX_PAGES) {
		kfree(p);
		return NULL;
	}
	while (slot < USER_SLOTS && slot_used[slot])
		slot++;
	if (slot == USER_SLOTS) {
		kfree(p);
		return NULL;
	}
	slot_used[slot] = true;
	p->slot = slot;
	p->base = USER_BASE + slot * USER_SLOT_SIZE;

	const u8 *src = code;
	bool ok = true;

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
		ok = arch_mmu_map_user(p->base + i * PMM_PAGE_SIZE, (uptr)page, ARCH_USER_CODE);
	}
	if (ok) {
		u64 *stack = (u64 *)pmm_alloc_page();

		ok = stack != NULL;
		for (usize w = 0; ok && w < PMM_PAGE_SIZE / sizeof(u64); w++)
			stack[w] = 0;
		p->stack_pa = (uptr)stack;
		ok = ok && arch_mmu_map_user(stack_va(p), (uptr)stack, ARCH_USER_DATA);
	}

	struct thread *t = ok ? thread_create(name, user_thread, p) : NULL;

	if (!t) {
		free_pages(p);
		kfree(p);
		return NULL;
	}
	t->user = p;
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

	free_pages(p);
	p->exit_code = code;
	p->exited = true;
	thread_exit();
}

/* True if [va, va + len) lies inside memory p may read: its code or its stack. */
static bool user_range_ok(const struct user_proc *p, u64 va, u64 len)
{
	u64 code_end = p->base + (u64)p->code_pages * PMM_PAGE_SIZE;
	u64 stack = stack_va(p);

	if (va + len < va)
		return false;
	return (va >= p->base && va + len <= code_end) ||
	       (va >= stack && va + len <= stack + PMM_PAGE_SIZE);
}

long user_syscall(u64 nr, u64 a0, u64 a1, u64 a2)
{
	struct user_proc *p = thread_current()->user;

	(void)a2;
	switch (nr) {
	case SYS_WRITE:
		if (!user_range_ok(p, a0, a1))
			return -SYS_EFAULT;
		console_write((const char *)(uptr)a0, (usize)a1);
		return (long)a1;
	case SYS_EXIT:
		user_end((int)a0);
	case SYS_YIELD:
		thread_yield();
		return 0;
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

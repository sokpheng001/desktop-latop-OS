/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Kernel self-tests for paths that cannot be reached from a normal boot, such as
 * faults. tests/boot_test.sh builds each one and checks the console output
 * (see tests/README.md for the full list).
 */

#include "arch.h"
#include "compiler.h"
#include "hal.h"
#include "heap.h"
#include "irq.h"
#include "lib/kprintf.h"
#include "pmm.h"
#include "selftest.h"
#include "thread.h"
#include "time.h"
#include "user.h"

#if defined(KERNEL_TEST_IRQ)
/* Software-generated interrupts used by the test; any of 0-15 would do. */
#define SELFTEST_SGI	       1u
/*
 * SGIs may be impossible to disable (GICv2 leaves that IMPLEMENTATION DEFINED, and
 * QEMU keeps them enabled), so the no-handler case uses the highest shared
 * interrupt, set pending from software. No device on either board uses it.
 */
#define SELFTEST_SPI_NOHANDLER (hal_irq_count() - 1)
/* Timer ticks to wait for an interrupt that must not arrive. */
#define SELFTEST_QUIET_TICKS   5u
/* WFI wake-ups to wait before giving up (a spurious wake-up is allowed). */
#define SELFTEST_IRQ_TRIES     1000

static volatile u32 irq_seen;

static void selftest_irq_handler(u32 irq, void *ctx)
{
	(void)ctx;
	irq_seen = irq + 1;
}
#endif

#if defined(KERNEL_TEST_ABORT)
/*
 * Inside the 39-bit address space but in no RAM or device range on any board, so with
 * the MMU on it has no level 1 entry: a load must end in a translation fault at level 1.
 */
#define SELFTEST_UNMAPPED_ADDR 0x7f00000000ul
#endif

#if defined(KERNEL_TEST_RODATA)
/* A kernel constant: mapped read-only, so writing it must fault. */
static const u32 selftest_const = 1;
#endif

#if defined(KERNEL_TEST_NX)
/* A RET instruction (A64 encoding 0xd65f03c0) placed in writable data, never executable. */
static u32 selftest_code[1] = {0xd65f03c0u};
#endif

#if defined(KERNEL_TEST_TIMER)
/* Ticks to wait for: half a second at 100 Hz. */
#define SELFTEST_TICKS	       50u
/* Allowed difference between tick time and counter time, in percent. */
#define SELFTEST_TOLERANCE_PCT 10u
#endif

#if defined(KERNEL_TEST_THREAD)
#define SELFTEST_THREADS 3u
#define SELFTEST_ROUNDS	 3u

static char thread_log[SELFTEST_THREADS * SELFTEST_ROUNDS + 1];
static usize thread_log_len;
static u32 thread_bad;

/*
 * Each worker logs its letter once per turn. Its locals live in callee-saved registers
 * or on its own stack across every yield, so a wrong value means a broken switch.
 */
static void selftest_worker(void *arg)
{
	uptr id = (uptr)arg;
	u64 a = id * 1000u + 1u;
	u64 b = id * 7u + 3u;
	u64 sum = 0;

	for (u32 r = 0; r < SELFTEST_ROUNDS; r++) {
		if (thread_log_len < sizeof(thread_log) - 1)
			thread_log[thread_log_len++] = (char)('A' + id);
		sum += a * b + r;
		thread_yield();
	}
	if (sum != (id * 1000u + 1u) * (id * 7u + 3u) * SELFTEST_ROUNDS + 3u)
		thread_bad++;
}

/* Main starts three workers and yields until they have all finished and been freed. */
static void selftest_thread(void)
{
	usize live0 = heap_live_count();
	bool ok = true;

	for (uptr i = 0; i < SELFTEST_THREADS; i++)
		ok = ok && thread_create("worker", selftest_worker, (void *)i) != NULL;
	ok = ok && thread_count() == SELFTEST_THREADS + 1;
	for (u32 spins = 0; ok && thread_count() > 1 && spins < 100; spins++)
		thread_yield();
	thread_log[thread_log_len] = '\0';

	const char *expect = "ABCABCABC";

	for (usize i = 0; ok && i <= SELFTEST_THREADS * SELFTEST_ROUNDS; i++)
		ok = thread_log[i] == expect[i];
	ok = ok && thread_bad == 0 && thread_count() == 1 && heap_live_count() == live0 &&
	     thread_current()->id == 0;

	if (ok)
		kprintf("Self-test: threads OK (order %s, stacks freed)\n", thread_log);
	else
		kprintf("Self-test: threads FAILED (order %s, bad=%u, alive=%lu)\n", thread_log,
			thread_bad, (unsigned long)thread_count());
}
#endif

#if defined(KERNEL_TEST_USER)
/* User programs from arch/aarch64/user_selftest.S. */
extern const char user_hello_start[], user_hello_end[];
extern const char user_bad_start[], user_bad_end[];
extern const char user_peek_start[], user_peek_end[];
extern const char user_peek_addr_offset[]; /* an offset, not an address */
extern char kernel_start[];

#define SELFTEST_USER_EXIT 7

/* Run the three programs to the end; true if each ended the way it must. */
static bool user_round(void)
{
	/* The peek program reads the u64 at its offset as the address to load from. */
	usize peek_size = (usize)(user_peek_end - user_peek_start);
	u8 *peek = kmalloc(peek_size);

	if (!peek)
		return false;
	for (usize i = 0; i < peek_size; i++)
		peek[i] = (u8)user_peek_start[i];
	*(u64 *)(peek + (uptr)user_peek_addr_offset) = (u64)(uptr)kernel_start;

	struct user_proc *hello =
	    user_spawn("hello", user_hello_start, (usize)(user_hello_end - user_hello_start));
	struct user_proc *bad =
	    user_spawn("bad", user_bad_start, (usize)(user_bad_end - user_bad_start));
	struct user_proc *snoop = user_spawn("peek", peek, peek_size);
	bool ok = hello && bad && snoop;

	for (u32 spins = 0; ok && thread_count() > 1 && spins < 1000; spins++)
		thread_yield();
	ok = ok && thread_count() == 1 && hello->exited && hello->exit_code == SELFTEST_USER_EXIT &&
	     bad->exited && bad->exit_code == USER_EXIT_KILLED && snoop->exited &&
	     snoop->exit_code == USER_EXIT_KILLED;
	if (hello)
		user_release(hello);
	if (bad)
		user_release(bad);
	if (snoop)
		user_release(snoop);
	kfree(peek);
	return ok;
}

/*
 * Two rounds: the first also creates the page tables for the user windows, which
 * stay; the second must then leave memory exactly as it found it.
 */
static void selftest_user(void)
{
	usize live0 = heap_live_count();
	bool ok = user_round();
	usize free1 = pmm_free_count();

	ok = ok && user_round();
	ok = ok && pmm_free_count() == free1 && heap_live_count() == live0;
	if (ok)
		kprintf("Self-test: user mode OK (exit code %d, two programs killed, no leaks)\n",
			SELFTEST_USER_EXIT);
	else
		kprintf("Self-test: user mode FAILED (free %lu -> %lu, alive=%lu)\n",
			(unsigned long)free1, (unsigned long)pmm_free_count(),
			(unsigned long)thread_count());
}
#endif

#if defined(KERNEL_TEST_SCHED)
#define SELFTEST_SPINNERS    3u
#define SELFTEST_SCHED_TICKS 100u /* one second at 100 Hz */
/* Fewest turns each spinner must get: four threads share 50 slices of 2 ticks. */
#define SELFTEST_MIN_TURNS   4u

static volatile bool sched_stop;
static volatile u64 spin_count[SELFTEST_SPINNERS];
static struct thread *spinners[SELFTEST_SPINNERS];

/* Never yields: only the timer can take the CPU away from it. */
static void selftest_spinner(void *arg)
{
	uptr id = (uptr)arg;

	while (!sched_stop)
		spin_count[id]++;
}

static void selftest_sched(void)
{
	usize live0 = heap_live_count();
	bool ok = true;

	for (uptr i = 0; i < SELFTEST_SPINNERS; i++) {
		spinners[i] = thread_create("spinner", selftest_spinner, (void *)i);
		ok = ok && spinners[i];
	}

	/* Main only sleeps; it gets the CPU back on its own turns and counts ticks. */
	u64 t0 = time_ticks();

	while (ok && time_ticks() - t0 < SELFTEST_SCHED_TICKS)
		arch_wait_for_interrupt();

	u64 turns[SELFTEST_SPINNERS];

	for (u32 i = 0; i < SELFTEST_SPINNERS; i++) {
		turns[i] = spinners[i] ? spinners[i]->runs : 0;
		ok = ok && spin_count[i] > 0 && turns[i] >= SELFTEST_MIN_TURNS;
	}
	sched_stop = true;
	for (u32 spins = 0; thread_count() > 1 && spins < 100; spins++)
		thread_yield();
	ok = ok && thread_count() == 1 && heap_live_count() == live0;

	if (ok)
		kprintf("Self-test: preemptive scheduler OK (turns %lu %lu %lu in %u ticks)\n",
			(unsigned long)turns[0], (unsigned long)turns[1], (unsigned long)turns[2],
			SELFTEST_SCHED_TICKS);
	else
		kprintf("Self-test: scheduler FAILED (turns %lu %lu %lu, alive=%lu)\n",
			(unsigned long)turns[0], (unsigned long)turns[1], (unsigned long)turns[2],
			(unsigned long)thread_count());
}
#endif

#if defined(KERNEL_TEST_HEAP)
/* Objects per round; sizes cycle through every class and into whole pages. */
#define SELFTEST_HEAP_OBJS 512u
#define SELFTEST_HEAP_BIG  (3u * PMM_PAGE_SIZE)

static void *heap_objs[SELFTEST_HEAP_OBJS];

static usize heap_size(usize i)
{
	static const usize sizes[] = {1, 8, 16, 17, 48, 100, 200, 500, 1000, 2000, 2032, 5000};

	return sizes[i % (sizeof(sizes) / sizeof(sizes[0]))];
}

/* Fill object i with a pattern unique to it, or check that the pattern is intact. */
static bool heap_pattern(usize i, bool check)
{
	volatile u8 *p = heap_objs[i];

	for (usize b = 0; b < heap_size(i); b++) {
		u8 v = (u8)(i * 31u + b);

		if (!check)
			p[b] = v;
		else if (p[b] != v)
			return false;
	}
	return true;
}

/*
 * Two rounds of allocate, fill, check, free. Overlapping objects would break a
 * pattern; the second round must reuse the first round's pages; and every page and
 * object must be accounted for at the end.
 */
static void selftest_heap(void)
{
	usize free0 = pmm_free_count();
	usize pages_after_first = 0;
	bool ok = true;

	for (int round = 0; round < 2 && ok; round++) {
		for (usize i = 0; i < SELFTEST_HEAP_OBJS && ok; i++) {
			heap_objs[i] = kmalloc(heap_size(i));
			ok = heap_objs[i] && (uptr)heap_objs[i] % HEAP_ALIGN == 0;
			if (ok)
				heap_pattern(i, false);
		}
		for (usize i = 0; i < SELFTEST_HEAP_OBJS && ok; i++)
			ok = heap_pattern(i, true);
		ok = ok && heap_live_count() == SELFTEST_HEAP_OBJS;
		for (usize i = 0; i < SELFTEST_HEAP_OBJS && ok; i++)
			kfree(heap_objs[i]);
		ok = ok && heap_live_count() == 0 && heap_bad_frees() == 0;
		if (round == 0)
			pages_after_first = heap_page_count();
	}
	ok = ok && heap_page_count() == pages_after_first &&
	     pmm_free_count() == free0 - pages_after_first;

	/* Zeroed memory, a multi-page object, and refused bad frees. */
	u64 *z = kzalloc(SELFTEST_HEAP_BIG);

	for (usize i = 0; ok && i < SELFTEST_HEAP_BIG / sizeof(u64); i++)
		ok = z[i] == 0;
	kfree(z);
	ok = ok && pmm_free_count() == free0 - pages_after_first;

	void *p = kmalloc(64);

	kfree(p);
	kfree(p);
	kfree((u8 *)p + 1);
	ok = ok && heap_bad_frees() == 2 && kmalloc(0) == NULL;

	if (ok)
		kprintf("Self-test: heap OK (%u objects twice, %lu pages reused)\n",
			SELFTEST_HEAP_OBJS, (unsigned long)pages_after_first);
	else
		kprintf("Self-test: heap FAILED (live=%lu pages=%lu bad_frees=%lu)\n",
			(unsigned long)heap_live_count(), (unsigned long)heap_page_count(),
			(unsigned long)heap_bad_frees());
}
#endif

#if defined(KERNEL_TEST_PMM)
/* Contiguous run size for the multi-page check. */
#define SELFTEST_RUN_PAGES 16u
/* Mixed into each page's second word, so a page handed out twice is caught. */
#define SELFTEST_PAGE_TAG  0x05001a11c0de0001ul

extern char kernel_start[];
extern char kernel_end[];

static bool page_ok(uptr p)
{
	return p && !(p & (PMM_PAGE_SIZE - 1)) &&
	       (p + PMM_PAGE_SIZE <= (uptr)kernel_start || p >= (uptr)kernel_end);
}

/*
 * Take every free page, write to it, then give them all back. Each page holds the
 * address of the previous one (a chain needing no other memory) and a tag derived
 * from its own address; a page given out twice breaks the chain or the tag.
 */
static void selftest_pmm(void)
{
	usize free0 = pmm_free_count();
	usize taken = 0;
	uptr head = 0;
	bool ok = free0 > 0;

	for (uptr p; ok && (p = pmm_alloc_page()) != 0; taken++) {
		volatile u64 *w = (volatile u64 *)p;

		ok = page_ok(p);
		w[0] = head;
		w[1] = p ^ SELFTEST_PAGE_TAG;
		head = p;
	}
	ok = ok && taken == free0 && pmm_free_count() == 0 && pmm_alloc_page() == 0;

	usize given = 0;

	while (head && given < taken) {
		volatile u64 *w = (volatile u64 *)head;
		uptr prev = (uptr)w[0];

		ok = ok && w[1] == (head ^ SELFTEST_PAGE_TAG) && pmm_free_page(head);
		head = prev;
		given++;
	}
	ok = ok && head == 0 && given == taken && pmm_free_count() == free0;

	/* A second free of the same page and a misaligned free must be refused. */
	uptr one = pmm_alloc_page();

	ok = ok && page_ok(one) && pmm_free_page(one) && !pmm_free_page(one) &&
	     !pmm_free_page(one + 8);

	uptr run = pmm_alloc_pages(SELFTEST_RUN_PAGES);

	for (usize i = 0; ok && i < SELFTEST_RUN_PAGES; i++)
		ok = page_ok(run + i * PMM_PAGE_SIZE);
	ok = ok && pmm_free_pages(run, SELFTEST_RUN_PAGES) && pmm_free_count() == free0;

	if (ok)
		kprintf("Self-test: page allocator OK (%lu pages taken and returned)\n",
			(unsigned long)taken);
	else
		kprintf("Self-test: page allocator FAILED (taken=%lu of %lu, returned=%lu)\n",
			(unsigned long)taken, (unsigned long)free0, (unsigned long)given);
}
#endif

void selftest_run(void)
{
#if defined(KERNEL_TEST_PMM)
	selftest_pmm();
#endif
#if defined(KERNEL_TEST_HEAP)
	selftest_heap();
#endif
#if defined(KERNEL_TEST_THREAD)
	selftest_thread();
#endif
#if defined(KERNEL_TEST_SCHED)
	selftest_sched();
#endif
#if defined(KERNEL_TEST_USER)
	selftest_user();
#endif
#if defined(KERNEL_TEST_FAULT)
	/* Must end in the exception handler's report, never return here. */
	kprintf("Self-test: triggering a breakpoint exception\n");
	TRAP();
#endif
#if defined(KERNEL_TEST_ABORT)
	/* Must end in a data abort report whose FAR is this address. */
	volatile u32 *unmapped = (volatile u32 *)SELFTEST_UNMAPPED_ADDR;

	kprintf("Self-test: load from unmapped memory at %p\n", (void *)unmapped);
	(void)*unmapped;
	kprintf("Self-test: abort FAILED (the load did not fault)\n");
#endif
#if defined(KERNEL_TEST_RODATA)
	/* Must end in a data abort, permission fault, whose FAR is this address. */
	uptr ro = (uptr)&selftest_const;

	HIDE_VALUE(ro);
	kprintf("Self-test: write to read-only data at %p\n", (void *)ro);
	*(volatile u32 *)ro = 2;
	kprintf("Self-test: rodata FAILED (the write did not fault)\n");
#endif
#if defined(KERNEL_TEST_NX)
	/* Must end in an instruction abort, permission fault, whose FAR is this address. */
	uptr code = (uptr)selftest_code;

	HIDE_VALUE(code);
	kprintf("Self-test: jump to data at %p\n", (void *)code);
	((void (*)(void))code)();
	kprintf("Self-test: nx FAILED (data ran as code)\n");
#endif
#if defined(KERNEL_TEST_NULL)
	/* Page 0 is unmapped: must end in a translation fault whose FAR is 0. */
	uptr null = 0;

	HIDE_VALUE(null);
	kprintf("Self-test: load from NULL at %p\n", (void *)null);
	(void)*(volatile u32 *)null;
	kprintf("Self-test: null FAILED (the load did not fault)\n");
#endif
#if defined(KERNEL_TEST_IRQ)
	/* Exercises the GIC, the IRQ vector, dispatch and the return to interrupted code. */
	irq_register(SELFTEST_SGI, selftest_irq_handler, NULL);
	hal_irq_raise(SELFTEST_SGI);
	for (int i = 0; i < SELFTEST_IRQ_TRIES && !irq_seen; i++)
		arch_wait_for_interrupt();
	if (irq_seen == SELFTEST_SGI + 1)
		kprintf("Self-test: IRQ %u received and returned\n", SELFTEST_SGI);
	else
		kprintf("Self-test: IRQ FAILED (seen=%u)\n", irq_seen);

	/* An interrupt with no handler is reported and disabled, so raising it again is silent. */
	u32 spi = SELFTEST_SPI_NOHANDLER;

	hal_irq_enable(spi);
	hal_irq_raise(spi);
	u64 t0 = time_ticks();
	while (time_ticks() - t0 < SELFTEST_QUIET_TICKS)
		arch_wait_for_interrupt();
	hal_irq_raise(spi);
	t0 = time_ticks();
	while (time_ticks() - t0 < SELFTEST_QUIET_TICKS)
		arch_wait_for_interrupt();
	if (irq_unhandled_count() == 1)
		kprintf("Self-test: unhandled IRQ %u reported once and disabled\n", spi);
	else
		kprintf("Self-test: unhandled IRQ FAILED (count=%lu)\n",
			(unsigned long)irq_unhandled_count());
#endif
#if defined(KERNEL_TEST_TIMER)
	/* Ticks must arrive, and at the right rate measured against the hardware counter. */
	u64 start_ticks = time_ticks();
	u64 start_ms = time_uptime_ms();

	while (time_ticks() - start_ticks < SELFTEST_TICKS)
		arch_wait_for_interrupt();

	u64 elapsed_ms = time_uptime_ms() - start_ms;
	u64 expect_ms = SELFTEST_TICKS * 1000u / TIME_TICK_HZ;
	u64 slack_ms = expect_ms * SELFTEST_TOLERANCE_PCT / 100u;

	if (elapsed_ms + slack_ms >= expect_ms && elapsed_ms <= expect_ms + slack_ms)
		kprintf("Self-test: timer OK (%u ticks in %lu ms)\n", SELFTEST_TICKS,
			(unsigned long)elapsed_ms);
	else
		kprintf("Self-test: timer FAILED (%u ticks in %lu ms, expected %lu ms)\n",
			SELFTEST_TICKS, (unsigned long)elapsed_ms, (unsigned long)expect_ms);
#endif
}

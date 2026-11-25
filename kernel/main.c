/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * kernel_main: architecture-independent kernel entry after early boot.
 */

#include "arch.h"
#include "console.h"
#include "hal.h"
#include "kernel.h"
#include "lib/kprintf.h"
#include "mem.h"
#include "selftest.h"
#include "time.h"
#include "version.h"

/* Image bounds from the linker script (arch/aarch64/kernel.ld). */
extern char kernel_start[];
extern char kernel_end[];

void kernel_main(uptr dtb)
{
	/* First, so any fault from here on is reported instead of hanging silently. */
	arch_exception_init();
	hal_init();
	console_init();

	kprintf("\n%s %s\n", OS001_NAME, OS001_VERSION);
	kprintf("Board:  %s\n", hal_board_name());
	kprintf("EL:     %u\n", arch_current_el());
	kprintf("Kernel: %p - %p\n", (void *)kernel_start, (void *)kernel_end);
	if (dtb)
		kprintf("DTB:    %p\n", (void *)dtb);
	else
		kprintf("DTB:    not passed by the loader\n");
	mem_init(dtb);
	kprintf("Hello from OS-001!\n");

	/* Each source is still disabled at the controller until a driver registers it. */
	time_init();
	arch_irq_enable();
	kprintf("Timer:  %u Hz tick, counter %lu Hz, uptime %lu ms\n", TIME_TICK_HZ,
		(unsigned long)hal_counter_freq(), (unsigned long)time_uptime_ms());

	selftest_run();

	/* Nothing else to do yet: sleep until the next interrupt, forever. */
	for (;;)
		arch_wait_for_interrupt();
}

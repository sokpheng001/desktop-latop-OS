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
#include "version.h"

/* Image bounds from the linker script (arch/aarch64/kernel.ld). */
extern char kernel_start[];
extern char kernel_end[];

void kernel_main(uptr dtb)
{
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
	kprintf("Hello from OS-001!\n");
}

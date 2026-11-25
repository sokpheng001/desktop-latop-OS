/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Kernel console: connects kprintf to hal_console_putc.
 */

#include "console.h"
#include "hal.h"
#include "lib/kprintf.h"

/* Serial terminals need CR+LF to start a new line; kernel code just writes '\n'. */
static void console_sink(char c, void *ctx)
{
	(void)ctx;
	if (c == '\n')
		hal_console_putc('\r');
	hal_console_putc(c);
}

void console_init(void)
{
	kprintf_set_output(console_sink, NULL);
}

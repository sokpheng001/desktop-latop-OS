/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * HAL implementation for the QEMU virt board.
 */

#include "board.h"
#include "drivers/pl011.h"
#include "hal.h"

#define CONSOLE_BAUD 115200u

static const struct pl011 console = {
    .base = BOARD_UART_BASE,
    .clock_hz = BOARD_UART_CLOCK_HZ,
};

void hal_init(void)
{
	pl011_init(&console, CONSOLE_BAUD);
}

void hal_console_putc(char c)
{
	pl011_putc(&console, c);
}

int hal_console_getc(void)
{
	return pl011_getc(&console);
}

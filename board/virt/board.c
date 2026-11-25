/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * HAL implementation for the QEMU virt board.
 */

#include "board.h"
#include "drivers/gicv2.h"
#include "drivers/pl011.h"
#include "hal.h"

#define CONSOLE_BAUD 115200u

static const struct gicv2 gic = {
    .dist_base = BOARD_GICD_BASE,
    .cpu_base = BOARD_GICC_BASE,
};

static const struct pl011 console = {
    .base = BOARD_UART_BASE,
    .clock_hz = BOARD_UART_CLOCK_HZ,
};

void hal_init(void)
{
	pl011_init(&console, CONSOLE_BAUD);
	gicv2_init(&gic);
}

void hal_console_putc(char c)
{
	pl011_putc(&console, c);
}

int hal_console_getc(void)
{
	return pl011_getc(&console);
}

const char *hal_board_name(void)
{
	return BOARD_NAME;
}

void hal_irq_enable(u32 irq)
{
	gicv2_enable(&gic, irq);
}

void hal_irq_disable(u32 irq)
{
	gicv2_disable(&gic, irq);
}

int hal_irq_claim(u32 *token)
{
	u32 iar = gicv2_ack(&gic);
	u32 irq = gicv2_iar_irq(iar);

	if (irq == GICV2_SPURIOUS_ID)
		return -1;
	*token = iar;
	return (int)irq;
}

void hal_irq_complete(u32 token)
{
	gicv2_eoi(&gic, token);
}

void hal_irq_raise_soft(u32 sgi)
{
	gicv2_send_sgi_self(&gic, sgi);
}

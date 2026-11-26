/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * HAL implementation for the QEMU virt board.
 */

#include "board.h"
#include "compiler.h"
#include "drivers/arm_gtimer.h"
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

static const struct hal_mmio_range mmio_ranges[] = {
    {BOARD_MMIO_BASE, BOARD_MMIO_SIZE},
};

int hal_mmio_ranges(const struct hal_mmio_range **ranges)
{
	*ranges = mmio_ranges;
	return (int)ARRAY_SIZE(mmio_ranges);
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

void hal_irq_raise(u32 irq)
{
	if (irq < GICV2_NUM_SGIS)
		gicv2_send_sgi_self(&gic, irq);
	else
		gicv2_set_pending(&gic, irq);
}

u32 hal_irq_count(void)
{
	return gicv2_num_irqs(&gic);
}

u32 hal_timer_irq(void)
{
	return BOARD_TIMER_IRQ;
}

void hal_timer_start(u32 hz)
{
	arm_gtimer_start(hz);
}

void hal_timer_ack(void)
{
	arm_gtimer_rearm();
}

u64 hal_counter(void)
{
	return arm_gtimer_count();
}

u64 hal_counter_freq(void)
{
	return arm_gtimer_freq();
}

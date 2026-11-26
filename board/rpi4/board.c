/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * HAL implementation for the Raspberry Pi 4. The console is PL011 UART0 on
 * GPIO 14 (TXD0) and GPIO 15 (RXD0), header pins 8 and 10.
 */

#include "board.h"
#include "compiler.h"
#include "drivers/arm_gtimer.h"
#include "drivers/gicv2.h"
#include "drivers/pl011.h"
#include "hal.h"
#include "mmio.h"

#define CONSOLE_BAUD 115200u

/*
 * GPIO registers (BCM2711 ARM Peripherals, chapter 5, "Register View").
 * GPFSEL1 holds the 3-bit function select fields for GPIO 10-19.
 * GPIO_PUP_PDN_CNTRL_REG0 holds 2-bit pull-up/down fields for GPIO 0-15.
 */
#define GPIO_GPFSEL1	   0x04
#define GPIO_PUP_PDN_CNTRL 0xe4

#define GPIO_UART_TX 14
#define GPIO_UART_RX 15

#define GPFSEL_BITS    3
#define GPFSEL_MASK    0x7u
#define GPFSEL_ALT0    0x4u /* function 0b100 = alternate function 0 (TXD0/RXD0) */
#define GPFSEL_PER_REG 10
#define GPIO_PULL_BITS 2
#define GPIO_PULL_MASK 0x3u
#define GPIO_PULL_NONE 0x0u

static const struct gicv2 gic = {
    .dist_base = BOARD_GICD_BASE,
    .cpu_base = BOARD_GICC_BASE,
};

static const struct pl011 console = {
    .base = BOARD_UART_BASE,
    .clock_hz = BOARD_UART_CLOCK_HZ,
};

/* Route GPIO 14/15 to UART0 with no pull resistors, whatever the firmware left there. */
static void board_uart_pins(void)
{
	uptr fsel = BOARD_GPIO_BASE + GPIO_GPFSEL1;
	u32 val = mmio_read32(fsel);
	u32 tx_shift = (GPIO_UART_TX % GPFSEL_PER_REG) * GPFSEL_BITS;
	u32 rx_shift = (GPIO_UART_RX % GPFSEL_PER_REG) * GPFSEL_BITS;

	val &= ~((GPFSEL_MASK << tx_shift) | (GPFSEL_MASK << rx_shift));
	val |= (GPFSEL_ALT0 << tx_shift) | (GPFSEL_ALT0 << rx_shift);
	mmio_write32(fsel, val);

	uptr pull = BOARD_GPIO_BASE + GPIO_PUP_PDN_CNTRL;
	val = mmio_read32(pull);
	val &= ~((GPIO_PULL_MASK << (GPIO_UART_TX * GPIO_PULL_BITS)) |
		 (GPIO_PULL_MASK << (GPIO_UART_RX * GPIO_PULL_BITS)));
	val |= (GPIO_PULL_NONE << (GPIO_UART_TX * GPIO_PULL_BITS)) |
	       (GPIO_PULL_NONE << (GPIO_UART_RX * GPIO_PULL_BITS));
	mmio_write32(pull, val);
}

void hal_init(void)
{
	board_uart_pins();
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

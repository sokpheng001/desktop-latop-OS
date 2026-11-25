/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * HAL implementation for the Raspberry Pi 4. The console is PL011 UART0 on
 * GPIO 14 (TXD0) and GPIO 15 (RXD0), header pins 8 and 10.
 */

#include "board.h"
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

/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Arm PrimeCell PL011 UART: polled 8N1 console driver.
 * Register map and bits from the PL011 Technical Reference Manual (Arm DDI 0183), chapter 3.
 */

#include "drivers/pl011.h"
#include "mmio.h"

/* Register offsets (DDI 0183, chapter 3). */
#define PL011_DR    0x000 /* Data register */
#define PL011_FR    0x018 /* Flag register */
#define PL011_IBRD  0x024 /* Integer baud rate divisor */
#define PL011_FBRD  0x028 /* Fractional baud rate divisor */
#define PL011_LCR_H 0x02c /* Line control */
#define PL011_CR    0x030 /* Control */
#define PL011_IMSC  0x038 /* Interrupt mask set/clear */
#define PL011_ICR   0x044 /* Interrupt clear */

/* UARTFR bits. */
#define PL011_FR_BUSY (1u << 3)
#define PL011_FR_RXFE (1u << 4) /* Receive FIFO empty */
#define PL011_FR_TXFF (1u << 5) /* Transmit FIFO full */

/* UARTLCR_H bits. */
#define PL011_LCR_H_FEN	   (1u << 4) /* Enable FIFOs */
#define PL011_LCR_H_WLEN_8 (3u << 5) /* 8 data bits */

/* UARTCR bits. */
#define PL011_CR_UARTEN (1u << 0)
#define PL011_CR_TXE	(1u << 8)
#define PL011_CR_RXE	(1u << 9)

/* UARTICR: writing 1 to bits [10:0] clears every interrupt. */
#define PL011_ICR_ALL 0x7ffu

/* UARTDR bits [7:0] hold the received byte; [11:8] are error flags. */
#define PL011_DR_DATA_MASK 0xffu

/* The fractional divisor has 6 bits: it counts in 1/64ths. */
#define PL011_FBRD_BITS 6
#define PL011_FBRD_MASK ((1u << PL011_FBRD_BITS) - 1)

void pl011_init(const struct pl011 *uart, u32 baud)
{
	/*
	 * Baud divisor = UARTCLK / (16 * baud), stored as 16.6 fixed point.
	 * Scaled by 64 that is UARTCLK * 4 / baud; adding baud / 2 rounds to nearest.
	 */
	u64 div64 = ((u64)uart->clock_hz * 4 + baud / 2) / baud;

	/* The TRM requires the UART to be disabled and idle while it is reprogrammed. */
	mmio_write32(uart->base + PL011_CR, 0);
	while (mmio_read32(uart->base + PL011_FR) & PL011_FR_BUSY) {
	}

	/* Clearing FEN flushes the transmit FIFO. */
	mmio_write32(uart->base + PL011_LCR_H, 0);
	mmio_write32(uart->base + PL011_ICR, PL011_ICR_ALL);
	mmio_write32(uart->base + PL011_IMSC, 0); /* polled: all interrupts masked */

	mmio_write32(uart->base + PL011_IBRD, (u32)(div64 >> PL011_FBRD_BITS));
	mmio_write32(uart->base + PL011_FBRD, (u32)(div64 & PL011_FBRD_MASK));

	/* The divisors only take effect on a write to UARTLCR_H, so it comes last. */
	mmio_write32(uart->base + PL011_LCR_H, PL011_LCR_H_WLEN_8 | PL011_LCR_H_FEN);
	mmio_write32(uart->base + PL011_CR, PL011_CR_UARTEN | PL011_CR_TXE | PL011_CR_RXE);
}

void pl011_putc(const struct pl011 *uart, char c)
{
	while (mmio_read32(uart->base + PL011_FR) & PL011_FR_TXFF) {
	}
	mmio_write32(uart->base + PL011_DR, (u8)c);
}

int pl011_getc(const struct pl011 *uart)
{
	if (mmio_read32(uart->base + PL011_FR) & PL011_FR_RXFE)
		return -1;
	return (int)(mmio_read32(uart->base + PL011_DR) & PL011_DR_DATA_MASK);
}

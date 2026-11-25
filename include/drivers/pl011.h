/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Arm PrimeCell PL011 UART driver interface.
 */

#ifndef OS001_DRIVERS_PL011_H
#define OS001_DRIVERS_PL011_H

#include "types.h"

#ifndef __ASSEMBLY__

struct pl011 {
	uptr base;    /* MMIO base address of the UART registers */
	u32 clock_hz; /* UARTCLK reference clock, used to compute the baud divisor */
};

void pl011_init(const struct pl011 *uart, u32 baud);
void pl011_putc(const struct pl011 *uart, char c);
int pl011_getc(const struct pl011 *uart);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_DRIVERS_PL011_H */

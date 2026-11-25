/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Raspberry Pi 4 (BCM2711) board constants, for the "low peripheral" address map
 * the firmware uses by default (BCM2711 ARM Peripherals, chapter 1).
 */

#ifndef OS001_BOARD_H
#define OS001_BOARD_H

#define BOARD_NAME "raspberry-pi-4"

/* Main peripherals start at 0xFE000000 as seen by the ARM cores. */
#define BOARD_PERIPH_BASE 0xfe000000ul

/* GPIO controller (BCM2711 ARM Peripherals, chapter 5). */
#define BOARD_GPIO_BASE (BOARD_PERIPH_BASE + 0x200000ul)

/* PL011 UART0 (BCM2711 ARM Peripherals, chapter 11). */
#define BOARD_UART_BASE	    (BOARD_PERIPH_BASE + 0x201000ul)
/* UART reference clock the Pi 4 firmware sets by default (48 MHz). */
#define BOARD_UART_CLOCK_HZ 48000000u

/* GIC-400 (BCM2711 ARM Peripherals, chapter 6): distributor and CPU interface. */
#define BOARD_GIC_BASE	0xff840000ul
#define BOARD_GICD_BASE (BOARD_GIC_BASE + 0x1000ul)
#define BOARD_GICC_BASE (BOARD_GIC_BASE + 0x2000ul)

#endif /* OS001_BOARD_H */

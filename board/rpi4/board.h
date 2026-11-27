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

/*
 * Device registers: the low-peripheral window 0xFC000000-0xFFFFFFFF holds the main
 * peripherals and the ARM local block with the GIC (BCM2711 ARM Peripherals, 1.2.4).
 */
#define BOARD_MMIO_BASE 0xfc000000ul
#define BOARD_MMIO_SIZE 0x04000000ul

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

/*
 * EL1 non-secure physical timer: PPI 14, GIC interrupt ID 30. This assignment is
 * the one Arm recommends for the generic timer (Arm Server Base System Architecture)
 * and the one the board's Device Tree uses.
 */
#define BOARD_TIMER_IRQ 30u

/*
 * SD host controllers (BCM2711 ARM Peripherals, "EMMC"): EMMC2 drives the SD card slot
 * on the Pi 4; the older EMMC (Arasan) controller is tried too, since QEMU's raspi4b
 * model has both.
 */
#define BOARD_EMMC2_BASE (BOARD_PERIPH_BASE + 0x340000ul)
#define BOARD_EMMC_BASE	 (BOARD_PERIPH_BASE + 0x300000ul)

#endif /* OS001_BOARD_H */

/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * QEMU virt board constants. Addresses match the Device Tree QEMU generates
 * for the virt machine (node /pl011@9000000).
 */

#ifndef OS001_BOARD_H
#define OS001_BOARD_H

#define BOARD_NAME "qemu-virt"

/* PL011 UART0 */
#define BOARD_UART_BASE	    0x09000000ul
#define BOARD_UART_CLOCK_HZ 24000000u /* apb-pclk "clk24mhz" in QEMU's Device Tree */

/* GICv2 (QEMU virt default, gic-version=2): node /intc@8000000 in QEMU's Device Tree. */
#define BOARD_GICD_BASE 0x08000000ul
#define BOARD_GICC_BASE 0x08010000ul

/*
 * EL1 non-secure physical timer: PPI 14, GIC interrupt ID 30. This assignment is
 * the one Arm recommends for the generic timer (Arm Server Base System Architecture)
 * and the one the board's Device Tree uses.
 */
#define BOARD_TIMER_IRQ 30u

#endif /* OS001_BOARD_H */

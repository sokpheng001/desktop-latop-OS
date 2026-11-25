/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Linker script for the rpi4 board.
 * The Raspberry Pi 4 firmware (and QEMU `raspi4b`) loads a 64-bit kernel8.img at 0x80000.
 */

KERNEL_BASE = 0x80000;

INCLUDE kernel.ld

/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Linker script for the virt board.
 * QEMU `virt` loads the ELF here; RAM starts at 0x40000000 and QEMU places the DTB at its start.
 */

KERNEL_BASE = 0x40080000;

INCLUDE kernel.ld

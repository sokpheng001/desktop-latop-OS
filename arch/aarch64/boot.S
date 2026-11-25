/*
 * Project: OS-001
 * Authors: kwan, oda, sokpheng001
 *
 * Kernel entry point. Runs first on every core the firmware starts: parks all
 * secondary cores, sets up the boot stack, clears .bss and calls kernel_main.
 */

/* MPIDR_EL1.Aff0, bits [7:0]: core number within the cluster (Arm ARM, MPIDR_EL1). */
#define MPIDR_AFF0_MASK 0xff

	.section .text.boot, "ax"
	.global _start
	.type _start, %function
_start:
	/* Only core 0 boots the kernel for now; SMP comes in a later phase. */
	mrs	x1, mpidr_el1
	and	x1, x1, #MPIDR_AFF0_MASK
	cbnz	x1, park

	/*
	 * x0 holds the Device Tree address when the loader follows the Linux arm64 boot
	 * protocol (Pi firmware, QEMU raspi4b). QEMU virt booting an ELF passes 0 and leaves
	 * the DTB at the start of RAM (0x40000000). Keep x0 for kernel_main either way.
	 */
	mov	x19, x0

	ldr	x1, =boot_stack_top
	mov	sp, x1

	/* C code expects zero-initialised .bss; both bounds are 16-byte aligned by the linker script. */
	ldr	x1, =bss_start
	ldr	x2, =bss_end
1:	cmp	x1, x2
	b.hs	2f
	stp	xzr, xzr, [x1], #16
	b	1b

2:	mov	x0, x19
	bl	kernel_main

	/* kernel_main must not return; if it does, stop this core too. */
park:
	wfe
	b	park
	.size _start, . - _start

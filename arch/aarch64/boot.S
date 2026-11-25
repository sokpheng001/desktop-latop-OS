/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Kernel entry point. Runs first on every core the firmware starts: parks all
 * secondary cores, drops from EL2 to EL1 if needed, sets up the boot stack,
 * clears .bss and calls kernel_main.
 */

#include "arch/aarch64/sysregs.h"

/* MPIDR_EL1.Aff0, bits [7:0]: core number within the cluster (Arm ARM, MPIDR_EL1). */
#define MPIDR_AFF0_MASK 0xff

/*
 * arm64 kernel image header (64 bytes), as described in the Linux kernel's
 * Documentation/arch/arm64/booting.rst boot protocol. With it, QEMU and the
 * Raspberry Pi firmware treat the raw image as a kernel: they load it at
 * 2 MB-aligned RAM base + text_offset and pass the Device Tree address in x0.
 */
#define IMAGE_MAGIC 0x644d5241 /* "ARM\x64" */
/* flags: little-endian, page size unspecified, placed close to the start of RAM. */
#define IMAGE_FLAGS 0

	.section .text.boot, "ax"
	.global _start
	.type _start, %function
_start:
	b	boot_entry		/* code0: first instruction executed */
	.long	0			/* code1 */
	.quad	kernel_text_offset	/* offset from the 2 MB-aligned base (linker script) */
	.quad	kernel_image_size	/* bytes the image occupies, including .bss and stack */
	.quad	IMAGE_FLAGS
	.quad	0			/* res2 */
	.quad	0			/* res3 */
	.quad	0			/* res4 */
	.long	IMAGE_MAGIC
	.long	0			/* res5: no PE/COFF header */

boot_entry:
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

	/*
	 * The kernel runs at EL1. QEMU virt enters there directly; the Pi 4 firmware and
	 * QEMU raspi4b (and virt with virtualization=on) enter at EL2, so configure EL1
	 * and drop into it with an exception return.
	 */
	mrs	x1, CurrentEL
	and	x1, x1, #(CURRENTEL_EL_MASK << CURRENTEL_EL_SHIFT)
	cmp	x1, #CURRENTEL_EL1
	b.eq	el1_entry
	cmp	x1, #CURRENTEL_EL2
	b.ne	park			/* EL3 entry is not supported yet */

	ldr	x1, =HCR_EL2_RW
	msr	hcr_el2, x1

	/* EL1 reads MIDR_EL1/MPIDR_EL1 through these virtual copies: give it the real values. */
	mrs	x1, midr_el1
	msr	vpidr_el2, x1
	mrs	x1, mpidr_el1
	msr	vmpidr_el2, x1

	/* The generic timer is driven from EL1 (Phase 2): no counter offset, no traps. */
	mov	x1, #(CNTHCTL_EL2_EL1PCTEN | CNTHCTL_EL2_EL1PCEN)
	msr	cnthctl_el2, x1
	msr	cntvoff_el2, xzr

	mov	x1, #CPTR_EL2_RES1
	msr	cptr_el2, x1
	msr	hstr_el2, xzr

	ldr	x1, =SCTLR_EL1_RES1
	msr	sctlr_el1, x1

	mov	x1, #SPSR_EL2_TO_EL1
	msr	spsr_el2, x1
	adr	x1, el1_entry
	msr	elr_el2, x1
	eret

el1_entry:
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

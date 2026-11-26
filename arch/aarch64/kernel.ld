/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Common AArch64 kernel memory layout. Each board's linker.ld sets KERNEL_BASE
 * (the address the firmware or QEMU loads the image at) and then includes this file.
 */

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

/* 4 KB: the smallest AArch64 translation granule, so each region can later get its own MMU permissions. */
PAGE_SIZE = 0x1000;

/* Boot stack for the primary core. */
BOOT_STACK_SIZE = 0x4000;

/* Separate segments so code is read-execute and data read-write (no RWX mapping). */
PHDRS
{
	text PT_LOAD FLAGS(5);   /* R + X */
	rodata PT_LOAD FLAGS(4); /* R */
	data PT_LOAD FLAGS(6);   /* R + W */
}

SECTIONS
{
	. = KERNEL_BASE;
	kernel_start = .;

	/* .text.boot holds _start and must be the first byte of the image. */
	.text : {
		KEEP(*(.text.boot))
		*(.text .text.*)
	} :text

	. = ALIGN(PAGE_SIZE);
	kernel_rodata_start = .;
	.rodata : {
		*(.rodata .rodata.*)
	} :rodata

	. = ALIGN(PAGE_SIZE);
	kernel_data_start = .;
	.data : {
		*(.data .data.*)
	} :data

	/* Zeroed by boot.S in 16-byte steps, so both ends are 16-byte aligned. */
	. = ALIGN(16);
	.bss (NOLOAD) : {
		bss_start = .;
		*(.bss .bss.*)
		*(COMMON)
		. = ALIGN(16);
		bss_end = .;
	} :data

	/* The AArch64 stack pointer must stay 16-byte aligned. */
	. = ALIGN(PAGE_SIZE);
	.stack (NOLOAD) : {
		boot_stack_bottom = .;
		. += BOOT_STACK_SIZE;
		boot_stack_top = .;
	} :data

	kernel_end = .;

	/* Values for the arm64 image header in boot.S. */
	kernel_image_size = kernel_end - kernel_start;
	kernel_text_offset = KERNEL_BASE % 0x200000;

	/DISCARD/ : {
		*(.comment)
		*(.note .note.*)
		*(.eh_frame .eh_frame_hdr)
	}
}

/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * EL1 exception handling: installs the vector table and reports any exception
 * the kernel does not expect, with the registers needed to debug it.
 */

#include "arch.h"
#include "arch/aarch64/exception.h"
#include "irq.h"
#include "lib/kprintf.h"

/* ESR_ELx fields (Arm ARM DDI 0487, ESR_EL1): exception class [31:26], ISS [24:0]. */
#define ESR_EC_SHIFT 26
#define ESR_EC_MASK  0x3fu
#define ESR_ISS_MASK 0x1ffffffu

/* Exception classes for aborts, whose ISS[5:0] holds a fault status code (DFSC/IFSC). */
#define EC_IABT_LOWER 0x20u
#define EC_IABT_CUR   0x21u
#define EC_DABT_LOWER 0x24u
#define EC_DABT_CUR   0x25u
#define ESR_FSC_MASK  0x3fu

extern char exception_vectors[];

struct ec_name {
	u32 ec;
	const char *name;
};

/* Exception classes the kernel is most likely to hit (ESR_EL1.EC encodings). */
static const struct ec_name ec_names[] = {
    {0x00, "Unknown reason"},
    {0x01, "Trapped WFI/WFE"},
    {0x07, "Trapped FP/SIMD access"},
    {0x0e, "Illegal execution state"},
    {0x15, "SVC from AArch64"},
    {0x18, "Trapped MSR/MRS/system instruction"},
    {0x20, "Instruction abort from lower EL"},
    {0x21, "Instruction abort from current EL"},
    {0x22, "PC alignment fault"},
    {0x24, "Data abort from lower EL"},
    {0x25, "Data abort from current EL"},
    {0x26, "SP alignment fault"},
    {0x2f, "SError"},
    {0x3c, "Breakpoint (BRK)"},
};

/* Abort fault status codes (ESR_EL1 ISS encoding for instruction and data aborts). */
static const struct ec_name fsc_names[] = {
    {0x04, "Translation fault, level 0"}, {0x05, "Translation fault, level 1"},
    {0x06, "Translation fault, level 2"}, {0x07, "Translation fault, level 3"},
    {0x09, "Access flag fault, level 1"}, {0x0a, "Access flag fault, level 2"},
    {0x0b, "Access flag fault, level 3"}, {0x0d, "Permission fault, level 1"},
    {0x0e, "Permission fault, level 2"},  {0x0f, "Permission fault, level 3"},
    {0x10, "Synchronous external abort"}, {0x21, "Alignment fault"},
};

static const char *const group_names[] = {
    "current EL, SP_EL0",
    "current EL, SP_ELx",
    "lower EL, AArch64",
    "lower EL, AArch32",
};

static const char *const type_names[] = {"Synchronous", "IRQ", "FIQ", "SError"};

static const char *lookup(const struct ec_name *table, usize n, u32 code)
{
	for (usize i = 0; i < n; i++) {
		if (table[i].ec == code)
			return table[i].name;
	}
	return "Other";
}

static bool ec_is_abort(u32 ec)
{
	return ec == EC_IABT_LOWER || ec == EC_IABT_CUR || ec == EC_DABT_LOWER || ec == EC_DABT_CUR;
}

void arch_exception_init(void)
{
	__asm__ volatile("msr vbar_el1, %0\n\tisb" : : "r"(exception_vectors) : "memory");
}

void arch_exception_handler(struct exc_frame *frame, u64 kind)
{
	u32 ec = (u32)(frame->esr >> ESR_EC_SHIFT) & ESR_EC_MASK;

	/* The kernel runs at EL1 on SP_EL1, so its interrupts arrive in the current-EL SPx slot. */
	if (kind == EXC_GROUP_CUR_SPX * EXC_TYPES_PER_GROUP + EXC_TYPE_IRQ) {
		irq_dispatch();
		return;
	}

	/* Anything else is a bug: report it and stop. */
	kprintf("\n*** Unexpected exception: %s (%s)\n", type_names[kind % EXC_TYPES_PER_GROUP],
		group_names[kind / EXC_TYPES_PER_GROUP]);
	kprintf("ESR_EL1:  %p  EC=0x%02x %s  ISS=0x%07x\n", (void *)frame->esr, ec,
		lookup(ec_names, ARRAY_SIZE(ec_names), ec), (u32)frame->esr & ESR_ISS_MASK);
	if (ec_is_abort(ec)) {
		u32 fsc = (u32)frame->esr & ESR_FSC_MASK;

		kprintf("FSC:      0x%02x %s\n", fsc,
			lookup(fsc_names, ARRAY_SIZE(fsc_names), fsc));
	}
	kprintf("ELR_EL1:  %p\n", (void *)frame->elr);
	kprintf("FAR_EL1:  %p  (valid for aborts only)\n", (void *)frame->far);
	kprintf("SPSR_EL1: %p\n", (void *)frame->spsr);
	for (unsigned int i = 0; i < 31; i += 2) {
		if (i + 1 < 31)
			kprintf("x%02u: %p  x%02u: %p\n", i, (void *)frame->x[i], i + 1,
				(void *)frame->x[i + 1]);
		else
			kprintf("x%02u: %p\n", i, (void *)frame->x[i]);
	}
	kprintf("*** System halted.\n");
	arch_halt();
}

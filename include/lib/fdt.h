/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Flattened Devicetree (DTB) reader: just what the kernel needs at boot.
 */

#ifndef OS001_LIB_FDT_H
#define OS001_LIB_FDT_H

#include "types.h"

#ifndef __ASSEMBLY__

struct fdt_mem_region {
	u64 base;
	u64 size;
};

/* Most interrupt specifier cells kept per device (GIC bindings use three). */
#define FDT_MAX_IRQ_CELLS 3

/* A device node: its first "reg" entry and the cells of its first "interrupts" entry. */
struct fdt_device {
	u64 base;
	u64 size;
	u32 irq[FDT_MAX_IRQ_CELLS]; /* raw cells; their meaning belongs to the interrupt controller
				     */
	u32 irq_cells;		    /* cells stored in irq (0 if the node has no interrupts) */
};

/* True if blob starts with a DTB header of a version this reader understands. */
bool fdt_valid(const void *blob);

/* Size of the whole DTB in bytes (0 if invalid). */
u32 fdt_total_size(const void *blob);

/*
 * Collect the RAM ranges from every /memory node's "reg" property into out (at most
 * max entries). Returns the number of ranges found, or -1 if the DTB is malformed.
 */
int fdt_memory_regions(const void *blob, struct fdt_mem_region *out, int max);

/*
 * Collect the memory reservation block (/memreserve/ entries: RAM the loader or
 * firmware keeps for itself) into out (at most max entries). Returns the number of
 * entries in the DTB, which may exceed max, or -1 if the DTB is malformed.
 */
int fdt_reserved_regions(const void *blob, struct fdt_mem_region *out, int max);

/*
 * Collect every enabled node (status absent, "okay" or "ok") whose "compatible" list
 * contains compat and that has a "reg" property, in DTB order, into out (at most max
 * entries). Returns the number of such nodes, which may exceed max, or -1 if the DTB
 * is malformed.
 */
int fdt_find_compatible(const void *blob, const char *compat, struct fdt_device *out, int max);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_LIB_FDT_H */

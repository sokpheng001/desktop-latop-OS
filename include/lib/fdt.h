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

/* True if blob starts with a DTB header of a version this reader understands. */
bool fdt_valid(const void *blob);

/* Size of the whole DTB in bytes (0 if invalid). */
u32 fdt_total_size(const void *blob);

/*
 * Collect the RAM ranges from every /memory node's "reg" property into out (at most
 * max entries). Returns the number of ranges found, or -1 if the DTB is malformed.
 */
int fdt_memory_regions(const void *blob, struct fdt_mem_region *out, int max);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_LIB_FDT_H */

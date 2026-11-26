/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Physical memory layout discovered at boot.
 */

#ifndef OS001_MEM_H
#define OS001_MEM_H

#include "lib/fdt.h"
#include "types.h"

#ifndef __ASSEMBLY__

/* Most RAM ranges recorded; boards so far have one (virt) or two (Pi 4). */
#define MEM_MAX_REGIONS 8

/*
 * Read the RAM ranges from the Device Tree at dtb (0 if the loader passed none) and
 * start the page allocator (pmm.h) on the RAM nothing else is using.
 */
void mem_init(uptr dtb);

/* Recorded RAM ranges and their total size in bytes (0 if unknown). */
int mem_region_count(void);
const struct fdt_mem_region *mem_region(int i);
u64 mem_total(void);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_MEM_H */

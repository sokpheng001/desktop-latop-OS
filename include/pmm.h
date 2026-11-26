/*
 * Project: OS-001
 * Date: 2026-11-26
 * Authors: kwan, oda, sokpheng001
 *
 * Physical page allocator: hands out 4 KB pages of RAM.
 */

#ifndef OS001_PMM_H
#define OS001_PMM_H

#include "types.h"

#ifndef __ASSEMBLY__

/* 4 KB: the smallest AArch64 translation granule (Arm ARM, D8.2), so one page = one mapping. */
#define PMM_PAGE_SHIFT 12u
#define PMM_PAGE_SIZE  (1ul << PMM_PAGE_SHIFT)

/* Most RAM ranges the allocator tracks (matches MEM_MAX_REGIONS). */
#define PMM_MAX_REGIONS 8

/* A byte range of physical memory; neither end needs to be page-aligned. */
struct pmm_range {
	u64 base;
	u64 size;
};

/*
 * Start tracking the RAM in ram[0..nram), minus every range in reserved[0..nres).
 * Pages only partly covered by RAM are skipped; pages even partly reserved are never
 * handed out. Physical page 0 is never handed out either, because 0 means "no page".
 * The allocator's own bitmap is placed in free RAM and reserved too.
 * Returns false if there is no usable RAM or no room for the bitmap.
 */
bool pmm_init(const struct pmm_range *ram, int nram, const struct pmm_range *reserved, int nres);

/* count physically contiguous, page-aligned pages; 0 if no such run is free. */
uptr pmm_alloc_pages(usize count);

/*
 * Return count pages starting at addr. Returns false and changes nothing if addr is not
 * page-aligned, not tracked RAM, or any of the pages is already free (a double free).
 */
bool pmm_free_pages(uptr addr, usize count);

static inline uptr pmm_alloc_page(void)
{
	return pmm_alloc_pages(1);
}

static inline bool pmm_free_page(uptr addr)
{
	return pmm_free_pages(addr, 1);
}

/* Pages currently free, and all pages tracked (free + in use + reserved). */
usize pmm_free_count(void);
usize pmm_total_count(void);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_PMM_H */

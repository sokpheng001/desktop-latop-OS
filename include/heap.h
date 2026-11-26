/*
 * Project: OS-001
 * Date: 2026-11-26
 * Authors: kwan, oda, sokpheng001
 *
 * Kernel heap: kmalloc / kfree for objects of any size.
 */

#ifndef OS001_HEAP_H
#define OS001_HEAP_H

#include "types.h"

#ifndef __ASSEMBLY__

/* Every pointer kmalloc returns is aligned to this many bytes. */
#define HEAP_ALIGN 16u

/* Start with an empty heap. Call once, after the page allocator is up (pmm_init). */
void heap_init(void);

/* size bytes, aligned to HEAP_ALIGN, or NULL if size is 0 or memory is exhausted. */
void *kmalloc(usize size);

/* kmalloc, with the memory zero-filled. */
void *kzalloc(usize size);

/*
 * Give back memory from kmalloc. kfree(NULL) does nothing. A pointer kmalloc did not
 * return, or one already freed, is refused and counted (heap_bad_frees) instead of
 * corrupting the heap.
 */
void kfree(void *ptr);

/* Live allocations, pages the heap holds, and refused kfree calls. */
usize heap_live_count(void);
usize heap_page_count(void);
usize heap_bad_frees(void);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_HEAP_H */

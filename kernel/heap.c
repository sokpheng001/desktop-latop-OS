/*
 * Project: OS-001
 * Date: 2026-11-26
 * Authors: kwan, oda, sokpheng001
 *
 * Kernel heap on top of the page allocator. Small requests come from size classes
 * (32 to 2048-byte blocks): each class carves whole pages into equal blocks and
 * keeps the free ones on a list, so allocation and free are O(1) and blocks of one
 * size never fragment memory for another. This is the object-cache idea of Bonwick,
 * "The Slab Allocator: An Object-Caching Kernel Memory Allocator", USENIX Summer 1994,
 * without constructors. Requests too big for a class get whole contiguous pages.
 *
 * Every block starts with a 16-byte header recording its class (or page count) and
 * whether it is in use, so kfree needs no size and refuses double or foreign frees.
 * Pages of a size class are kept by the heap once taken, ready for reuse.
 * Not safe to call from interrupt handlers yet (single core, no locking).
 */

#include "heap.h"
#include "pmm.h"

#define HEAP_MAGIC_USED 0x4b4d414cu /* "KMAL" */
#define HEAP_MAGIC_FREE 0x46524545u /* "FREE" */
#define HEAP_LARGE	0xffffffffu /* header class value for page-sized allocations */

struct heap_header {
	u32 magic;
	u32 class; /* index into class_size, or HEAP_LARGE */
	u64 pages; /* HEAP_LARGE: number of pages; otherwise unused */
};

_Static_assert(sizeof(struct heap_header) == HEAP_ALIGN, "header must keep blocks aligned");

/* A free block's payload holds the next free block of its class. */
struct heap_free {
	struct heap_free *next;
};

/* Block sizes including the header: each a multiple of HEAP_ALIGN that divides a page. */
static const usize class_size[] = {32, 64, 128, 256, 512, 1024, 2048};
#define HEAP_CLASSES ((int)(sizeof(class_size) / sizeof(class_size[0])))

static struct heap_free *free_list[HEAP_CLASSES];
static usize live;
static usize pages;
static usize bad_frees;

void heap_init(void)
{
	for (int c = 0; c < HEAP_CLASSES; c++)
		free_list[c] = NULL;
	live = 0;
	pages = 0;
	bad_frees = 0;
}

/* Carve a fresh page into free blocks of class c. */
static bool refill(int c)
{
	uptr page = pmm_alloc_page();

	if (!page)
		return false;
	pages++;
	for (usize off = 0; off + class_size[c] <= PMM_PAGE_SIZE; off += class_size[c]) {
		struct heap_header *h = (struct heap_header *)(page + off);
		struct heap_free *f = (struct heap_free *)(h + 1);

		h->magic = HEAP_MAGIC_FREE;
		h->class = (u32)c;
		h->pages = 0;
		f->next = free_list[c];
		free_list[c] = f;
	}
	return true;
}

void *kmalloc(usize size)
{
	if (size == 0 || size > ~(usize)0 - PMM_PAGE_SIZE)
		return NULL;
	usize need = size + sizeof(struct heap_header);

	for (int c = 0; c < HEAP_CLASSES; c++) {
		if (need > class_size[c])
			continue;
		if (!free_list[c] && !refill(c))
			return NULL;
		struct heap_free *f = free_list[c];
		struct heap_header *h = (struct heap_header *)f - 1;

		free_list[c] = f->next;
		h->magic = HEAP_MAGIC_USED;
		live++;
		return f;
	}

	usize n = (need + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
	uptr first = pmm_alloc_pages(n);

	if (!first)
		return NULL;
	struct heap_header *h = (struct heap_header *)first;

	h->magic = HEAP_MAGIC_USED;
	h->class = HEAP_LARGE;
	h->pages = n;
	pages += n;
	live++;
	return h + 1;
}

void *kzalloc(usize size)
{
	u64 *p = kmalloc(size);

	/* Blocks are HEAP_ALIGN-aligned and sized in whole 8-byte words beyond size. */
	if (p)
		for (usize i = 0; i < (size + sizeof(u64) - 1) / sizeof(u64); i++)
			p[i] = 0;
	return p;
}

void kfree(void *ptr)
{
	if (!ptr)
		return;
	if ((uptr)ptr % HEAP_ALIGN) {
		bad_frees++;
		return;
	}
	struct heap_header *h = (struct heap_header *)ptr - 1;

	if (h->magic != HEAP_MAGIC_USED) {
		bad_frees++;
		return;
	}
	if (h->class == HEAP_LARGE) {
		usize n = (usize)h->pages;

		h->magic = HEAP_MAGIC_FREE;
		if (!pmm_free_pages((uptr)h, n)) {
			bad_frees++;
			return;
		}
		pages -= n;
		live--;
		return;
	}
	if (h->class >= (u32)HEAP_CLASSES) {
		bad_frees++;
		return;
	}
	struct heap_free *f = ptr;

	h->magic = HEAP_MAGIC_FREE;
	f->next = free_list[h->class];
	free_list[h->class] = f;
	live--;
}

usize heap_live_count(void)
{
	return live;
}

usize heap_page_count(void)
{
	return pages;
}

usize heap_bad_frees(void)
{
	return bad_frees;
}

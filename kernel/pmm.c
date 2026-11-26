/*
 * Project: OS-001
 * Date: 2026-11-26
 * Authors: kwan, oda, sokpheng001
 *
 * Physical page allocator. One bit per 4 KB page (1 = in use or reserved), one
 * bitmap per RAM range: 32 KB of bitmap per GB of RAM. Single pages are found by a
 * next-fit scan that skips full 64-page words; contiguous runs by a linear scan.
 * Pure bookkeeping with no console or hardware access, so the host unit tests
 * (tests/lib_test.c) run the same code.
 */

#include "pmm.h"

#define BITS_PER_WORD 64u
#define WORD_FULL     (~(u64)0)

struct pmm_region {
	u64 base; /* first page address */
	usize npages;
	u64 *bitmap; /* ceil(npages / 64) words */
	usize next;  /* next-fit hint: page index to start the next scan at */
};

static struct pmm_region regions[PMM_MAX_REGIONS];
static int region_count;
static usize free_pages;
static usize total_pages;

static u64 align_up(u64 v)
{
	return (v + PMM_PAGE_SIZE - 1) & ~(u64)(PMM_PAGE_SIZE - 1);
}

static u64 align_down(u64 v)
{
	return v & ~(u64)(PMM_PAGE_SIZE - 1);
}

static usize words_for(usize npages)
{
	return (npages + BITS_PER_WORD - 1) / BITS_PER_WORD;
}

static bool bit_get(const struct pmm_region *r, usize i)
{
	return (r->bitmap[i / BITS_PER_WORD] >> (i % BITS_PER_WORD)) & 1u;
}

static void bit_set(struct pmm_region *r, usize i)
{
	r->bitmap[i / BITS_PER_WORD] |= (u64)1 << (i % BITS_PER_WORD);
}

static void bit_clear(struct pmm_region *r, usize i)
{
	r->bitmap[i / BITS_PER_WORD] &= ~((u64)1 << (i % BITS_PER_WORD));
}

/* Mark every page touching [base, base + size) as used. */
static void mark_used(u64 base, u64 size)
{
	if (size == 0)
		return;
	u64 first = align_down(base);
	u64 last = align_down(base + size - 1);

	for (int k = 0; k < region_count; k++) {
		struct pmm_region *r = &regions[k];
		u64 end = r->base + (u64)r->npages * PMM_PAGE_SIZE;
		u64 lo = first > r->base ? first : r->base;

		for (u64 a = lo; a <= last && a < end; a += PMM_PAGE_SIZE) {
			usize i = (usize)((a - r->base) >> PMM_PAGE_SHIFT);

			if (!bit_get(r, i)) {
				bit_set(r, i);
				free_pages--;
			}
		}
	}
}

static bool overlaps(u64 a, u64 asize, u64 b, u64 bsize)
{
	return asize && bsize && a < b + bsize && b < a + asize;
}

/*
 * Find page-aligned room for size bytes of bitmap inside RAM that is not reserved
 * and not page 0. Returns 0 if there is none.
 */
static u64 place_bitmap(u64 size, const struct pmm_range *reserved, int nres)
{
	for (int k = 0; k < region_count; k++) {
		u64 end = regions[k].base + (u64)regions[k].npages * PMM_PAGE_SIZE;
		u64 cand = regions[k].base ? regions[k].base : PMM_PAGE_SIZE;
		bool moved = true;

		/* Step past each reserved range in the way until a full pass finds none. */
		while (moved && cand + size <= end) {
			moved = false;
			for (int j = 0; j < nres; j++) {
				if (overlaps(cand, size, reserved[j].base, reserved[j].size)) {
					cand = align_up(reserved[j].base + reserved[j].size);
					moved = true;
				}
			}
		}
		if (!moved && cand + size <= end)
			return cand;
	}
	return 0;
}

bool pmm_init(const struct pmm_range *ram, int nram, const struct pmm_range *reserved, int nres)
{
	usize words = 0;

	region_count = 0;
	free_pages = 0;
	total_pages = 0;

	for (int k = 0; k < nram && region_count < PMM_MAX_REGIONS; k++) {
		u64 base = align_up(ram[k].base);
		u64 end = align_down(ram[k].base + ram[k].size);

		if (end <= base)
			continue;
		struct pmm_region *r = &regions[region_count++];

		r->base = base;
		r->npages = (usize)((end - base) >> PMM_PAGE_SHIFT);
		r->bitmap = NULL;
		r->next = 0;
		total_pages += r->npages;
		words += words_for(r->npages);
	}
	if (region_count == 0)
		return false;

	u64 bitmap_size = align_up((u64)words * sizeof(u64));
	u64 bitmap_base = place_bitmap(bitmap_size, reserved, nres);

	if (bitmap_base == 0) {
		region_count = 0;
		total_pages = 0;
		return false;
	}

	/* Everything starts free; bits past the end of a region stay set so they never match. */
	u64 *w = (u64 *)(uptr)bitmap_base;

	for (int k = 0; k < region_count; k++) {
		struct pmm_region *r = &regions[k];
		usize n = words_for(r->npages);

		r->bitmap = w;
		for (usize i = 0; i < n; i++)
			w[i] = 0;
		if (r->npages % BITS_PER_WORD)
			w[n - 1] = WORD_FULL << (r->npages % BITS_PER_WORD);
		w += n;
	}
	free_pages = total_pages;

	mark_used(0, PMM_PAGE_SIZE);
	mark_used(bitmap_base, bitmap_size);
	for (int j = 0; j < nres; j++)
		mark_used(reserved[j].base, reserved[j].size);
	return true;
}

/* Next-fit search for one free page in r; returns its index or r->npages. */
static usize find_one(struct pmm_region *r)
{
	usize nwords = words_for(r->npages);
	usize start = r->next / BITS_PER_WORD;

	for (usize n = 0; n < nwords; n++) {
		usize wi = (start + n) % nwords;
		u64 word = r->bitmap[wi];

		if (word == WORD_FULL)
			continue;
		usize b = 0;

		while ((word >> b) & 1u)
			b++;
		return wi * BITS_PER_WORD + b;
	}
	return r->npages;
}

/* First run of count free pages in r; returns its first index or r->npages. */
static usize find_run(const struct pmm_region *r, usize count)
{
	usize run = 0;

	for (usize i = 0; i < r->npages; i++) {
		run = bit_get(r, i) ? 0 : run + 1;
		if (run == count)
			return i + 1 - count;
	}
	return r->npages;
}

uptr pmm_alloc_pages(usize count)
{
	if (count == 0 || count > free_pages)
		return 0;

	for (int k = 0; k < region_count; k++) {
		struct pmm_region *r = &regions[k];
		usize first = count == 1 ? find_one(r) : find_run(r, count);

		if (first >= r->npages)
			continue;
		for (usize i = first; i < first + count; i++)
			bit_set(r, i);
		free_pages -= count;
		r->next = first + count < r->npages ? first + count : 0;
		return (uptr)(r->base + ((u64)first << PMM_PAGE_SHIFT));
	}
	return 0;
}

bool pmm_free_pages(uptr addr, usize count)
{
	if (count == 0 || (addr & (PMM_PAGE_SIZE - 1)))
		return false;

	for (int k = 0; k < region_count; k++) {
		struct pmm_region *r = &regions[k];

		if (addr < r->base)
			continue;
		usize first = (usize)((addr - r->base) >> PMM_PAGE_SHIFT);

		if (first >= r->npages)
			continue;
		if (count > r->npages - first)
			return false;
		/* Check everything first, so a bad call changes nothing. */
		for (usize i = first; i < first + count; i++)
			if (!bit_get(r, i))
				return false;
		for (usize i = first; i < first + count; i++)
			bit_clear(r, i);
		free_pages += count;
		return true;
	}
	return false;
}

usize pmm_free_count(void)
{
	return free_pages;
}

usize pmm_total_count(void)
{
	return total_pages;
}

/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Physical memory discovery: RAM ranges from the Device Tree the loader passes,
 * then the page allocator over that RAM minus everything already in use.
 */

#include "lib/fdt.h"
#include "lib/kprintf.h"
#include "mem.h"
#include "pmm.h"

#define MIB (1024ul * 1024ul)

/* Ranges kept from the allocator: image, low memory, the DTB and its /memreserve/ entries. */
#define MEM_MAX_RESERVED (3 + MEM_MAX_RSVMAP)
#define MEM_MAX_RSVMAP	 8

/* Image bounds from the linker script (arch/aarch64/kernel.ld); include the boot stack. */
extern char kernel_start[];
extern char kernel_end[];

static struct fdt_mem_region regions[MEM_MAX_REGIONS];
static int region_count;
static u64 total_bytes;

static void add_reserved(struct pmm_range *list, int *n, u64 base, u64 size)
{
	if (size && *n < MEM_MAX_RESERVED) {
		list[*n].base = base;
		list[*n].size = size;
		(*n)++;
	}
}

static void pages_init(uptr dtb)
{
	struct pmm_range ram[MEM_MAX_REGIONS];
	struct pmm_range reserved[MEM_MAX_RESERVED];
	struct fdt_mem_region rsv[MEM_MAX_RSVMAP];
	int nres = 0;
	u64 kstart = (u64)(uptr)kernel_start;

	for (int i = 0; i < region_count; i++) {
		ram[i].base = regions[i].base;
		ram[i].size = regions[i].size;
		/*
		 * RAM below the image in the same range belongs to the loader: on the Pi the
		 * firmware's stub and the secondary-core spin table (release addresses
		 * 0xd8-0xf0) live at the bottom of RAM, and QEMU places the same there.
		 */
		if (kstart >= regions[i].base && kstart < regions[i].base + regions[i].size)
			add_reserved(reserved, &nres, regions[i].base, kstart - regions[i].base);
	}
	add_reserved(reserved, &nres, kstart, (u64)(uptr)kernel_end - kstart);
	add_reserved(reserved, &nres, dtb, fdt_total_size((const void *)dtb));

	int nrsv = fdt_reserved_regions((const void *)dtb, rsv, MEM_MAX_RSVMAP);

	for (int i = 0; i < nrsv && i < MEM_MAX_RSVMAP; i++)
		add_reserved(reserved, &nres, rsv[i].base, rsv[i].size);
	if (nrsv > MEM_MAX_RSVMAP)
		kprintf("Pages:  warning: only %d of %d /memreserve/ entries kept\n",
			MEM_MAX_RSVMAP, nrsv);

	if (!pmm_init(ram, region_count, reserved, nres)) {
		kprintf("Pages:  allocator not started (no room for its bitmap)\n");
		return;
	}
	kprintf("Pages:  %lu free of %lu (%lu KB each)\n", (unsigned long)pmm_free_count(),
		(unsigned long)pmm_total_count(), (unsigned long)(PMM_PAGE_SIZE / 1024));
}

void mem_init(uptr dtb)
{
	if (!dtb || !fdt_valid((const void *)dtb)) {
		kprintf("RAM:    unknown (no valid Device Tree from the loader)\n");
		return;
	}

	int n = fdt_memory_regions((const void *)dtb, regions, MEM_MAX_REGIONS);

	if (n <= 0) {
		kprintf("RAM:    unknown (no memory node in the Device Tree)\n");
		return;
	}
	region_count = n;
	for (int i = 0; i < n; i++)
		total_bytes += regions[i].size;

	kprintf("RAM:    %lu MiB in %d region%s\n", (unsigned long)(total_bytes / MIB), n,
		n == 1 ? "" : "s");
	for (int i = 0; i < n; i++)
		kprintf("        %p - %p (%lu MiB)\n", (void *)regions[i].base,
			(void *)(regions[i].base + regions[i].size - 1),
			(unsigned long)(regions[i].size / MIB));

	pages_init(dtb);
}

int mem_region_count(void)
{
	return region_count;
}

const struct fdt_mem_region *mem_region(int i)
{
	return (i >= 0 && i < region_count) ? &regions[i] : NULL;
}

u64 mem_total(void)
{
	return total_bytes;
}

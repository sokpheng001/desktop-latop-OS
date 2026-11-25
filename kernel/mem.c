/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Physical memory discovery: RAM ranges from the Device Tree the loader passes.
 */

#include "lib/fdt.h"
#include "lib/kprintf.h"
#include "mem.h"

#define MIB (1024ul * 1024ul)

static struct fdt_mem_region regions[MEM_MAX_REGIONS];
static int region_count;
static u64 total_bytes;

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

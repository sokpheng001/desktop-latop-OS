/*
 * Project: OS-001
 * Date: 2026-11-26
 * Authors: kwan, oda, sokpheng001
 *
 * Kernel address space: every RAM range from the Device Tree plus the board's
 * device registers, identity-mapped. Anything else stays unmapped, so a stray
 * pointer faults instead of touching hardware or memory by accident.
 *
 * Least privilege: kernel code is read-only, kernel constants read-only and never
 * executed, all other RAM read/write and never executed, and page 0 is not mapped so
 * a NULL pointer faults.
 */

#include "arch.h"
#include "compiler.h"
#include "hal.h"
#include "lib/kprintf.h"
#include "mem.h"
#include "pmm.h"
#include "vm.h"

/*
 * RAM ranges (each split in up to three pieces around page 0 and the image), the three
 * kernel sections and a few device ranges per board.
 */
#define VM_MAX_MAPS (3 * MEM_MAX_REGIONS + 3 + 4)

/* Section bounds from the linker script (arch/aarch64/kernel.ld), all page-aligned. */
extern char kernel_start[];
extern char kernel_rodata_start[];
extern char kernel_data_start[];
extern char kernel_end[];

struct map_list {
	struct arch_mem_map m[VM_MAX_MAPS];
	int n;
	bool full;
};

static void add(struct map_list *l, u64 base, u64 end, enum arch_mem_type type)
{
	if (end <= base)
		return;
	if (l->n == VM_MAX_MAPS) {
		l->full = true;
		return;
	}
	l->m[l->n].base = base;
	l->m[l->n].size = end - base;
	l->m[l->n].type = type;
	l->n++;
}

/* Add RAM [base, end) as read/write data, leaving out the holes (sorted, disjoint). */
static void add_ram(struct map_list *l, u64 base, u64 end, const u64 (*holes)[2], int nholes)
{
	for (int h = 0; h < nholes; h++) {
		if (holes[h][1] <= base || holes[h][0] >= end)
			continue;
		add(l, base, holes[h][0], ARCH_MEM_NORMAL);
		base = holes[h][1];
	}
	add(l, base, end, ARCH_MEM_NORMAL);
}

void vm_init(void)
{
	static struct map_list list;
	const struct hal_mmio_range *mmio;
	u64 text = (u64)(uptr)kernel_start;
	u64 rodata = (u64)(uptr)kernel_rodata_start;
	u64 data = (u64)(uptr)kernel_data_start;
	u64 end = (u64)(uptr)kernel_end;
	const u64 holes[][2] = {{0, PMM_PAGE_SIZE}, {text, end}};

	if (mem_region_count() == 0) {
		kprintf("MMU:    off (RAM layout unknown)\n");
		return;
	}
	for (int i = 0; i < mem_region_count(); i++)
		add_ram(&list, mem_region(i)->base, mem_region(i)->base + mem_region(i)->size,
			holes, (int)ARRAY_SIZE(holes));
	add(&list, text, rodata, ARCH_MEM_TEXT);
	add(&list, rodata, data, ARCH_MEM_RODATA);
	add(&list, data, end, ARCH_MEM_NORMAL);

	int nmmio = hal_mmio_ranges(&mmio);

	for (int i = 0; i < nmmio; i++)
		add(&list, mmio[i].base, mmio[i].base + mmio[i].size, ARCH_MEM_DEVICE);
	if (list.full) {
		kprintf("MMU:    off (too many memory ranges)\n");
		return;
	}
	struct arch_mem_map *maps = list.m;
	int n = list.n;

	int tables = arch_mmu_init(maps, n);

	if (tables < 0) {
		kprintf("MMU:    off (could not build the page tables)\n");
		return;
	}
	kprintf("MMU:    on, caches on, W^X, %u-bit identity map, %d table page%s\n",
		arch_mmu_va_bits(), tables, tables == 1 ? "" : "s");
}

/*
 * Project: OS-001
 * Date: 2026-11-26
 * Authors: kwan, oda, sokpheng001
 *
 * Kernel address space: every RAM range from the Device Tree plus the board's
 * device registers, identity-mapped. Anything else stays unmapped, so a stray
 * pointer faults instead of touching hardware or memory by accident.
 */

#include "arch.h"
#include "hal.h"
#include "lib/kprintf.h"
#include "mem.h"
#include "vm.h"

/* RAM ranges plus a few device ranges per board. */
#define VM_MAX_MAPS (MEM_MAX_REGIONS + 4)

void vm_init(void)
{
	struct arch_mem_map maps[VM_MAX_MAPS];
	const struct hal_mmio_range *mmio;
	int n = 0;

	for (int i = 0; i < mem_region_count() && n < VM_MAX_MAPS; i++, n++) {
		maps[n].base = mem_region(i)->base;
		maps[n].size = mem_region(i)->size;
		maps[n].type = ARCH_MEM_NORMAL;
	}
	if (n == 0) {
		kprintf("MMU:    off (RAM layout unknown)\n");
		return;
	}

	int nmmio = hal_mmio_ranges(&mmio);

	for (int i = 0; i < nmmio && n < VM_MAX_MAPS; i++, n++) {
		maps[n].base = mmio[i].base;
		maps[n].size = mmio[i].size;
		maps[n].type = ARCH_MEM_DEVICE;
	}

	int tables = arch_mmu_init(maps, n);

	if (tables < 0) {
		kprintf("MMU:    off (could not build the page tables)\n");
		return;
	}
	kprintf("MMU:    on, caches on, %u-bit identity map, %d table page%s\n", arch_mmu_va_bits(),
		tables, tables == 1 ? "" : "s");
}

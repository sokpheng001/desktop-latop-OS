/*
 * Project: OS-001
 * Date: 2026-11-26
 * Authors: kwan, oda, sokpheng001
 *
 * Stage 1 EL1 translation: identity-mapped page tables and MMU enable. Written from
 * the Arm Architecture Reference Manual for A-profile (Arm DDI 0487), chapter D8
 * "The AArch64 Virtual Memory System Architecture".
 *
 * Layout: 4 KB granule, 39-bit virtual addresses (T0SZ = 25), so a walk starts at
 * level 1 and has three levels: level 1 entries cover 1 GB, level 2 entries 2 MB,
 * level 3 entries 4 KB (D8.2, "Translation granule" and "starting level"). Ranges are
 * mapped with the largest block that fits, which keeps the tables and the TLB
 * footprint small (the superpage argument of Navarro et al., "Practical, transparent
 * operating system support for superpages", OSDI 2002).
 */

#include "arch.h"
#include "arch/aarch64/sysregs.h"
#include "pmm.h"

#define VA_BITS	       39u
#define ENTRIES	       512u /* 4 KB table / 8-byte descriptors */
#define LEVEL_FIRST    1
#define LEVEL_LAST     3
#define L3_SHIFT       12u /* bits translated per level: 9, from 4 KB upwards */
#define BITS_PER_LEVEL 9u

/* Descriptor fields (D8.3, "Translation table descriptor formats"). */
#define DESC_VALID	(1ul << 0)
#define DESC_TABLE	(1ul << 1) /* at levels 1-2: points to a table; at level 3: a page */
#define DESC_BLOCK	(0ul << 1)
#define DESC_ATTRIDX(i) ((u64)(i) << 2)
#define DESC_AP_EL1_RW	(0ul << 6) /* EL1 read/write, EL0 no access */
#define DESC_AP_EL1_RO	(2ul << 6) /* EL1 read-only, EL0 no access */
#define DESC_AP_ALL_RW	(1ul << 6) /* EL1 and EL0 read/write */
#define DESC_AP_ALL_RO	(3ul << 6) /* EL1 and EL0 read-only */
#define DESC_SH_INNER	(3ul << 8)
#define DESC_AF		(1ul << 10) /* access flag set up front, so no access flag faults */
#define DESC_NG		(1ul << 11) /* not global: TLB entries tagged with the current ASID */
#define DESC_PXN	(1ul << 53) /* never executed at EL1 */
#define DESC_UXN	(1ul << 54) /* never executed at EL0 */
#define DESC_ADDR_MASK	0x0000fffffffff000ul /* output address bits [47:12] */

/* Cached memory; EL0 never executes kernel memory. */
#define ATTRS_RAM (DESC_ATTRIDX(MAIR_IDX_NORMAL) | DESC_SH_INNER | DESC_AF | DESC_UXN)

static u64 type_attrs(enum arch_mem_type type)
{
	switch (type) {
	case ARCH_MEM_TEXT:
		return ATTRS_RAM | DESC_AP_EL1_RO;
	case ARCH_MEM_RODATA:
		return ATTRS_RAM | DESC_AP_EL1_RO | DESC_PXN;
	case ARCH_MEM_DEVICE:
		return DESC_ATTRIDX(MAIR_IDX_DEVICE) | DESC_AP_EL1_RW | DESC_AF | DESC_PXN |
		       DESC_UXN;
	case ARCH_MEM_NORMAL:
	default:
		return ATTRS_RAM | DESC_AP_EL1_RW | DESC_PXN;
	}
}

static u64 *root;
static int table_pages;

static unsigned int level_shift(int level)
{
	return L3_SHIFT + BITS_PER_LEVEL * (unsigned int)(LEVEL_LAST - level);
}

static u64 *new_table(void)
{
	u64 *t = (u64 *)pmm_alloc_page();

	if (!t)
		return NULL;
	for (unsigned int i = 0; i < ENTRIES; i++)
		t[i] = 0;
	table_pages++;
	return t;
}

/* Map one block or page at va to pa at the given level, creating tables on the way. */
static bool map_one(u64 *table, u64 va, u64 pa, int level, u64 attrs)
{

	for (int l = LEVEL_FIRST; l < level; l++) {
		u64 *entry = &table[(va >> level_shift(l)) % ENTRIES];

		if (!(*entry & DESC_VALID)) {
			u64 *next = new_table();

			if (!next)
				return false;
			*entry = ((u64)(uptr)next & DESC_ADDR_MASK) | DESC_TABLE | DESC_VALID;
		} else if (!(*entry & DESC_TABLE)) {
			return false; /* already covered by a larger block: ranges overlap */
		}
		table = (u64 *)(uptr)(*entry & DESC_ADDR_MASK);
	}

	u64 *entry = &table[(va >> level_shift(level)) % ENTRIES];

	if (*entry & DESC_VALID)
		return false;
	*entry = (pa & DESC_ADDR_MASK) | attrs | (level == LEVEL_LAST ? DESC_TABLE : DESC_BLOCK) |
		 DESC_VALID;
	return true;
}

/* The level 3 entry for va, or NULL if no level 3 table covers it. */
static u64 *walk_l3(u64 *table, u64 va)
{

	for (int l = LEVEL_FIRST; table && l < LEVEL_LAST; l++) {
		u64 e = table[(va >> level_shift(l)) % ENTRIES];

		if ((e & (DESC_VALID | DESC_TABLE)) != (DESC_VALID | DESC_TABLE))
			return NULL;
		table = (u64 *)(uptr)(e & DESC_ADDR_MASK);
	}
	return table ? &table[(va >> L3_SHIFT) % ENTRIES] : NULL;
}

static bool map_range(u64 base, u64 size, u64 attrs)
{
	u64 page = 1ul << L3_SHIFT;
	u64 va = base & ~(page - 1);
	u64 end = (base + size + page - 1) & ~(page - 1);

	if (size == 0 || end > (1ul << VA_BITS) || end < va)
		return false;

	while (va < end) {
		int level = LEVEL_FIRST;

		/* Largest block whose alignment and size fit what is left. */
		while (level < LEVEL_LAST) {
			u64 block = 1ul << level_shift(level);

			if ((va & (block - 1)) == 0 && end - va >= block)
				break;
			level++;
		}
		if (!map_one(root, va, va, level, attrs))
			return false;
		va += 1ul << level_shift(level);
	}
	return true;
}

/* Clean and invalidate [start, end) to the point of coherency (D7.5, data cache maintenance). */
static void dcache_clean_inval(uptr start, uptr end)
{
	u64 ctr;

	__asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr));
	uptr line = 4ul << ((ctr >> CTR_DMINLINE_SHIFT) & CTR_DMINLINE_MASK);

	for (uptr a = start & ~(line - 1); a < end; a += line)
		__asm__ volatile("dc civac, %0" ::"r"(a) : "memory");
	__asm__ volatile("dsb sy" ::: "memory");
}

/* Clean every table reachable from table (at level) out of the data cache. */
static void clean_tables(const u64 *table, int level)
{
	dcache_clean_inval((uptr)table, (uptr)table + ENTRIES * sizeof(u64));
	if (level == LEVEL_LAST)
		return;
	for (unsigned int i = 0; i < ENTRIES; i++)
		if ((table[i] & (DESC_VALID | DESC_TABLE)) == (DESC_VALID | DESC_TABLE))
			clean_tables((const u64 *)(uptr)(table[i] & DESC_ADDR_MASK), level + 1);
}

int arch_mmu_init(const struct arch_mem_map *maps, int n)
{
	table_pages = 0;
	root = new_table();
	if (!root)
		return -1;

	for (int i = 0; i < n; i++) {
		if (!map_range(maps[i].base, maps[i].size, type_attrs(maps[i].type)))
			return -1;
	}

	/*
	 * The tables were written with the MMU off, i.e. uncached, straight to memory. The
	 * walker will read them through the cache, so make sure no stale line shadows them.
	 */
	clean_tables(root, LEVEL_FIRST);

	u64 mmfr0;

	__asm__ volatile("mrs %0, id_aa64mmfr0_el1" : "=r"(mmfr0));
	u64 mair = (MAIR_DEVICE_NGNRNE << (8 * MAIR_IDX_DEVICE)) |
		   (MAIR_NORMAL_WB << (8 * MAIR_IDX_NORMAL));
	u64 tcr = ((64ul - VA_BITS) << TCR_T0SZ_SHIFT) | TCR_IRGN0_WBWA | TCR_ORGN0_WBWA |
		  TCR_SH0_INNER | TCR_TG0_4K | ((64ul - VA_BITS) << TCR_T1SZ_SHIFT) | TCR_EPD1 |
		  TCR_TG1_4K | ((mmfr0 & ID_AA64MMFR0_PARANGE_MASK) << TCR_IPS_SHIFT);

	/*
	 * Register writes must take effect before the MMU turns on (isb), and no TLB entry
	 * may survive from before (tlbi + dsb). The code keeps running at the same address
	 * because it is identity-mapped.
	 */
	__asm__ volatile("msr mair_el1, %0\n"
			 "msr tcr_el1, %1\n"
			 "msr ttbr0_el1, %2\n"
			 "isb\n"
			 "tlbi vmalle1\n"
			 "dsb nsh\n"
			 "isb\n" ::"r"(mair),
			 "r"(tcr), "r"((u64)(uptr)root)
			 : "memory");

	u64 sctlr;

	__asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
	sctlr |= SCTLR_EL1_M | SCTLR_EL1_C | SCTLR_EL1_I | SCTLR_EL1_WXN;
	__asm__ volatile("msr sctlr_el1, %0\n"
			 "isb\n" ::"r"(sctlr)
			 : "memory");
	return table_pages;
}

/* Root table and ASID packed the way TTBR0_EL1 holds them (D19.2, TTBR0_EL1). */
#define TTBR_ASID_SHIFT 48u

static u64 *space_root(uptr space)
{
	return space ? (u64 *)(space & DESC_ADDR_MASK) : root;
}

static u64 space_asid(uptr space)
{
	return (u64)space >> TTBR_ASID_SHIFT;
}

uptr arch_mmu_space_create(u32 asid)
{
	if (!root || asid == 0 || asid > ARCH_MAX_ASID)
		return 0;
	u64 *t = new_table();

	if (!t)
		return 0;
	/* Share the kernel's mappings: same lower-level tables, so nothing is copied twice. */
	for (unsigned int i = 0; i < ENTRIES; i++)
		t[i] = root[i];
	return (uptr)t | ((uptr)asid << TTBR_ASID_SHIFT);
}

/* Free a table and every table below it (never the pages they map). */
static void free_tables(u64 *table, int level)
{
	for (unsigned int i = 0; level < LEVEL_LAST && i < ENTRIES; i++)
		if ((table[i] & (DESC_VALID | DESC_TABLE)) == (DESC_VALID | DESC_TABLE))
			free_tables((u64 *)(uptr)(table[i] & DESC_ADDR_MASK), level + 1);
	pmm_free_page((uptr)table);
	table_pages--;
}

void arch_mmu_space_destroy(uptr space)
{
	u64 *t = space_root(space);

	if (!space || t == root)
		return;
	/* Only entries the space added itself are its own; the rest belong to the kernel. */
	for (unsigned int i = 0; i < ENTRIES; i++)
		if (t[i] != root[i] &&
		    (t[i] & (DESC_VALID | DESC_TABLE)) == (DESC_VALID | DESC_TABLE))
			free_tables((u64 *)(uptr)(t[i] & DESC_ADDR_MASK), LEVEL_FIRST + 1);
	pmm_free_page((uptr)t);
	table_pages--;
	/* Drop every TLB entry tagged with this ASID, so the ASID can be reused. */
	__asm__ volatile("dsb ishst\n"
			 "tlbi aside1, %0\n"
			 "dsb ish\n"
			 "isb" ::"r"(space_asid(space) << TTBR_ASID_SHIFT)
			 : "memory");
}

void arch_mmu_switch(uptr space)
{
	u64 ttbr = space ? (u64)space : (u64)(uptr)root;

	/* Kernel mappings are global and user ones carry an ASID: no TLB flush needed. */
	__asm__ volatile("msr ttbr0_el1, %0\n"
			 "isb" ::"r"(ttbr)
			 : "memory");
}

bool arch_mmu_map_user(uptr space, u64 va, u64 pa, enum arch_user_perm perm)
{
	u64 attrs = ATTRS_RAM | DESC_PXN | DESC_NG;

	if (perm == ARCH_USER_CODE)
		attrs = (attrs & ~DESC_UXN) | DESC_AP_ALL_RO;
	else
		attrs |= DESC_AP_ALL_RW;
	if (!space || (va | pa) & ((1ul << L3_SHIFT) - 1) || va >= (1ul << VA_BITS))
		return false;
	if (!map_one(space_root(space), va, pa, LEVEL_LAST, attrs))
		return false;
	/* The new entry must be visible to the table walker before it is used. */
	__asm__ volatile("dsb ishst\n"
			 "isb" ::
			     : "memory");
	return true;
}

void arch_mmu_unmap_user(uptr space, u64 va)
{
	u64 *entry = space ? walk_l3(space_root(space), va) : NULL;

	if (!entry)
		return;
	*entry = 0;
	/* Remove the cached translation of va for this ASID, then wait for it to finish. */
	__asm__ volatile("dsb ishst\n"
			 "tlbi vae1, %0\n"
			 "dsb ish\n"
			 "isb" ::"r"((space_asid(space) << TTBR_ASID_SHIFT) | (va >> L3_SHIFT))
			 : "memory");
}

void arch_sync_icache(uptr addr, usize len)
{
	u64 ctr;

	__asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr));
	uptr line = 4ul << ((ctr >> CTR_DMINLINE_SHIFT) & CTR_DMINLINE_MASK);

	/* Push the new code to the point of unification, then drop stale instructions. */
	for (uptr a = addr & ~(line - 1); a < addr + len; a += line)
		__asm__ volatile("dc cvau, %0" ::"r"(a) : "memory");
	__asm__ volatile("dsb ish\n"
			 "ic iallu\n"
			 "dsb ish\n"
			 "isb" ::
			     : "memory");
}

unsigned int arch_mmu_va_bits(void)
{
	return VA_BITS;
}

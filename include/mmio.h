/*
 * Project: OS-001
 * Authors: kwan, oda, sokpheng001
 *
 * Memory-mapped I/O accessors. All hardware registers are read and written
 * through these helpers so that every device access is visible and ordered.
 */

#ifndef OS001_MMIO_H
#define OS001_MMIO_H

#include "compiler.h"
#include "types.h"

#ifndef __ASSEMBLY__

/*
 * Each access goes through a volatile pointer, so the compiler emits exactly one
 * load or store of the given width and never merges, splits or removes it.
 * Device memory (Arm ARM, "Device memory" attribute types) keeps accesses to the
 * same device in program order; use the barriers below when ordering against
 * normal memory (e.g. DMA buffers) or across devices matters.
 */

static inline u8 mmio_read8(uptr addr)
{
	return *(volatile u8 *)addr;
}

static inline u16 mmio_read16(uptr addr)
{
	return *(volatile u16 *)addr;
}

static inline u32 mmio_read32(uptr addr)
{
	return *(volatile u32 *)addr;
}

static inline u64 mmio_read64(uptr addr)
{
	return *(volatile u64 *)addr;
}

static inline void mmio_write8(uptr addr, u8 val)
{
	*(volatile u8 *)addr = val;
}

static inline void mmio_write16(uptr addr, u16 val)
{
	*(volatile u16 *)addr = val;
}

static inline void mmio_write32(uptr addr, u32 val)
{
	*(volatile u32 *)addr = val;
}

static inline void mmio_write64(uptr addr, u64 val)
{
	*(volatile u64 *)addr = val;
}

/* Full-system data synchronization barrier: all earlier accesses complete first. */
static inline void mmio_barrier(void)
{
	__asm__ volatile("dsb sy" ::: "memory");
}

/* Orders earlier memory accesses before later ones, without waiting for completion. */
static inline void mmio_order(void)
{
	__asm__ volatile("dmb sy" ::: "memory");
}

#endif /* !__ASSEMBLY__ */

#endif /* OS001_MMIO_H */

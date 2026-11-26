/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * AArch64 system register fields used at boot. Values from the Arm Architecture
 * Reference Manual for A-profile (Arm DDI 0487), register descriptions in chapter D19.
 * Safe to include from assembly.
 */

#ifndef OS001_ARCH_AARCH64_SYSREGS_H
#define OS001_ARCH_AARCH64_SYSREGS_H

/* Unsigned long constants in C; the assembler does not accept the ul suffix. */
#ifdef __ASSEMBLY__
#define UL(x) x
#else
#define UL(x) x##ul
#endif

/* CurrentEL: exception level in bits [3:2]. */
#define CURRENTEL_EL_SHIFT UL(2)
#define CURRENTEL_EL_MASK  UL(0x3)
#define CURRENTEL_EL1	   (UL(1) << CURRENTEL_EL_SHIFT)
#define CURRENTEL_EL2	   (UL(2) << CURRENTEL_EL_SHIFT)

/* HCR_EL2.RW (bit 31): EL1 executes in AArch64. All other bits 0: no traps to EL2. */
#define HCR_EL2_RW (UL(1) << 31)

/* CNTHCTL_EL2: let EL1/EL0 use the physical counter (EL1PCTEN) and timer (EL1PCEN). */
#define CNTHCTL_EL2_EL1PCTEN (UL(1) << 0)
#define CNTHCTL_EL2_EL1PCEN  (UL(1) << 1)

/* CPTR_EL2 with only its RES1 bits set (bits 13:12 and 9:0): FP/SIMD not trapped to EL2. */
#define CPTR_EL2_RES1 UL(0x33ff)

/*
 * SCTLR_EL1 with only its RES1 bits for Armv8.0 (bits 29, 28, 23, 22, 20, 11):
 * MMU, caches and alignment checking off, little-endian. arch_mmu_init turns the MMU on.
 */
#define SCTLR_EL1_RES1 UL(0x30d00800)

/* SCTLR_EL1 enable bits: M (bit 0) stage 1 MMU, C (bit 2) data cache, I (bit 12) instruction cache.
 */
#define SCTLR_EL1_M   (UL(1) << 0)
#define SCTLR_EL1_C   (UL(1) << 2)
#define SCTLR_EL1_I   (UL(1) << 12)
/* SCTLR_EL1.WXN (bit 19): every writable mapping is also treated as never-executable. */
#define SCTLR_EL1_WXN (UL(1) << 19)

/*
 * MAIR_EL1 attribute encodings (D19.2, MAIR_EL1), one byte per index:
 * 0x00 Device-nGnRnE; 0xff Normal, inner and outer write-back, read- and write-allocate.
 */
#define MAIR_DEVICE_NGNRNE UL(0x00)
#define MAIR_NORMAL_WB	   UL(0xff)
#define MAIR_IDX_DEVICE	   UL(0)
#define MAIR_IDX_NORMAL	   UL(1)

/* TCR_EL1 fields (D19.2, TCR_EL1). */
#define TCR_T0SZ_SHIFT UL(0)
#define TCR_IRGN0_WBWA (UL(1) << 8)  /* table walks: inner write-back, write-allocate */
#define TCR_ORGN0_WBWA (UL(1) << 10) /* table walks: outer write-back, write-allocate */
#define TCR_SH0_INNER  (UL(3) << 12) /* table walks: inner shareable */
#define TCR_TG0_4K     (UL(0) << 14)
#define TCR_T1SZ_SHIFT UL(16)
#define TCR_EPD1       (UL(1) << 23) /* no walks through TTBR1_EL1 (upper half unused) */
#define TCR_TG1_4K     (UL(2) << 30) /* TG1 encodes 4 KB differently from TG0 */
#define TCR_IPS_SHIFT  UL(32)

/* ID_AA64MMFR0_EL1.PARange (bits 3:0): physical address size, same encoding as TCR_EL1.IPS. */
#define ID_AA64MMFR0_PARANGE_MASK UL(0xf)

/* CTR_EL0.DminLine (bits 19:16): log2 of the smallest data cache line, in 4-byte words. */
#define CTR_DMINLINE_SHIFT UL(16)
#define CTR_DMINLINE_MASK  UL(0xf)

/* SPSR_ELx.M[3:0] = 0b0101: return to EL1 using SP_EL1 ("EL1h"). */
#define SPSR_MODE_EL1H	 UL(0x5)
/* SPSR_ELx.{D,A,I,F} (bits 9:6): all interrupts masked until vectors and the GIC exist. */
#define SPSR_DAIF_MASKED (UL(0xf) << 6)
#define SPSR_EL2_TO_EL1	 (SPSR_DAIF_MASKED | SPSR_MODE_EL1H)

#endif /* OS001_ARCH_AARCH64_SYSREGS_H */

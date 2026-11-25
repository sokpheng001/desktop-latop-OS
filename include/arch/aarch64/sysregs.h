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
 * MMU, caches and alignment checking off, little-endian. The MMU is enabled in Phase 3.
 */
#define SCTLR_EL1_RES1 UL(0x30d00800)

/* SPSR_ELx.M[3:0] = 0b0101: return to EL1 using SP_EL1 ("EL1h"). */
#define SPSR_MODE_EL1H	 UL(0x5)
/* SPSR_ELx.{D,A,I,F} (bits 9:6): all interrupts masked until vectors and the GIC exist. */
#define SPSR_DAIF_MASKED (UL(0xf) << 6)
#define SPSR_EL2_TO_EL1	 (SPSR_DAIF_MASKED | SPSR_MODE_EL1H)

#endif /* OS001_ARCH_AARCH64_SYSREGS_H */

/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Arm GICv2 driver: distributor and CPU interface setup, enable/disable,
 * acknowledge and end-of-interrupt, and SGIs. Register map and behaviour from the
 * ARM Generic Interrupt Controller Architecture Specification v2 (Arm IHI 0048B),
 * chapter 4 "Programmers' Model".
 */

#include "drivers/gicv2.h"
#include "mmio.h"

/* Distributor registers (IHI 0048B, chapter 4, distributor register map). */
#define GICD_CTLR	0x000
#define GICD_TYPER	0x004
#define GICD_ISENABLER	0x100
#define GICD_ICENABLER	0x180
#define GICD_ICPENDR	0x280
#define GICD_IPRIORITYR 0x400
#define GICD_ITARGETSR	0x800
#define GICD_SGIR	0xf00

/* CPU interface registers (IHI 0048B, chapter 4, CPU interface register map). */
#define GICC_CTLR 0x000
#define GICC_PMR  0x004
#define GICC_BPR  0x008
#define GICC_IAR  0x00c
#define GICC_EOIR 0x010

#define GICD_CTLR_ENABLE 1u
#define GICC_CTLR_ENABLE 1u

/* GICD_TYPER.ITLinesNumber [4:0]: the GIC supports 32 * (N + 1) interrupt IDs. */
#define GICD_TYPER_ITLINES_MASK 0x1fu
#define IRQS_PER_WORD		32u
/* IDs 0-15 are SGIs, 16-31 PPIs (per core); shared peripheral interrupts start at 32. */
#define GIC_FIRST_SPI		32u
#define GIC_MAX_IRQS		1020u

/* GICC_IAR.InterruptID [9:0]. */
#define GICC_IAR_ID_MASK 0x3ffu

/* Lower value = higher priority. Every interrupt gets the same middle priority for now. */
#define GIC_DEFAULT_PRIORITY 0xa0u
/* GICC_PMR: only priorities numerically below this reach the core. */
#define GIC_PRIORITY_MASK    0xf0u
/* Byte-per-interrupt registers, written four interrupts at a time. */
#define BYTE_x4(b)	     ((u32)(b) * 0x01010101u)
/* ITARGETSR CPU target bit for core 0 (single core until SMP). */
#define GIC_TARGET_CPU0	     0x01u

/* GICD_SGIR.TargetListFilter [25:24] = 0b10: forward only to the requesting core. */
#define GICD_SGIR_TARGET_SELF (2u << 24)
#define GICD_SGIR_ID_MASK     0xfu

u32 gicv2_num_irqs(const struct gicv2 *gic)
{
	u32 lines = mmio_read32(gic->dist_base + GICD_TYPER) & GICD_TYPER_ITLINES_MASK;
	u32 n = IRQS_PER_WORD * (lines + 1);

	return n > GIC_MAX_IRQS ? GIC_MAX_IRQS : n;
}

void gicv2_init(const struct gicv2 *gic)
{
	u32 n = gicv2_num_irqs(gic);

	mmio_write32(gic->dist_base + GICD_CTLR, 0);

	/* Start from a known state: everything disabled and not pending. */
	for (u32 i = 0; i < n; i += IRQS_PER_WORD) {
		mmio_write32(gic->dist_base + GICD_ICENABLER + i / 8, 0xffffffffu);
		mmio_write32(gic->dist_base + GICD_ICPENDR + i / 8, 0xffffffffu);
	}
	for (u32 i = 0; i < n; i += 4)
		mmio_write32(gic->dist_base + GICD_IPRIORITYR + i, BYTE_x4(GIC_DEFAULT_PRIORITY));
	/* SGI/PPI targets are fixed to the reading core; route every SPI to core 0. */
	for (u32 i = GIC_FIRST_SPI; i < n; i += 4)
		mmio_write32(gic->dist_base + GICD_ITARGETSR + i, BYTE_x4(GIC_TARGET_CPU0));

	mmio_write32(gic->dist_base + GICD_CTLR, GICD_CTLR_ENABLE);

	mmio_write32(gic->cpu_base + GICC_PMR, GIC_PRIORITY_MASK);
	mmio_write32(gic->cpu_base + GICC_BPR, 0); /* no priority grouping: no preemption yet */
	mmio_write32(gic->cpu_base + GICC_CTLR, GICC_CTLR_ENABLE);
}

void gicv2_enable(const struct gicv2 *gic, u32 irq)
{
	mmio_write32(gic->dist_base + GICD_ISENABLER + (irq / IRQS_PER_WORD) * 4,
		     1u << (irq % IRQS_PER_WORD));
}

void gicv2_disable(const struct gicv2 *gic, u32 irq)
{
	mmio_write32(gic->dist_base + GICD_ICENABLER + (irq / IRQS_PER_WORD) * 4,
		     1u << (irq % IRQS_PER_WORD));
}

u32 gicv2_ack(const struct gicv2 *gic)
{
	return mmio_read32(gic->cpu_base + GICC_IAR);
}

u32 gicv2_iar_irq(u32 iar)
{
	return iar & GICC_IAR_ID_MASK;
}

void gicv2_eoi(const struct gicv2 *gic, u32 iar)
{
	mmio_write32(gic->cpu_base + GICC_EOIR, iar);
}

void gicv2_send_sgi_self(const struct gicv2 *gic, u32 sgi)
{
	/* The SGI must be visible to the distributor only after earlier memory writes. */
	mmio_barrier();
	mmio_write32(gic->dist_base + GICD_SGIR, GICD_SGIR_TARGET_SELF | (sgi & GICD_SGIR_ID_MASK));
}

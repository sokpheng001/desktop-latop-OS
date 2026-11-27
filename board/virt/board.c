/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * HAL implementation for the QEMU virt board.
 */

#include "block.h"
#include "board.h"
#include "compiler.h"
#include "drivers/arm_gtimer.h"
#include "drivers/gicv2.h"
#include "drivers/pl011.h"
#include "drivers/virtio.h"
#include "hal.h"
#include "lib/fdt.h"
#include "lib/kprintf.h"

#define CONSOLE_BAUD 115200u

static const struct gicv2 gic = {
    .dist_base = BOARD_GICD_BASE,
    .cpu_base = BOARD_GICC_BASE,
};

static const struct pl011 console = {
    .base = BOARD_UART_BASE,
    .clock_hz = BOARD_UART_CLOCK_HZ,
};

void hal_init(void)
{
	pl011_init(&console, CONSOLE_BAUD);
	gicv2_init(&gic);
}

void hal_console_putc(char c)
{
	pl011_putc(&console, c);
}

int hal_console_getc(void)
{
	return pl011_getc(&console);
}

static const struct hal_mmio_range mmio_ranges[] = {
    {BOARD_MMIO_BASE, BOARD_MMIO_SIZE},
};

int hal_mmio_ranges(const struct hal_mmio_range **ranges)
{
	*ranges = mmio_ranges;
	return (int)ARRAY_SIZE(mmio_ranges);
}

/* The disk, if QEMU was given one, registered with the block layer as "vda". */
static struct virtio_blk disk;
static bool have_disk;

static int disk_read(void *ctx, u64 sector, void *buf, u32 count)
{
	return virtio_blk_rw(ctx, sector, buf, count, false);
}

static int disk_write(void *ctx, u64 sector, const void *buf, u32 count)
{
	/* The device only reads buf for a write. */
	return virtio_blk_rw(ctx, sector, (void *)(uptr)buf, count, true);
}

static struct block_dev disk_dev = {
    .name = "vda",
    .sector_size = VIRTIO_BLK_SECTOR_SIZE,
    .ctx = &disk,
    .read = disk_read,
    .write = disk_write,
};

/* GIC interrupt number for a device's Device Tree interrupt specifier; 0 if none. */
static u32 dt_irq(const struct fdt_device *d)
{
	if (d->irq_cells < 2)
		return 0;
	if (d->irq[0] == BOARD_DT_IRQ_SPI)
		return BOARD_GIC_FIRST_SPI + d->irq[1];
	if (d->irq[0] == BOARD_DT_IRQ_PPI)
		return BOARD_GIC_FIRST_PPI + d->irq[1];
	return 0;
}

/* True if [base, base + size) lies inside the device range the MMU maps. */
static bool in_mmio(u64 base, u64 size)
{
	return base >= BOARD_MMIO_BASE && size <= BOARD_MMIO_SIZE &&
	       base - BOARD_MMIO_BASE <= BOARD_MMIO_SIZE - size;
}

void hal_devices_init(uptr dtb)
{
	struct fdt_device dev[BOARD_VIRTIO_MAX];
	int n =
	    dtb ? fdt_find_compatible((const void *)dtb, "virtio,mmio", dev, BOARD_VIRTIO_MAX) : 0;

	if (n < 0) {
		kprintf("virtio: Device Tree unreadable, no devices\n");
		return;
	}
	if (n > BOARD_VIRTIO_MAX)
		n = BOARD_VIRTIO_MAX;

	int used = 0;

	for (int i = 0; i < n; i++) {
		/* Never touch an address the Device Tree claims but the MMU does not map. */
		if (!in_mmio(dev[i].base, dev[i].size))
			continue;

		struct virtio_blk blk;
		int id = virtio_mmio_probe((uptr)dev[i].base, dt_irq(&dev[i]), &blk);

		if (id <= (int)VIRTIO_ID_NONE)
			continue;
		used++;
		if (id == (int)VIRTIO_ID_BLOCK && !have_disk) {
			disk = blk;
			have_disk = true;
			disk_dev.sectors = disk.sectors;
			disk_dev.read_only = disk.read_only;
			block_register(&disk_dev);
			kprintf(
			    "Disk:   virtio block device at %p, irq %u, %lu sectors (%lu KiB)%s\n",
			    (void *)disk.base, disk.irq, (unsigned long)disk.sectors,
			    (unsigned long)(disk.sectors * VIRTIO_BLK_SECTOR_SIZE / 1024),
			    disk.read_only ? ", read-only" : "");
		}
	}
	kprintf("virtio: %d slots, %d in use\n", n, used);
	if (!have_disk)
		kprintf("Disk:   none\n");
}

const char *hal_board_name(void)
{
	return BOARD_NAME;
}

void hal_irq_enable(u32 irq)
{
	gicv2_enable(&gic, irq);
}

void hal_irq_disable(u32 irq)
{
	gicv2_disable(&gic, irq);
}

int hal_irq_claim(u32 *token)
{
	u32 iar = gicv2_ack(&gic);
	u32 irq = gicv2_iar_irq(iar);

	if (irq == GICV2_SPURIOUS_ID)
		return -1;
	*token = iar;
	return (int)irq;
}

void hal_irq_complete(u32 token)
{
	gicv2_eoi(&gic, token);
}

void hal_irq_raise(u32 irq)
{
	if (irq < GICV2_NUM_SGIS)
		gicv2_send_sgi_self(&gic, irq);
	else
		gicv2_set_pending(&gic, irq);
}

u32 hal_irq_count(void)
{
	return gicv2_num_irqs(&gic);
}

u32 hal_timer_irq(void)
{
	return BOARD_TIMER_IRQ;
}

void hal_timer_start(u32 hz)
{
	arm_gtimer_start(hz);
}

void hal_timer_ack(void)
{
	arm_gtimer_rearm();
}

u64 hal_counter(void)
{
	return arm_gtimer_count();
}

u64 hal_counter_freq(void)
{
	return arm_gtimer_freq();
}

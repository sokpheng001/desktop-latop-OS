/*
 * Project: OS-001
 * Date: 2026-11-27
 * Authors: kwan, oda, sokpheng001
 *
 * Block devices: disks the kernel reads and writes in fixed-size sectors, whatever
 * driver is behind them (virtio on virt, the SD card on the Pi later).
 */

#ifndef OS001_BLOCK_H
#define OS001_BLOCK_H

#include "types.h"

#ifndef __ASSEMBLY__

/* Most block devices registered at once. */
#define BLOCK_MAX_DEVICES 4

struct block_dev {
	const char *name;
	u64 sectors;	 /* capacity in sectors */
	u32 sector_size; /* bytes per sector */
	bool read_only;
	void *ctx; /* the driver's own device state */
	/* Transfer count sectors from sector; 0 on success, -1 on error. */
	int (*read)(void *ctx, u64 sector, void *buf, u32 count);
	int (*write)(void *ctx, u64 sector, const void *buf, u32 count);
};

/* Add a device (the structure must stay valid); false if the table is full. */
bool block_register(const struct block_dev *dev);

/* Number of devices registered, and device i (NULL if there is none). */
int block_count(void);
const struct block_dev *block_get(int i);

/*
 * Read or write count sectors starting at sector. buf must be a kernel address the
 * device can reach (any kernel address: the kernel's map is an identity map).
 * Returns 0, or -1 if the range is outside the device, it is read-only for a write,
 * or the driver reports an error.
 */
int block_read(const struct block_dev *dev, u64 sector, void *buf, u32 count);
int block_write(const struct block_dev *dev, u64 sector, const void *buf, u32 count);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_BLOCK_H */

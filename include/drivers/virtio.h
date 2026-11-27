/*
 * Project: OS-001
 * Date: 2026-11-27
 * Authors: kwan, oda, sokpheng001
 *
 * virtio devices over the MMIO transport (OASIS "Virtual I/O Device (VIRTIO)
 * Version 1.2", section 4.2), as found on QEMU's virt board.
 */

#ifndef OS001_DRIVERS_VIRTIO_H
#define OS001_DRIVERS_VIRTIO_H

#include "types.h"

#ifndef __ASSEMBLY__

/* Device IDs (spec 5). 0 means an empty slot. */
#define VIRTIO_ID_NONE	0u
#define VIRTIO_ID_BLOCK 2u

/* Bytes in one block-device sector: capacity and requests count in these (spec 5.2.4). */
#define VIRTIO_BLK_SECTOR_SIZE 512u

/* A block device found and initialised by virtio_mmio_probe. */
struct virtio_blk {
	uptr base;	/* MMIO registers */
	u32 irq;	/* interrupt number at the board's interrupt controller */
	u64 sectors;	/* capacity in VIRTIO_BLK_SECTOR_SIZE sectors */
	bool read_only; /* the device refuses writes (VIRTIO_BLK_F_RO) */
};

/*
 * Look at the virtio-mmio slot at base. Returns its device ID (VIRTIO_ID_NONE for an
 * empty slot), or -1 if there is no virtio device there or it uses the legacy
 * (version 1) interface. A block device is reset and its features negotiated, and its
 * details are stored in *blk (if blk is not NULL); other devices are left untouched.
 */
int virtio_mmio_probe(uptr base, u32 irq, struct virtio_blk *blk);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_DRIVERS_VIRTIO_H */

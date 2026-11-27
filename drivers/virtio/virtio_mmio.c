/*
 * Project: OS-001
 * Date: 2026-11-27
 * Authors: kwan, oda, sokpheng001
 *
 * virtio-mmio transport and block device setup, written from OASIS "Virtual I/O
 * Device (VIRTIO) Version 1.2" (section numbers below refer to it). Only the
 * version 2 ("modern") register layout is supported. Every register is little-endian,
 * which is also the CPU's byte order on our targets.
 */

#include "drivers/virtio.h"
#include "mmio.h"

/* MMIO register offsets (4.2.2). */
#define VIRTIO_MMIO_MAGIC	      0x000
#define VIRTIO_MMIO_VERSION	      0x004
#define VIRTIO_MMIO_DEVICE_ID	      0x008
#define VIRTIO_MMIO_DEVICE_FEATURES   0x010
#define VIRTIO_MMIO_DEVICE_FEAT_SEL   0x014
#define VIRTIO_MMIO_DRIVER_FEATURES   0x020
#define VIRTIO_MMIO_DRIVER_FEAT_SEL   0x024
#define VIRTIO_MMIO_STATUS	      0x070
#define VIRTIO_MMIO_CONFIG_GENERATION 0x0fc
#define VIRTIO_MMIO_CONFIG	      0x100

#define VIRTIO_MMIO_MAGIC_VALUE 0x74726976u /* "virt" in little-endian */
#define VIRTIO_MMIO_VERSION_2	2u

/* Device status bits (2.1). Writing 0 resets the device. */
#define VIRTIO_STATUS_ACKNOWLEDGE 1u
#define VIRTIO_STATUS_DRIVER	  2u
#define VIRTIO_STATUS_FEATURES_OK 8u
#define VIRTIO_STATUS_FAILED	  128u

/* Feature bits: 64 bits, read and written as two 32-bit halves selected by *_SEL. */
#define VIRTIO_F_VERSION_1 32u /* the device follows this spec, not the legacy one (6) */
#define VIRTIO_BLK_F_RO	   5u  /* the block device is read-only (5.2.3) */

/* Block device configuration space (5.2.4): capacity in sectors, a 64-bit field. */
#define VIRTIO_BLK_CFG_CAPACITY 0x00

static u64 read_features(uptr base)
{
	mmio_write32(base + VIRTIO_MMIO_DEVICE_FEAT_SEL, 1);
	u64 hi = mmio_read32(base + VIRTIO_MMIO_DEVICE_FEATURES);

	mmio_write32(base + VIRTIO_MMIO_DEVICE_FEAT_SEL, 0);
	return (hi << 32) | mmio_read32(base + VIRTIO_MMIO_DEVICE_FEATURES);
}

static void write_features(uptr base, u64 features)
{
	mmio_write32(base + VIRTIO_MMIO_DRIVER_FEAT_SEL, 0);
	mmio_write32(base + VIRTIO_MMIO_DRIVER_FEATURES, (u32)features);
	mmio_write32(base + VIRTIO_MMIO_DRIVER_FEAT_SEL, 1);
	mmio_write32(base + VIRTIO_MMIO_DRIVER_FEATURES, (u32)(features >> 32));
}

/*
 * A 64-bit configuration field, read as two 32-bit halves. The device may change its
 * configuration between the two reads, so read until the generation counter is the
 * same before and after (4.2.2.2, 2.5.1).
 */
static u64 read_config64(uptr base, u32 offset)
{
	u32 gen;
	u64 v;

	do {
		gen = mmio_read32(base + VIRTIO_MMIO_CONFIG_GENERATION);
		v = mmio_read32(base + VIRTIO_MMIO_CONFIG + offset);
		v |= (u64)mmio_read32(base + VIRTIO_MMIO_CONFIG + offset + 4) << 32;
	} while (gen != mmio_read32(base + VIRTIO_MMIO_CONFIG_GENERATION));
	return v;
}

/*
 * Driver initialisation, steps 1-6 of 3.1.1: reset, acknowledge, negotiate features.
 * We accept only what we use: VERSION_1 (required) and the read-only flag. Step 7,
 * setting DRIVER_OK, follows once the request queue exists.
 */
static bool blk_init(uptr base, struct virtio_blk *blk)
{
	mmio_write32(base + VIRTIO_MMIO_STATUS, 0);
	mmio_write32(base + VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
	mmio_write32(base + VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

	u64 offered = read_features(base);
	u64 wanted = (1ull << VIRTIO_F_VERSION_1) | (1ull << VIRTIO_BLK_F_RO);
	u32 status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;

	if (!(offered & (1ull << VIRTIO_F_VERSION_1))) {
		mmio_write32(base + VIRTIO_MMIO_STATUS, status | VIRTIO_STATUS_FAILED);
		return false;
	}
	write_features(base, offered & wanted);
	mmio_write32(base + VIRTIO_MMIO_STATUS, status | VIRTIO_STATUS_FEATURES_OK);
	/* The device clears FEATURES_OK if it cannot work with what we accepted. */
	if (!(mmio_read32(base + VIRTIO_MMIO_STATUS) & VIRTIO_STATUS_FEATURES_OK)) {
		mmio_write32(base + VIRTIO_MMIO_STATUS, status | VIRTIO_STATUS_FAILED);
		return false;
	}

	blk->base = base;
	blk->sectors = read_config64(base, VIRTIO_BLK_CFG_CAPACITY);
	blk->read_only = (offered & (1ull << VIRTIO_BLK_F_RO)) != 0;
	return true;
}

int virtio_mmio_probe(uptr base, u32 irq, struct virtio_blk *blk)
{
	if (mmio_read32(base + VIRTIO_MMIO_MAGIC) != VIRTIO_MMIO_MAGIC_VALUE ||
	    mmio_read32(base + VIRTIO_MMIO_VERSION) != VIRTIO_MMIO_VERSION_2)
		return -1;

	u32 id = mmio_read32(base + VIRTIO_MMIO_DEVICE_ID);

	if (id == VIRTIO_ID_BLOCK && blk) {
		if (!blk_init(base, blk))
			return -1;
		blk->irq = irq;
	}
	return (int)id;
}

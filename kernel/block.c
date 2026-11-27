/*
 * Project: OS-001
 * Date: 2026-11-27
 * Authors: kwan, oda, sokpheng001
 *
 * Block device table. Range and read-only checks are made here once, so every driver
 * gets only requests that fit its device.
 */

#include "block.h"

static const struct block_dev *devices[BLOCK_MAX_DEVICES];
static int ndevices;

bool block_register(const struct block_dev *dev)
{
	if (ndevices == BLOCK_MAX_DEVICES)
		return false;
	devices[ndevices++] = dev;
	return true;
}

int block_count(void)
{
	return ndevices;
}

const struct block_dev *block_get(int i)
{
	return i >= 0 && i < ndevices ? devices[i] : NULL;
}

/* True if count sectors from sector lie on dev (no overflow). */
static bool in_range(const struct block_dev *dev, u64 sector, u32 count)
{
	return dev && count > 0 && sector < dev->sectors && count <= dev->sectors - sector;
}

int block_read(const struct block_dev *dev, u64 sector, void *buf, u32 count)
{
	if (!in_range(dev, sector, count))
		return -1;
	return dev->read(dev->ctx, sector, buf, count);
}

int block_write(const struct block_dev *dev, u64 sector, const void *buf, u32 count)
{
	if (!in_range(dev, sector, count) || dev->read_only)
		return -1;
	return dev->write(dev->ctx, sector, buf, count);
}

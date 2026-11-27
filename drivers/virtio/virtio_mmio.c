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

#include "arch.h"
#include "drivers/virtio.h"
#include "mmio.h"
#include "pmm.h"

/* MMIO register offsets (4.2.2). */
#define VIRTIO_MMIO_MAGIC	      0x000
#define VIRTIO_MMIO_VERSION	      0x004
#define VIRTIO_MMIO_DEVICE_ID	      0x008
#define VIRTIO_MMIO_DEVICE_FEATURES   0x010
#define VIRTIO_MMIO_DEVICE_FEAT_SEL   0x014
#define VIRTIO_MMIO_DRIVER_FEATURES   0x020
#define VIRTIO_MMIO_DRIVER_FEAT_SEL   0x024
#define VIRTIO_MMIO_QUEUE_SEL	      0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX     0x034
#define VIRTIO_MMIO_QUEUE_NUM	      0x038
#define VIRTIO_MMIO_QUEUE_READY	      0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY      0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS  0x060
#define VIRTIO_MMIO_INTERRUPT_ACK     0x064
#define VIRTIO_MMIO_STATUS	      0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW    0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH   0x084
#define VIRTIO_MMIO_QUEUE_DRIVER_LOW  0x090 /* the available ring */
#define VIRTIO_MMIO_QUEUE_DRIVER_HIGH 0x094
#define VIRTIO_MMIO_QUEUE_DEVICE_LOW  0x0a0 /* the used ring */
#define VIRTIO_MMIO_QUEUE_DEVICE_HIGH 0x0a4
#define VIRTIO_MMIO_CONFIG_GENERATION 0x0fc
#define VIRTIO_MMIO_CONFIG	      0x100

#define VIRTIO_MMIO_MAGIC_VALUE 0x74726976u /* "virt" in little-endian */
#define VIRTIO_MMIO_VERSION_2	2u

/* Device status bits (2.1). Writing 0 resets the device. */
#define VIRTIO_STATUS_ACKNOWLEDGE 1u
#define VIRTIO_STATUS_DRIVER	  2u
#define VIRTIO_STATUS_DRIVER_OK	  4u
#define VIRTIO_STATUS_FEATURES_OK 8u
#define VIRTIO_STATUS_FAILED	  128u

/* Feature bits: 64 bits, read and written as two 32-bit halves selected by *_SEL. */
#define VIRTIO_F_VERSION_1 32u /* the device follows this spec, not the legacy one (6) */
#define VIRTIO_BLK_F_RO	   5u  /* the block device is read-only (5.2.3) */

/* Block device configuration space (5.2.4): capacity in sectors, a 64-bit field. */
#define VIRTIO_BLK_CFG_CAPACITY 0x00

/* The block device's only queue, "requestq" (5.2.2). */
#define VIRTIO_BLK_REQUESTQ 0u

/*
 * Split virtqueue (2.7): a descriptor table, the available ring (driver to device) and
 * the used ring (device to driver), all little-endian, here packed into one page with
 * the request header and status byte after them. Offsets meet each part's alignment
 * (16, 2 and 4 bytes, 2.7.2) and fit VIRTIO_BLK_QUEUE_SIZE entries.
 */
struct virtq_desc {
	u64 addr; /* physical address of the buffer */
	u32 len;
	u16 flags;
	u16 next; /* next descriptor in the chain, if VIRTQ_DESC_F_NEXT */
};

struct virtq_avail {
	u16 flags;
	u16 idx; /* where the driver puts the next entry (free-running) */
	u16 ring[VIRTIO_BLK_QUEUE_SIZE];
};

struct virtq_used_elem {
	u32 id; /* head descriptor of the finished chain */
	u32 len;
};

struct virtq_used {
	u16 flags;
	u16 idx; /* where the device puts the next entry (free-running) */
	struct virtq_used_elem ring[VIRTIO_BLK_QUEUE_SIZE];
};

#define VIRTQ_DESC_F_NEXT  1u /* the chain continues at next */
#define VIRTQ_DESC_F_WRITE 2u /* the device writes this buffer (otherwise it reads it) */

/* Block request (5.2.6): header, data, then one status byte the device writes. */
struct virtio_blk_req {
	u32 type;
	u32 reserved;
	u64 sector;
};

#define VIRTIO_BLK_T_IN	 0u /* read */
#define VIRTIO_BLK_T_OUT 1u /* write */
#define VIRTIO_BLK_S_OK	 0u

#define QUEUE_OFF_DESC	 0u
#define QUEUE_OFF_AVAIL	 256u
#define QUEUE_OFF_USED	 512u
#define QUEUE_OFF_REQ	 1024u
#define QUEUE_OFF_STATUS 1040u

/* Largest transfer one descriptor can describe: its length is 32 bits. */
#define VIRTIO_BLK_MAX_SECTORS (0xffffffffu / VIRTIO_BLK_SECTOR_SIZE)

/* Polls of the used ring before a request is declared lost (QEMU answers in far fewer). */
#define VIRTIO_BLK_POLL_LIMIT 100000000u

static void write_addr(uptr base, u32 low_reg, u64 addr)
{
	mmio_write32(base + low_reg, (u32)addr);
	mmio_write32(base + low_reg + 4, (u32)(addr >> 32));
}

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

	/* Step 7 (4.2.3.2): set up the request queue, then tell the device we are ready. */
	mmio_write32(base + VIRTIO_MMIO_QUEUE_SEL, VIRTIO_BLK_REQUESTQ);
	if (mmio_read32(base + VIRTIO_MMIO_QUEUE_READY) != 0 ||
	    mmio_read32(base + VIRTIO_MMIO_QUEUE_NUM_MAX) < VIRTIO_BLK_QUEUE_SIZE) {
		mmio_write32(base + VIRTIO_MMIO_STATUS, status | VIRTIO_STATUS_FAILED);
		return false;
	}
	blk->queue = pmm_alloc_page();
	if (!blk->queue) {
		mmio_write32(base + VIRTIO_MMIO_STATUS, status | VIRTIO_STATUS_FAILED);
		return false;
	}
	for (usize i = 0; i < PMM_PAGE_SIZE; i++)
		((volatile u8 *)blk->queue)[i] = 0;
	blk->avail_idx = 0;
	blk->used_idx = 0;
	blk->failed = false;
	mmio_write32(base + VIRTIO_MMIO_QUEUE_NUM, VIRTIO_BLK_QUEUE_SIZE);
	write_addr(base, VIRTIO_MMIO_QUEUE_DESC_LOW, blk->queue + QUEUE_OFF_DESC);
	write_addr(base, VIRTIO_MMIO_QUEUE_DRIVER_LOW, blk->queue + QUEUE_OFF_AVAIL);
	write_addr(base, VIRTIO_MMIO_QUEUE_DEVICE_LOW, blk->queue + QUEUE_OFF_USED);
	mmio_write32(base + VIRTIO_MMIO_QUEUE_READY, 1);
	mmio_write32(base + VIRTIO_MMIO_STATUS,
		     status | VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);
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

/*
 * One request at a time, polled: the chain is always descriptors 0-2, put in the
 * available ring, announced with a notify, then the used ring is polled until the
 * device hands it back (2.7.13, 2.7.14). IRQs stay masked meanwhile so no other thread
 * can start a request on the same queue. The device's DMA is cache-coherent on QEMU
 * virt (its Device Tree marks virtio-mmio "dma-coherent"), so barriers are enough.
 */
int virtio_blk_rw(struct virtio_blk *blk, u64 sector, void *buf, u32 count, bool write)
{
	if (!blk->queue || blk->failed || count == 0 || count > VIRTIO_BLK_MAX_SECTORS ||
	    sector >= blk->sectors || count > blk->sectors - sector || (write && blk->read_only))
		return -1;

	volatile struct virtq_desc *desc = (void *)(blk->queue + QUEUE_OFF_DESC);
	volatile struct virtq_avail *avail = (void *)(blk->queue + QUEUE_OFF_AVAIL);
	volatile struct virtq_used *used = (void *)(blk->queue + QUEUE_OFF_USED);
	volatile struct virtio_blk_req *req = (void *)(blk->queue + QUEUE_OFF_REQ);
	volatile u8 *status = (volatile u8 *)(blk->queue + QUEUE_OFF_STATUS);
	unsigned long flags = arch_irq_save();

	req->type = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
	req->reserved = 0;
	req->sector = sector;
	*status = 0xff; /* anything but OK, so a request the device never finished fails */

	desc[0].addr = (u64)(uptr)req;
	desc[0].len = sizeof(struct virtio_blk_req);
	desc[0].flags = VIRTQ_DESC_F_NEXT;
	desc[0].next = 1;
	desc[1].addr = (u64)(uptr)buf;
	desc[1].len = count * VIRTIO_BLK_SECTOR_SIZE;
	desc[1].flags = VIRTQ_DESC_F_NEXT | (write ? 0 : VIRTQ_DESC_F_WRITE);
	desc[1].next = 2;
	desc[2].addr = (u64)(uptr)status;
	desc[2].len = 1;
	desc[2].flags = VIRTQ_DESC_F_WRITE;
	desc[2].next = 0;

	avail->ring[blk->avail_idx % VIRTIO_BLK_QUEUE_SIZE] = 0;
	/* The device must see the chain and ring entry before the new index (2.7.13.3). */
	mmio_order();
	avail->idx = ++blk->avail_idx;
	/* ...and the new index before the notification (2.7.13.4). */
	mmio_barrier();
	mmio_write32(blk->base + VIRTIO_MMIO_QUEUE_NOTIFY, VIRTIO_BLK_REQUESTQ);

	u32 polls = 0;

	while (used->idx == blk->used_idx && ++polls < VIRTIO_BLK_POLL_LIMIT)
		;
	bool done = used->idx != blk->used_idx;

	if (done)
		blk->used_idx++;
	else
		blk->failed = true; /* the device still owns the chain: never reuse it */
	/* Read the status byte only after seeing the used index move. */
	mmio_order();
	int r = done && *status == VIRTIO_BLK_S_OK ? 0 : -1;

	/* No interrupt handler yet: clear whatever the device raised for this request. */
	mmio_write32(blk->base + VIRTIO_MMIO_INTERRUPT_ACK,
		     mmio_read32(blk->base + VIRTIO_MMIO_INTERRUPT_STATUS));
	arch_irq_restore(flags);
	return r;
}

/*
 * Project: OS-001
 * Date: 2026-11-27
 * Authors: kwan, oda, sokpheng001
 *
 * SD Host Controller driver, written from the SD Association's "SD Host Controller
 * Simplified Specification" (version 4.20; register sections 2.2.x) and "Physical
 * Layer Simplified Specification" (version 9.10; card identification 4.2, commands
 * 4.7, registers 5.x). The BCM2711 EMMC2 controller follows the SDHCI layout but its
 * registers must only be accessed 32 bits at a time (BCM2711 ARM Peripherals,
 * "EMMC"), so the 8- and 16-bit registers are reached through their 32-bit word.
 */

#include "arch.h"
#include "drivers/sdhci.h"
#include "mmio.h"

/* Registers (offsets of the 32-bit words that hold them). */
#define SDHCI_BLOCK	0x04 /* [11:0] block size, [31:16] block count */
#define SDHCI_ARGUMENT	0x08
#define SDHCI_CMD_XFER	0x0c /* [15:0] transfer mode, [31:16] command */
#define SDHCI_RESPONSE	0x10 /* four words: response bits [127:0] (2.2.7) */
#define SDHCI_PRESENT	0x24
#define SDHCI_HOST_CTRL 0x28 /* [7:0] host control 1, [15:8] power control */
#define SDHCI_CLOCK	0x2c /* [15:0] clock control, [23:16] timeout, [31:24] reset */
#define SDHCI_INT_STAT	0x30 /* [15:0] normal, [31:16] error interrupt status */
#define SDHCI_INT_EN	0x34 /* which interrupts are recorded in SDHCI_INT_STAT */
#define SDHCI_INT_SIG	0x38 /* which raise the interrupt line */
#define SDHCI_CAPS	0x40
#define SDHCI_DATA	0x20 /* buffer data port, one 32-bit word per access (2.2.8) */

/* Present state (2.2.9). */
#define SDHCI_PRESENT_CMD_INHIBIT (1u << 0)
#define SDHCI_PRESENT_DAT_INHIBIT (1u << 1)
#define SDHCI_PRESENT_CARD	  (1u << 16)

/* Power control (2.2.11), in bits [15:8] of SDHCI_HOST_CTRL. */
#define SDHCI_POWER_ON	(1u << 8)
#define SDHCI_POWER_3V3 (7u << 9)

/* Clock control (2.2.14), timeout (2.2.15) and software reset (2.2.16). */
#define SDHCI_CLOCK_INT_EN	 (1u << 0)
#define SDHCI_CLOCK_INT_STABLE	 (1u << 1)
#define SDHCI_CLOCK_CARD_EN	 (1u << 2)
#define SDHCI_CLOCK_DIV_LO_SHIFT 8 /* divider bits [7:0] */
#define SDHCI_CLOCK_DIV_HI_SHIFT 6 /* divider bits [9:8] */
#define SDHCI_CLOCK_DIV_MAX	 0x3ffu
#define SDHCI_TIMEOUT_MAX	 (0xeu << 16)
#define SDHCI_RESET_ALL		 (1u << 24)
#define SDHCI_RESET_CMD		 (1u << 25)
#define SDHCI_RESET_DAT		 (1u << 26)

/* Base clock in MHz, capabilities bits [15:8] (2.2.26). */
#define SDHCI_CAPS_BASE_CLOCK(c) (((c) >> 8) & 0xffu)

/* Interrupt status (2.2.17, 2.2.18). */
#define SDHCI_INT_CMD_DONE  (1u << 0)
#define SDHCI_INT_XFER_DONE (1u << 1)
#define SDHCI_INT_WRITE_RDY (1u << 4) /* the buffer can take a block */
#define SDHCI_INT_READ_RDY  (1u << 5) /* the buffer holds a block */
#define SDHCI_INT_ERROR	    (1u << 15)
#define SDHCI_INT_ALL	    0xffffffffu

/* Command register (2.2.6), in bits [31:16] of SDHCI_CMD_XFER. */
#define SDHCI_CMD_INDEX(n)    ((u32)(n) << 24)
#define SDHCI_CMD_RESP_NONE   (0u << 16)
#define SDHCI_CMD_RESP_136    (1u << 16)
#define SDHCI_CMD_RESP_48     (2u << 16)
#define SDHCI_CMD_RESP_48BUSY (3u << 16)
#define SDHCI_CMD_CRC	      (1u << 19)
#define SDHCI_CMD_IDX_CHECK   (1u << 20)
#define SDHCI_CMD_DATA	      (1u << 21) /* the command transfers data on DAT lines */

/* Transfer mode (2.2.5), in bits [15:0] of SDHCI_CMD_XFER: one block, no DMA. */
#define SDHCI_XFER_BLKCNT_EN (1u << 1)
#define SDHCI_XFER_READ	     (1u << 4) /* card to host */

/* Response formats (Physical Layer 4.9) as command flags. */
#define RESP_NONE SDHCI_CMD_RESP_NONE
#define RESP_R1	  (SDHCI_CMD_RESP_48 | SDHCI_CMD_CRC | SDHCI_CMD_IDX_CHECK)
#define RESP_R1B  (SDHCI_CMD_RESP_48BUSY | SDHCI_CMD_CRC | SDHCI_CMD_IDX_CHECK)
#define RESP_R2	  (SDHCI_CMD_RESP_136 | SDHCI_CMD_CRC)
#define RESP_R3	  SDHCI_CMD_RESP_48 /* OCR: no CRC or index to check */
#define RESP_R6	  RESP_R1
#define RESP_R7	  RESP_R1

/* SD commands used here (Physical Layer 4.7.4). ACMDs follow CMD55. */
#define SD_CMD_GO_IDLE	     0
#define SD_CMD_ALL_SEND_CID  2
#define SD_CMD_SEND_REL_ADDR 3
#define SD_CMD_SELECT	     7
#define SD_CMD_SEND_IF_COND  8
#define SD_CMD_SEND_CSD	     9
#define SD_CMD_SET_BLOCKLEN  16
#define SD_CMD_READ_BLOCK    17
#define SD_CMD_WRITE_BLOCK   24
#define SD_CMD_APP	     55
#define SD_ACMD_SEND_OP_COND 41

/* CMD8 argument: 2.7-3.6 V supply (bit 8) and a check pattern echoed back (4.3.13). */
#define SD_IF_COND_ARG	0x1aau
#define SD_IF_COND_ECHO 0xfffu
/* ACMD41: we support high capacity (HCS) and the 3.2-3.4 V window (5.1, OCR). */
#define SD_OCR_HCS	(1u << 30)
#define SD_OCR_3V3	(3u << 20)
#define SD_OCR_CCS	(1u << 30) /* card capacity status, valid once powered up */
#define SD_OCR_POWERED	(1u << 31)
#define SD_OCR_RETRIES	1000u

/* Identification clock: at most 400 kHz; then default speed, 25 MHz (Physical Layer 6.6.6). */
#define SD_ID_CLOCK_HZ	    400000u
#define SD_DEFAULT_CLOCK_HZ 25000000u

/* Register polls before giving up (QEMU answers at once; real cards within ms). */
#define SDHCI_POLL_LIMIT 10000000u

static u32 rd(const struct sdhci *h, u32 reg)
{
	return mmio_read32(h->base + reg);
}

static void wr(const struct sdhci *h, u32 reg, u32 v)
{
	mmio_write32(h->base + reg, v);
}

/* Wait until (reg & mask) == want; false on timeout. */
static bool wait_for(const struct sdhci *h, u32 reg, u32 mask, u32 want)
{
	for (u32 i = 0; i < SDHCI_POLL_LIMIT; i++)
		if ((rd(h, reg) & mask) == want)
			return true;
	return false;
}

/*
 * Wait for the interrupt status bit, then clear it. False on an error interrupt or a
 * timeout; the command and data lines are then reset (2.2.16) so the next command
 * starts clean.
 */
static bool wait_int(const struct sdhci *h, u32 bit)
{
	for (u32 i = 0; i < SDHCI_POLL_LIMIT; i++) {
		u32 st = rd(h, SDHCI_INT_STAT);

		if (st & SDHCI_INT_ERROR)
			break;
		if (st & bit) {
			wr(h, SDHCI_INT_STAT, bit);
			return true;
		}
	}
	wr(h, SDHCI_INT_STAT, SDHCI_INT_ALL);
	wr(h, SDHCI_CLOCK, rd(h, SDHCI_CLOCK) | SDHCI_RESET_CMD | SDHCI_RESET_DAT);
	wait_for(h, SDHCI_CLOCK, SDHCI_RESET_CMD | SDHCI_RESET_DAT, 0);
	return false;
}

/*
 * Send command index with arg and flags (response format, plus data and transfer mode
 * bits for a data command); store the response words in resp (four for R2, else one).
 * False if the controller is stuck or reports an error.
 */
static bool command(const struct sdhci *h, u32 index, u32 arg, u32 flags, u32 *resp)
{
	u32 inhibit = SDHCI_PRESENT_CMD_INHIBIT;

	if ((flags & SDHCI_CMD_RESP_48BUSY) == SDHCI_CMD_RESP_48BUSY || (flags & SDHCI_CMD_DATA))
		inhibit |= SDHCI_PRESENT_DAT_INHIBIT;
	if (!wait_for(h, SDHCI_PRESENT, inhibit, 0))
		return false;
	wr(h, SDHCI_INT_STAT, SDHCI_INT_ALL);
	wr(h, SDHCI_ARGUMENT, arg);
	wr(h, SDHCI_CMD_XFER, SDHCI_CMD_INDEX(index) | flags);
	if (!wait_int(h, SDHCI_INT_CMD_DONE))
		return false;
	if (resp) {
		int words = (flags & SDHCI_CMD_RESP_48BUSY) == SDHCI_CMD_RESP_136 ? 4 : 1;

		for (int i = 0; i < words; i++)
			resp[i] = rd(h, SDHCI_RESPONSE + 4 * (u32)i);
	}
	/* R1b: the card holds DAT0 low while busy. */
	if ((flags & SDHCI_CMD_RESP_48BUSY) == SDHCI_CMD_RESP_48BUSY)
		return wait_for(h, SDHCI_PRESENT, SDHCI_PRESENT_DAT_INHIBIT, 0);
	return true;
}

/* Run the SD clock at no more than hz, from the base clock in the capabilities. */
static bool set_clock(const struct sdhci *h, u32 hz)
{
	u32 base_mhz = SDHCI_CAPS_BASE_CLOCK(rd(h, SDHCI_CAPS));

	if (base_mhz == 0)
		return false; /* base clock not reported: it would have to come from firmware */

	/* SDHCI 3.0 10-bit divided clock mode: f = base / (2 * div), div 0 = base. */
	u32 base_hz = base_mhz * 1000000u;
	u32 div = (base_hz + 2 * hz - 1) / (2 * hz);

	if (div > SDHCI_CLOCK_DIV_MAX)
		div = SDHCI_CLOCK_DIV_MAX;

	u32 v = rd(h, SDHCI_CLOCK) & ~0xffffu;

	v |= ((div & 0xffu) << SDHCI_CLOCK_DIV_LO_SHIFT) |
	     (((div >> 8) & 0x3u) << SDHCI_CLOCK_DIV_HI_SHIFT) | SDHCI_CLOCK_INT_EN |
	     SDHCI_TIMEOUT_MAX;
	wr(h, SDHCI_CLOCK, v);
	if (!wait_for(h, SDHCI_CLOCK, SDHCI_CLOCK_INT_STABLE, SDHCI_CLOCK_INT_STABLE))
		return false;
	wr(h, SDHCI_CLOCK, rd(h, SDHCI_CLOCK) | SDHCI_CLOCK_CARD_EN);
	return true;
}

/*
 * Capacity from the CSD register (Physical Layer 5.3). The controller strips the CRC,
 * so CSD bit n is response bit n - 8 (SDHCI 2.2.7).
 */
static u64 csd_blocks(const u32 r[4])
{
	u32 structure = (r[3] >> 22) & 0x3u; /* CSD [127:126] */

	if (structure == 1) {
		/* Version 2.0 (SDHC/SDXC): C_SIZE [69:48], capacity (C_SIZE + 1) * 512 KiB. */
		u64 c_size = (r[1] >> 8) & 0x3fffffu;

		return (c_size + 1) * 1024u;
	}
	/* Version 1.0 (SDSC): (C_SIZE + 1) * 2^(C_SIZE_MULT + 2) * 2^READ_BL_LEN bytes. */
	u32 read_bl_len = (r[2] >> 8) & 0xfu;		   /* CSD [83:80] */
	u32 c_size = ((r[2] & 0x3u) << 10) | (r[1] >> 22); /* CSD [73:62] */
	u32 c_size_mult = (r[1] >> 7) & 0x7u;		   /* CSD [49:47] */
	u64 bytes = (u64)(c_size + 1) << (c_size_mult + 2 + read_bl_len);

	return bytes / SDHCI_BLOCK_SIZE;
}

/* Card identification (Physical Layer 4.2, figure 4-2), ending with the card selected. */
static bool identify(struct sdhci *h)
{
	u32 r[4];

	if (!command(h, SD_CMD_GO_IDLE, 0, RESP_NONE, NULL))
		return false;
	/* Only version 2.00+ cards answer CMD8; those are the only ones we accept. */
	if (!command(h, SD_CMD_SEND_IF_COND, SD_IF_COND_ARG, RESP_R7, r) ||
	    (r[0] & SD_IF_COND_ECHO) != SD_IF_COND_ARG)
		return false;

	u32 ocr = 0;

	for (u32 i = 0; i < SD_OCR_RETRIES && !(ocr & SD_OCR_POWERED); i++) {
		if (!command(h, SD_CMD_APP, 0, RESP_R1, NULL) ||
		    !command(h, SD_ACMD_SEND_OP_COND, SD_OCR_HCS | SD_OCR_3V3, RESP_R3, &ocr))
			return false;
	}
	if (!(ocr & SD_OCR_POWERED))
		return false;
	h->high_cap = (ocr & SD_OCR_CCS) != 0;

	if (!command(h, SD_CMD_ALL_SEND_CID, 0, RESP_R2, r) ||
	    !command(h, SD_CMD_SEND_REL_ADDR, 0, RESP_R6, r))
		return false;
	h->rca = r[0] >> 16;

	if (!command(h, SD_CMD_SEND_CSD, h->rca << 16, RESP_R2, r))
		return false;
	h->blocks = csd_blocks(r);

	if (!command(h, SD_CMD_SELECT, h->rca << 16, RESP_R1B, NULL))
		return false;
	/* SDSC cards take a block length; SDHC/SDXC are fixed at 512 bytes. */
	if (!h->high_cap && !command(h, SD_CMD_SET_BLOCKLEN, SDHCI_BLOCK_SIZE, RESP_R1, NULL))
		return false;
	/* Identification is over: every card supports default speed. */
	return h->blocks > 0 && set_clock(h, SD_DEFAULT_CLOCK_HZ);
}

bool sdhci_init(struct sdhci *host)
{
	host->present = false;
	host->blocks = 0;

	/* Reset everything, then power the bus at 3.3 V and start the slow ID clock. */
	wr(host, SDHCI_CLOCK, rd(host, SDHCI_CLOCK) | SDHCI_RESET_ALL);
	if (!wait_for(host, SDHCI_CLOCK, SDHCI_RESET_ALL, 0))
		return false;
	if (!(rd(host, SDHCI_PRESENT) & SDHCI_PRESENT_CARD))
		return false;
	wr(host, SDHCI_HOST_CTRL, SDHCI_POWER_ON | SDHCI_POWER_3V3);
	/* Record every event in the status register but raise no interrupt: we poll. */
	wr(host, SDHCI_INT_EN, SDHCI_INT_ALL);
	wr(host, SDHCI_INT_SIG, 0);
	if (!set_clock(host, SD_ID_CLOCK_HZ))
		return false;

	host->present = identify(host);
	return host->present;
}

/*
 * One block through the buffer data port (SDHCI 3.7.2, transfer without DMA): the
 * command, then a buffer-ready event, then 128 words, then transfer complete. SDSC
 * cards are addressed in bytes, SDHC/SDXC in blocks (Physical Layer 4.3.14).
 */
static bool transfer_block(const struct sdhci *h, u64 block, u8 *buf, bool write)
{
	u32 addr = (u32)(h->high_cap ? block : block * SDHCI_BLOCK_SIZE);
	u32 flags = RESP_R1 | SDHCI_CMD_DATA | SDHCI_XFER_BLKCNT_EN;

	wr(h, SDHCI_BLOCK, SDHCI_BLOCK_SIZE | (1u << 16));
	if (!command(h, write ? SD_CMD_WRITE_BLOCK : SD_CMD_READ_BLOCK, addr,
		     flags | (write ? 0 : SDHCI_XFER_READ), NULL) ||
	    !wait_int(h, write ? SDHCI_INT_WRITE_RDY : SDHCI_INT_READ_RDY))
		return false;
	/* Byte by byte on the memory side: buf need not be 4-byte aligned. */
	for (u32 i = 0; i < SDHCI_BLOCK_SIZE; i += 4) {
		if (write) {
			wr(h, SDHCI_DATA,
			   (u32)buf[i] | (u32)buf[i + 1] << 8 | (u32)buf[i + 2] << 16 |
			       (u32)buf[i + 3] << 24);
		} else {
			u32 w = rd(h, SDHCI_DATA);

			buf[i] = (u8)w;
			buf[i + 1] = (u8)(w >> 8);
			buf[i + 2] = (u8)(w >> 16);
			buf[i + 3] = (u8)(w >> 24);
		}
	}
	/* For a write this also waits until the card has finished programming. */
	return wait_int(h, SDHCI_INT_XFER_DONE);
}

int sdhci_rw(struct sdhci *host, u64 block, void *buf, u32 count, bool write)
{
	/* SDSC byte addresses are 32 bits, so such cards end below 4 GB anyway. */
	if (!host->present || count == 0 || block >= host->blocks || count > host->blocks - block)
		return -1;

	/* One transfer at a time on the controller: no other thread may start one. */
	unsigned long flags = arch_irq_save();
	bool ok = true;

	for (u32 i = 0; ok && i < count; i++)
		ok = transfer_block(host, block + i, (u8 *)buf + i * SDHCI_BLOCK_SIZE, write);
	arch_irq_restore(flags);
	return ok ? 0 : -1;
}

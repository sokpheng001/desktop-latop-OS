/*
 * Project: OS-001
 * Date: 2026-11-27
 * Authors: kwan, oda, sokpheng001
 *
 * SD Host Controller (SDHCI) driver with one SD card, polled (no DMA, no interrupts).
 */

#ifndef OS001_DRIVERS_SDHCI_H
#define OS001_DRIVERS_SDHCI_H

#include "types.h"

#ifndef __ASSEMBLY__

/* Bytes per block: the only block length used (SD Physical Layer spec, 4.3.3). */
#define SDHCI_BLOCK_SIZE 512u

struct sdhci {
	uptr base;     /* controller registers */
	bool present;  /* a card was found and identified */
	bool high_cap; /* SDHC/SDXC: block addressing; otherwise SDSC: byte addressing */
	u32 rca;       /* the card's relative address, set by CMD3 */
	u64 blocks;    /* card capacity in SDHCI_BLOCK_SIZE blocks */
};

/*
 * Reset the controller at host->base, power the bus and identify the card: on
 * success host->present, high_cap, rca and blocks are set and the card is selected
 * (transfer state), ready for block transfers. Returns false if the controller does
 * not respond, has no card, or the card does not answer as an SD card.
 */
bool sdhci_init(struct sdhci *host);

/*
 * Read (write = false) or write count blocks starting at block, to or from buf, which
 * holds count * SDHCI_BLOCK_SIZE bytes (any alignment). Polled, one block per command. Returns 0,
 * or -1 if the range is outside the card or the card or controller reports an error.
 */
int sdhci_rw(struct sdhci *host, u64 block, void *buf, u32 count, bool write);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_DRIVERS_SDHCI_H */

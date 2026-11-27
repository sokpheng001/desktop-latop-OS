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

#endif /* !__ASSEMBLY__ */

#endif /* OS001_DRIVERS_SDHCI_H */

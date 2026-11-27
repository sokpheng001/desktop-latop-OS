<!--
  Project: OS-001
  Date: 2026-11-25
  Authors: kwan, oda, sokpheng001
-->

# `board/rpi4/`

Raspberry Pi 4 board: linker script (load address 0x80000), `board.mk` driver list, board HAL glue, and the SD card (EMMC2, or the older EMMC controller where QEMU attaches it; QEMU runs with a 1 MiB test card).

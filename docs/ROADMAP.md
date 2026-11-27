<!--
  Project: OS-001
  Date: 2026-11-25
  Authors: kwan, oda, sokpheng001
-->

# OS-001 — Roadmap

Work moves top to bottom. Each `[ ]` item is one small, testable step.
When a whole phase is done, builds cleanly and passes its tests, it is merged
from `dev` into `main` and released with the version shown.

Coarse items (later phases) are split into small steps before work on them starts.

## Phase 1 — First boot (v0.1.0)
- [x] Folder structure (`arch/`, `board/`, `kernel/`, `drivers/`, `lib/`, `include/`, `tests/`) and Makefile following CONTRIBUTING.md §4: out-of-tree `build/<board>/`, `BOARD`, `CROSS_COMPILE`, `V=1`, `-MMD -MP`, `-Werror`, targets `all run debug test format check-format clean distclean help`
- [x] `include/types.h`, `include/compiler.h`, `include/mmio.h` (fixed-width types, attributes, MMIO helpers)
- [x] Linker scripts: `virt` (load at 0x40080000) and `rpi4` (load at 0x80000)
- [x] `arch/aarch64/boot.S`: park secondary cores, set up the stack, zero BSS, call `kernel_main`
- [x] HAL interface `include/hal.h`: `hal_init`, `hal_console_putc`, `hal_console_getc`
- [x] PL011 UART driver; `board/virt` (UART at 0x09000000) and `board/rpi4` (UART at 0xFE201000)
- [x] `lib/`: `memset`, `memcpy`, `strlen`, `kprintf` (%s %c %d %u %x %p)
- [x] `kernel_main` prints the OS-001 banner; boots in QEMU on both boards
- [x] `tests/boot_test.sh` + `make test`: boots each board in QEMU headless and checks the banner appears
- [x] GitHub Actions CI (`.github/workflows/ci.yml`): on Ubuntu with `aarch64-linux-gnu-gcc` + `qemu-system-aarch64`, run `make check-format`, build both boards, `make test`

## Phase 2 — Exceptions and time (v0.2.0)
- [x] Drop from EL2 to EL1 at boot
- [x] Exception vector table; print ESR/ELR/FAR on any unexpected exception
- [x] Interrupt controller: GIC on `virt`, GIC-400 on `rpi4`, behind the HAL
- [x] ARM generic timer interrupt (100 Hz tick) and uptime counter
- [x] Tests for each of the above in `tests/`

## Phase 3 — Memory (v0.3.0)
- [x] Read RAM size from the Device Tree passed by firmware / QEMU
- [x] Physical page allocator (4 KB pages)
- [x] MMU on: identity-mapped page tables for RAM (cached) and devices, caches enabled
- [x] Kernel mapped with least privilege: code read-only, data never executable, page 0 unmapped
- [x] Kernel heap: `kmalloc` / `kfree`

## Phase 4 — Processes and permissions (v0.4.0)
- [x] Kernel threads and context switch
- [x] Round-robin scheduler driven by the timer
- [x] User mode (EL0) and system calls: programs in their own pages, `write` / `yield` / `exit`, a faulting program is killed, not the kernel
- [x] Per-process address spaces: each process gets its own page tables
- [x] Per-process permission table checked by the kernel (privacy pillar)
- [x] Message-passing IPC between processes

## Phase 5 — Storage (v0.5.0)
- [ ] virtio-blk driver (`virt`)
- [ ] SD card driver (`rpi4`)
- [ ] FAT32 read support; load a program from disk

## Phase 6 — Screen and input (v0.6.0)
- [ ] Framebuffer: `ramfb` / virtio-gpu (`virt`), mailbox framebuffer (`rpi4`)
- [ ] Bitmap font and on-screen text console
- [ ] Keyboard input (virtio-input on `virt`; USB later)
- [ ] Simple shell

## Phase 7 and beyond
- [ ] SMP: use all CPU cores
- [ ] x86_64 port (UEFI boot)
- [ ] Semantic file index and local AI service (see VISION.md)
- [ ] Networking, USB, audio, laptop power management

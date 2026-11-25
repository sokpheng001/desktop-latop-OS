<!--
  Project: OS-001
  Authors: kwan, oda, sokpheng001
-->

# OS-001 — Roadmap

Work moves top to bottom. Each `[ ]` item is one small, testable step.
When a whole phase is done, builds cleanly and passes its tests, it is merged
from `dev` into `main` and released with the version shown.

Coarse items (later phases) are split into small steps before work on them starts.

## Phase 1 — First boot (v0.1.0)
- [ ] Folder structure (`arch/`, `board/`, `kernel/`, `drivers/`, `lib/`, `include/`, `tests/`) and Makefile with `BOARD=virt|rpi4` and `all`, `run`, `debug`, `clean` targets
- [ ] Linker scripts: `virt` (load at 0x40080000) and `rpi4` (load at 0x80000)
- [ ] `arch/aarch64/boot.S`: park secondary cores, set up the stack, zero BSS, call `kernel_main`
- [ ] HAL interface `include/hal.h`: `hal_init`, `hal_console_putc`, `hal_console_getc`
- [ ] PL011 UART driver; `board/virt` (UART at 0x09000000) and `board/rpi4` (UART at 0xFE201000)
- [ ] `lib/`: `memset`, `memcpy`, `strlen`, `kprintf` (%s %c %d %u %x %p)
- [ ] `kernel_main` prints the OS-001 banner; boots in QEMU on both boards
- [ ] `tests/boot_test.sh`: boots each board in QEMU headless and checks the banner appears

## Phase 2 — Exceptions and time (v0.2.0)
- [ ] Drop from EL2 to EL1 at boot
- [ ] Exception vector table; print ESR/ELR/FAR on any unexpected exception
- [ ] Interrupt controller: GIC on `virt`, GIC-400 on `rpi4`, behind the HAL
- [ ] ARM generic timer interrupt (100 Hz tick) and uptime counter
- [ ] Tests for each of the above in `tests/`

## Phase 3 — Memory (v0.3.0)
- [ ] Read RAM size from the Device Tree passed by firmware / QEMU
- [ ] Physical page allocator (4 KB pages)
- [ ] MMU on: page tables, kernel mapping, device memory mapping
- [ ] Kernel heap: `kmalloc` / `kfree`

## Phase 4 — Processes and permissions (v0.4.0)
- [ ] Kernel threads and context switch
- [ ] Round-robin scheduler driven by the timer
- [ ] User mode (EL0) and system calls
- [ ] Per-process permission table checked by the kernel (privacy pillar)
- [ ] Message-passing IPC between processes

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

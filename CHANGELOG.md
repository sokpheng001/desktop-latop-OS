<!--
  Project: OS-001
  Date: 2026-11-25
  Authors: kwan, oda, sokpheng001
-->

# Changelog

All notable changes to OS-001 are recorded in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Progress report for 2026-11-26 (`docs/progress/2026-11-26.md`)
- Kernel threads (`kernel/thread.c`): `thread_create`, `thread_yield`, `thread_exit` with a FIFO run queue; the boot code becomes thread 0 ("main"); each thread gets a 16 KB stack from the heap, freed after it exits. Threads can also give up the CPU early with `thread_yield`
- AArch64 context switch (`arch/aarch64/context.S`): saves only the AAPCS64 callee-saved registers (x19-x30) on the outgoing thread's stack, so a parked thread is one stack pointer; `arch_irq_save` / `arch_irq_restore`
- Preemptive round-robin scheduling: each thread gets a slice of 2 timer ticks (20 ms); when it runs out, the switch happens at the end of interrupt handling, after every interrupt is completed, on the interrupted thread's own stack (`thread_tick`, `thread_preempt`)
- `virt-sched` / `rpi4-sched` boot runs: three threads that never yield must each get regular turns from the timer, then stop and be freed
- User mode (`kernel/user.c`): `user_spawn` copies a position-independent program into fresh pages mapped at a user address (read and execute for the program, never executed by the kernel), gives it a stack page and runs it at EL0 in its own thread. Each program runs in its own address space
- System calls through `svc` (`include/syscall.h`): `write`, `exit`, `yield`; every pointer is checked against the caller's own pages (`-EFAULT`), unknown numbers return `-ENOSYS`. A fault in a user program kills only that program and frees its pages; interrupts and preemption work while user code runs (each thread's `SP_EL0` travels in its exception frame)
- Per-process address spaces: each program gets its own top-level page table that shares the kernel's mappings, plus its own user tables, all freed when it ends. Every program now uses the same layout (code at 256 GB, stack at the top of a 1 GB window). User pages are non-global and each space has its own ASID (`TTBR0_EL1[63:48]`), so switching threads switches page tables without flushing the TLB; an ending space's ASID is flushed before reuse (`arch_mmu_space_create`, `arch_mmu_space_destroy`, `arch_mmu_switch`)
- `user_spawn` passes an argument to the program in x0; `thread_create_user` sets a thread's program and address space before it can run
- `arch_mmu_map_user`, `arch_mmu_unmap_user` (with TLB invalidation), `arch_sync_icache`, `arch_enter_user`; `console_write`
- `virt-user` / `rpi4-user` boot runs: a program says hello, yields, has a bad pointer and an unknown call refused and exits with code 7; programs that write their own code or read kernel memory are killed; two copies of one program at the same addresses keep separate memory; two rounds leave no pages, page tables or heap memory behind
- `virt-thread` / `rpi4-thread` boot runs: three threads must run in turn (ABCABCABC), keep their registers across switches and be freed on exit

### Changed
- Version string set to `0.4.0-dev` for Phase 4 work

### Fixed
- Assembly files now see `-DKERNEL_TEST_<name>` like C files (`ASFLAGS` follows `CFLAGS`)

### Planned
- Phase 4 (v0.4.0): kernel threads and context switch, round-robin scheduler, user mode and system calls, per-process permissions, message-passing IPC

## [0.3.0] - 2026-11-26

Memory: the kernel finds its RAM in the Device Tree, hands it out in 4 KB pages, runs with the
MMU and caches on under a least-privilege mapping (code read-only, data never executable, NULL
unmapped) and has a heap with `kmalloc` / `kfree`.

### Added
- RAM discovery from the Device Tree: Flattened Devicetree reader `lib/fdt.c` (written from the Devicetree Specification v0.4, chapter 5) and `kernel/mem.c`; the banner lists every RAM range and the total
- arm64 kernel image header at the start of the image: QEMU and the Pi firmware load it as a kernel and pass the Device Tree address in x0
- `board/rpi4/qemu.dts`, built with `dtc` into `build/rpi4/qemu.dtb` for QEMU runs (QEMU raspi4b only passes a DTB when given one)
- Host unit tests for the Device Tree reader (`tests/fdt_test.dts`); boot tests check the RAM each board reports
- Physical page allocator `kernel/pmm.c`: one bitmap bit per 4 KB page, placed in free RAM; single pages by next-fit, contiguous runs, double and misaligned frees refused. It manages all RAM except the kernel image, the memory below it (loader stubs and the Pi spin table), the Device Tree and its `/memreserve/` entries; the banner prints the free and total page counts
- `fdt_reserved_regions`: reads the Device Tree memory reservation block (Devicetree Specification v0.4, section 5.3)
- Page allocator tests: host unit tests on fake RAM with two ranges, a gap and unaligned reserved ranges; `virt-pmm` and `rpi4-pmm` boot runs take every free page, write it and return it
- MMU on (`arch/aarch64/mmu.c`, `kernel/vm.c`): 4 KB granule, 39-bit identity map built from the page allocator, RAM as cached normal memory, device registers as uncached Device-nGnRnE and never executable, everything else unmapped; data and instruction caches on. Ranges use the largest block that fits (1 GB, 2 MB, 4 KB), so virt needs 2 table pages and the Pi 3. The banner prints an `MMU:` line
- Kernel mapped with least privilege: code read-only and executable, constants read-only, data and all other RAM never executable, page 0 unmapped so NULL pointers fault; `SCTLR_EL1.WXN` makes every writable mapping non-executable as a second line of defence. The linker script exports `kernel_rodata_start` and `kernel_data_start`
- Boot runs `rodata`, `nx` and `null` on both boards: writing a constant, jumping into data and reading address 0 must each be reported with the right fault and address
- Kernel heap `kernel/heap.c`: `kmalloc`, `kzalloc` and `kfree` with 16-byte alignment. Seven size classes (32 to 2048-byte blocks) carved from pages, after Bonwick's slab allocator (USENIX 1994); larger requests get whole contiguous pages. A header on every block lets `kfree` work without a size and refuse double or foreign frees
- Heap tests: host unit tests (class layout, reuse, zeroing, refused frees, exhaustion) and `virt-heap` / `rpi4-heap` boot runs (512 objects of every size, twice, with pattern checks and page reuse)
- `hal_mmio_ranges`: each board lists its device register ranges for the MMU
- Progress report for 2026-11-25 (`docs/progress/2026-11-25.md`) covering the whole first working day

### Changed
- QEMU runs and boot tests use the raw `kernel8.img` on both boards (the ELF is still built for debugging)
- `dtc` is now a build requirement; CI installs `device-tree-compiler`
- The abort self-test now loads from an unmapped address and expects a level 1 translation fault (a misaligned load no longer faults once RAM is normal memory)

## [0.2.0] - 2026-11-25

Exceptions and time: the kernel runs at EL1 on both boards, reports any CPU exception with a
decoded register dump, handles interrupts through the GIC and keeps a 100 Hz tick.

### Added
- Boot drops from EL2 to EL1 when the loader enters at EL2 (Pi 4 firmware, QEMU `raspi4b`, `virt` with virtualization): EL1 runs AArch64 with no traps to EL2, timer and counter usable from EL1, interrupts masked
- AArch64 system register constants (`include/arch/aarch64/sysregs.h`) and architecture interface `include/arch.h` (`arch_current_el`); the boot banner shows the exception level
- Boot test checks the kernel runs at EL1 and adds a `virt-el2` run that enters at EL2
- EL1 exception vector table (`arch/aarch64/vectors.S`): every exception saves a full register frame and calls a C handler; unexpected exceptions print their type, ESR (with decoded exception class), ELR, FAR, SPSR and x0-x30, then halt the core
- `arch_exception_init`, `arch_halt` in the architecture interface; `TRAP()` in `compiler.h`
- Kernel self-tests built with `make KERNEL_TEST=<name>` into `build/<board>-test-<name>/`; first one, `fault`, raises a breakpoint after boot
- Boot test runs `virt-fault` and `rpi4-fault` and checks the exception report
- GICv2 interrupt controller driver (`drivers/gicv2/`, from Arm IHI 0048B): distributor and CPU interface reset, enable/disable, acknowledge/end-of-interrupt, SGIs; used for QEMU `virt` (GICv2 at 0x08000000) and the Pi 4 GIC-400 (0xFF840000)
- HAL interrupt API (`hal_irq_enable`, `hal_irq_disable`, `hal_irq_claim`, `hal_irq_complete`) and kernel IRQ dispatch with a per-interrupt handler table (`kernel/irq.c`, `irq_register`)
- IRQ exceptions are dispatched and return to the interrupted code; `arch_irq_enable`, `arch_irq_disable`, `arch_wait_for_interrupt`; IRQs are unmasked after boot
- Self-test `KERNEL_TEST=irq` and boot-test runs `virt-irq`, `rpi4-irq`: a software interrupt must arrive through the GIC and return
- Arm generic timer driver (`drivers/arm_gtimer/`): EL1 physical timer, drift-free periodic deadlines, system counter read-out; HAL timer/counter API (`hal_timer_*`, `hal_counter`, `hal_counter_freq`) on both boards (timer interrupt ID 30)
- Kernel time (`kernel/time.c`): 100 Hz tick counter and uptime in milliseconds from the hardware counter; the banner shows tick rate and counter frequency
- `kernel_main` no longer returns: after boot it sleeps in `wfi` waiting for interrupts
- Self-test `KERNEL_TEST=timer` and boot-test runs `virt-timer`, `rpi4-timer`: 50 ticks must take 500 ms (within 10%) by the hardware counter
- Exception reports decode the fault status code of instruction and data aborts (translation, access flag, permission, external, alignment)
- Self-test `KERNEL_TEST=abort` and boot-test runs `virt-abort`, `rpi4-abort`: a misaligned load must be reported as an alignment-fault data abort with FAR equal to the address
- IRQ self-test also checks that an interrupt without a handler is reported once and disabled; `hal_irq_raise` (SGI or software set-pending), `hal_irq_count`, `gicv2_set_pending`, `irq_unhandled_count`, `HIDE_VALUE()`
- Test overview in `tests/README.md`

## [0.1.0] - 2026-11-25

First boot: OS-001 boots on QEMU `virt` and Raspberry Pi 4 (`raspi4b`) and prints its banner over the PL011 UART.

### Added
- Roadmap ([docs/ROADMAP.md](docs/ROADMAP.md)): phases 1-7 broken into small steps
- Branch workflow: daily work on `dev`, finished and tested phases merged to `main`
- Contribution rules ([CONTRIBUTING.md](CONTRIBUTING.md)): from-scratch originality policy, author header on every file, C17 code style, industry-standard Makefile conventions, testing and git workflow
- `.editorconfig` and `.clang-format` shared editor and formatting settings
- Daily progress reports in `docs/progress/`
- README: project timeline (2024 – present) and copyright line
- Research foundations ([docs/RESEARCH.md](docs/RESEARCH.md)): 26 verified papers mapped to OS-001 subsystems and roadmap phases
- Source folder structure (`arch/`, `board/`, `kernel/`, `drivers/`, `lib/`, `include/`, `tests/`), each with a README describing what belongs there
- `Makefile`: out-of-tree builds in `build/<board>/`, `BOARD`, `CROSS_COMPILE`, `V=1`, `CFLAGS_EXTRA`, `-MMD -MP` header dependencies, `-Werror`, reproducible output (`-ffile-prefix-map`, no build ID), per-board driver selection via `board/<board>/board.mk`; targets `all run debug test format check-format clean distclean help`
- Base headers: `include/types.h` (fixed-width types with size checks), `include/compiler.h` (attribute and helper macros), `include/mmio.h` (8/16/32/64-bit MMIO accessors and barriers)
- Linker scripts: shared AArch64 layout `arch/aarch64/kernel.ld` (page-aligned sections, separate R-X / R / RW segments, 16 KB boot stack, `bss_start`/`bss_end` symbols) included by `board/virt/linker.ld` (0x40080000) and `board/rpi4/linker.ld` (0x80000); linker warnings are fatal
- `arch/aarch64/boot.S`: parks secondary cores, sets the boot stack, clears `.bss`, passes the Device Tree address to `kernel_main`
- HAL interface `include/hal.h`: `hal_init`, `hal_console_putc`, `hal_console_getc`
- PL011 UART driver (`drivers/pl011/`): polled 8N1, baud divisor from the reference clock; HAL for `board/virt` (UART at 0x09000000) and `board/rpi4` (UART at 0xFE201000, GPIO 14/15 routed to UART0); `kernel_main` calls `hal_init`
- `lib/`: `memset`, `memcpy`, `strlen` and `kprintf` (`%s %c %d %u %x %p %%`, `l` modifier, field width and zero padding) with an output sink set by the kernel
- Host unit tests for `lib/` (`tests/lib_test.sh`), run by `make test`
- Boot banner: `kernel_main` prints the OS-001 version, board, kernel image range, Device Tree address and "Hello from OS-001!"; kernel console (`kernel/console.c`) and `hal_board_name`
- Boot test `tests/boot_test.sh`: builds each board, boots it headless in QEMU and checks the banner; `make test` runs it
- GitHub Actions CI (`.github/workflows/ci.yml`): in Debian trixie (QEMU 10), format check, build both boards with `aarch64-linux-gnu-`, `make test`; boot logs uploaded
- `include/kernel.h` and a minimal `kernel/main.c` (`kernel_main` entry point)
- Vision: OS-001 is both a complete normal OS and a human-AI system ([docs/VISION.md](docs/VISION.md))

### Changed
- Working hours for daily progress reports: Monday to Friday, 5 PM to 11:59 PM
- File header now records the date each file was first written (`Date: YYYY-MM-DD` above `Authors`)

### Fixed
- Linux cross toolchain (`aarch64-linux-gnu-`): `.bss` and the boot stack are placed in the RW segment, so no empty segment is emitted at address 0
- QEMU `virt` runs with `-nic none`: no network card (and no option ROM) is needed yet
- Host unit test builds cleanly with GCC's format checker

## [0.0.1] - 2026-11-25

### Added
- Project vision ([docs/VISION.md](docs/VISION.md)): an AI-native, private, lightweight desktop OS with on-device AI, no telemetry, and tiered hardware support
- Hardware targets and requirements ([docs/HARDWARE.md](docs/HARDWARE.md)): Raspberry Pi and QEMU first, then x86_64 PCs; Apple Silicon Macs supported as a VM guest only
- `README.md`, `AUTHORS`, `.gitignore`
- This changelog

[Unreleased]: https://github.com/sokpheng001/desktop-latop-OS/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/sokpheng001/desktop-latop-OS/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/sokpheng001/desktop-latop-OS/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/sokpheng001/desktop-latop-OS/compare/v0.0.1...v0.1.0
[0.0.1]: https://github.com/sokpheng001/desktop-latop-OS/releases/tag/v0.0.1

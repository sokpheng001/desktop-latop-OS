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
- RAM discovery from the Device Tree: Flattened Devicetree reader `lib/fdt.c` (written from the Devicetree Specification v0.4, chapter 5) and `kernel/mem.c`; the banner lists every RAM range and the total
- arm64 kernel image header at the start of the image: QEMU and the Pi firmware load it as a kernel and pass the Device Tree address in x0
- `board/rpi4/qemu.dts`, built with `dtc` into `build/rpi4/qemu.dtb` for QEMU runs (QEMU raspi4b only passes a DTB when given one)
- Host unit tests for the Device Tree reader (`tests/fdt_test.dts`); boot tests check the RAM each board reports
- Progress report for 2026-11-25 (`docs/progress/2026-11-25.md`) covering the whole first working day

### Changed
- QEMU runs and boot tests use the raw `kernel8.img` on both boards (the ELF is still built for debugging)
- `dtc` is now a build requirement; CI installs `device-tree-compiler`
- Version string set to `0.3.0-dev` for Phase 3 work

### Planned
- Phase 3 (v0.3.0): RAM size from the Device Tree, physical page allocator, MMU on, kernel heap

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

[Unreleased]: https://github.com/sokpheng001/desktop-latop-OS/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/sokpheng001/desktop-latop-OS/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/sokpheng001/desktop-latop-OS/compare/v0.0.1...v0.1.0
[0.0.1]: https://github.com/sokpheng001/desktop-latop-OS/releases/tag/v0.0.1

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
- `include/kernel.h` and a minimal `kernel/main.c` (`kernel_main` entry point)
- Vision: OS-001 is both a complete normal OS and a human-AI system ([docs/VISION.md](docs/VISION.md))

### Changed
- Working hours for daily progress reports: Monday to Friday, 5 PM to 11:59 PM
- File header now records the date each file was first written (`Date: YYYY-MM-DD` above `Authors`)

### Planned
- Phase 1: boot code (AArch64 assembly), `kernel_main`, UART output, "Hello from OS-001!" in QEMU
- Board support: Raspberry Pi 4 (`raspi4b`) and QEMU `virt`
- Hardware abstraction layer (HAL)

## [0.0.1] - 2026-11-25

### Added
- Project vision ([docs/VISION.md](docs/VISION.md)): an AI-native, private, lightweight desktop OS with on-device AI, no telemetry, and tiered hardware support
- Hardware targets and requirements ([docs/HARDWARE.md](docs/HARDWARE.md)): Raspberry Pi and QEMU first, then x86_64 PCs; Apple Silicon Macs supported as a VM guest only
- `README.md`, `AUTHORS`, `.gitignore`
- This changelog

[Unreleased]: https://github.com/sokpheng001/desktop-latop-OS/compare/v0.0.1...HEAD
[0.0.1]: https://github.com/sokpheng001/desktop-latop-OS/releases/tag/v0.0.1

<!--
  Project: OS-001
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

### Planned
- Phase 1: boot code (AArch64 assembly), `kernel_main`, UART output, "Hello from OS-001!" in QEMU
- Board support: Raspberry Pi 4 (`raspi4b`) and QEMU `virt`
- Hardware abstraction layer (HAL) and project folder structure
- Makefile with `make BOARD=<board> run` and `debug` targets

## [0.0.1] - 2026-11-25

### Added
- Project vision ([docs/VISION.md](docs/VISION.md)): an AI-native, private, lightweight desktop OS with on-device AI, no telemetry, and tiered hardware support
- Hardware targets and requirements ([docs/HARDWARE.md](docs/HARDWARE.md)): Raspberry Pi and QEMU first, then x86_64 PCs; Apple Silicon Macs supported as a VM guest only
- `README.md`, `AUTHORS`, `.gitignore`
- This changelog

[Unreleased]: https://github.com/sokpheng001/desktop-latop-OS/compare/v0.0.1...HEAD
[0.0.1]: https://github.com/sokpheng001/desktop-latop-OS/releases/tag/v0.0.1

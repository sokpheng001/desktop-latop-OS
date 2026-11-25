<!--
  Project: OS-001
  Date: 2026-11-25
  Authors: kwan, oda, sokpheng001
-->

# OS-001

A private, lightweight desktop OS where AI is built in and runs entirely on your own machine.
Our own kernel, written from scratch. Raspberry Pi first, then x86_64 PCs, and VMs everywhere.

**Project timeline:** 2024 – present

- [Vision](docs/VISION.md): purpose and architecture
- [Hardware](docs/HARDWARE.md): target platforms and requirements
- [Roadmap](docs/ROADMAP.md): planned work, step by step
- [Changelog](CHANGELOG.md): history of changes
- [Research](docs/RESEARCH.md): papers behind the design
- [Contributing](CONTRIBUTING.md): code, build and originality rules
- [Daily progress](docs/progress/): what was done each working day

## Build and run

Needs `aarch64-elf-gcc` (or `aarch64-linux-gnu-gcc` with `CROSS_COMPILE=aarch64-linux-gnu-`),
`qemu-system-aarch64` (9.0 or newer for the Raspberry Pi 4 machine) and `clang-format`.

```sh
make                 # build for QEMU virt (default)
make run             # boot it in QEMU; quit with Ctrl-A then X
make BOARD=rpi4 run  # Raspberry Pi 4: build/rpi4/kernel8.img
make test            # unit tests and QEMU boot tests for every board
make help            # all targets and options
```

## Authors

kwan, oda, sokpheng001 (see [AUTHORS](AUTHORS))

Copyright © 2024–present kwan, oda, sokpheng001. All rights reserved.

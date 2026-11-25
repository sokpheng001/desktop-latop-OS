<!--
  Project: OS-001
  Authors: kwan, oda, sokpheng001
-->

# OS-001 — Hardware Targets

OS-001 is a **desktop/laptop** OS (not mobile). One codebase builds a separate kernel per platform.

## Target platforms

| Priority | Platform | CPU | Boot | Role |
|---|---|---|---|---|
| 1 | Raspberry Pi 4 / 5 | AArch64 | Pi firmware | First real-hardware boot |
| 1 | QEMU `virt` (and UTM on Apple Silicon Macs) | AArch64 | Direct / UEFI | Daily development; near-native speed on Apple Silicon |
| 2 | x86_64 desktops and laptops | x86_64 | UEFI | Main real-world desktop target |
| 3 | ARM laptops (e.g. Snapdragon X) | AArch64 | UEFI + ACPI | Later |
| ✗ | Apple Silicon Macs (bare metal) | AArch64 | iBoot | Not planned: undocumented hardware. Supported as a **VM guest** only |

No legacy BIOS. Pi firmware or UEFI only.

## Minimum requirements

| Component | Base OS | AI-lite | AI-full |
|---|---|---|---|
| CPU | 64-bit, 1 core | 4 cores with NEON / AVX2 | 4+ cores |
| RAM | 512 MB | 4 GB | 8 GB+ |
| Storage | 1 GB | 8 GB | 16 GB+ |
| Display | HDMI / built-in panel (framebuffer) | same | same |
| Input | USB keyboard + mouse | same | same |

## Hardware support roadmap

1. Serial console (UART / COM port): debug output
2. Timer + interrupts (ARM generic timer + GIC; x86 APIC)
3. MMU / paging
4. Framebuffer display
5. USB keyboard and mouse (HID)
6. Storage: SD card (Pi), virtio-blk (VM), then AHCI / NVMe (PC)
7. File system
8. Multi-core (SMP), needed for AI performance
9. Vector math (NEON / AVX) in user space, for AI inference
10. Networking: Ethernet, then Wi-Fi
11. Laptop features: ACPI power, battery, sleep/wake
12. Audio
13. GPU compute, for faster AI (long-term)

## Source layout

```
arch/<cpu>/      CPU-specific: boot assembly, exceptions, MMU
board/<board>/   Board-specific: addresses, HAL implementation
kernel/          Portable core: never touches hardware directly
drivers/         Device drivers
lib/             Our own libc subset (printf, memcpy, ...)
docs/            Design documents
```

Build: `make BOARD=rpi4`, `make BOARD=virt`, `make BOARD=pc` (later).

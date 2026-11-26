<!--
  Project: OS-001
  Date: 2026-11-25
  Authors: kwan, oda, sokpheng001
-->

# `tests/`

`make test` runs every `tests/*.sh`. All tests run headless and need only the cross toolchain,
QEMU and a host C compiler. CI runs the same command.

## Host unit tests: `lib_test.sh`

Builds `lib/` and the page allocator for the development machine and checks `memset`, `memcpy`,
`strlen`, the `kprintf` formatter (every conversion, width, padding, 64-bit limits), the Device
Tree reader against `fdt_test.dts` (64-bit cells, several memory nodes and ranges, nested `reg`
properties, `/memreserve/` entries, a damaged header) and `kernel/pmm.c` on fake RAM (two ranges
with a gap, unaligned and out-of-range reservations, every page handed out exactly once,
contiguous runs, refused bad frees).

## Boot tests: `boot_test.sh`

Each run builds a kernel, boots it in QEMU and checks the serial console. Every run must print
the banner, the board name, `EL: 1`, the RAM found in the Device Tree, the page count and the timer line. Kernels
boot as raw images, so QEMU passes the Device Tree in x0: its own for `virt`, and the one built
from `board/rpi4/qemu.dts` for the Pi (QEMU fills in the memory node). Self-test kernels are built with
`make KERNEL_TEST=<name>` (see `kernel/selftest.c`) into `build/<board>-test-<name>/`.

| Run | What it proves |
|---|---|
| `virt`, `rpi4` | Normal boot on both boards; console, GIC and timer come up |
| `virt-el2` | `virt` with virtualization: entered at EL2, the kernel drops to EL1 (the Pi is always entered at EL2) |
| `virt-fault`, `rpi4-fault` | A breakpoint reaches the exception vectors and is reported (EC `0x3c`) |
| `virt-abort`, `rpi4-abort` | A misaligned load is reported as a data abort, alignment fault, with FAR = that address |
| `virt-irq`, `rpi4-irq` | A software interrupt goes through the GIC, IRQ vector and dispatch and returns; an interrupt with no handler is reported once and disabled |
| `virt-timer`, `rpi4-timer` | 50 timer ticks take 500 ms (within 10%) by the hardware counter |
| `virt-pmm`, `rpi4-pmm` | Every free page is handed out once, written and returned, none inside the kernel; a double or misaligned free is refused; a 16-page run works |

Run a subset with `RUNS="virt-irq rpi4-irq" sh tests/boot_test.sh`. Console logs are kept in
`build/test/`.

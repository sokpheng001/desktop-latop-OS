<!--
  Project: OS-001
  Date: 2026-11-25
  Authors: kwan, oda, sokpheng001
-->

# `tests/`

`make test` runs every `tests/*.sh`. All tests run headless and need only the cross toolchain,
QEMU and a host C compiler. CI runs the same command.

## Host unit tests: `lib_test.sh`

Builds `lib/` for the development machine and checks `memset`, `memcpy`, `strlen` and the
`kprintf` formatter (every conversion, width, padding, 64-bit limits).

## Boot tests: `boot_test.sh`

Each run builds a kernel, boots it in QEMU and checks the serial console. Every run must print
the banner, the board name, `EL: 1` and the timer line. Self-test kernels are built with
`make KERNEL_TEST=<name>` (see `kernel/selftest.c`) into `build/<board>-test-<name>/`.

| Run | What it proves |
|---|---|
| `virt`, `rpi4` | Normal boot on both boards; console, GIC and timer come up |
| `virt-el2` | `virt` with virtualization: entered at EL2, the kernel drops to EL1 (the Pi is always entered at EL2) |
| `virt-fault`, `rpi4-fault` | A breakpoint reaches the exception vectors and is reported (EC `0x3c`) |
| `virt-abort`, `rpi4-abort` | A misaligned load is reported as a data abort, alignment fault, with FAR = that address |
| `virt-irq`, `rpi4-irq` | A software interrupt goes through the GIC, IRQ vector and dispatch and returns; an interrupt with no handler is reported once and disabled |
| `virt-timer`, `rpi4-timer` | 50 timer ticks take 500 ms (within 10%) by the hardware counter |

Run a subset with `RUNS="virt-irq rpi4-irq" sh tests/boot_test.sh`. Console logs are kept in
`build/test/`.

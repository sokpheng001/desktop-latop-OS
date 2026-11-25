<!--
  Project: OS-001
  Authors: kwan, oda, sokpheng001
-->

# Contributing to OS-001

These rules apply to every change, by every contributor.

## 1. Originality: our own kernel, written from scratch

- All kernel, driver, library and boot code is **written by us for OS-001**.
- **Do not copy or clone code** from other kernels or projects (Linux, BSD, xv6, seL4, Redox, SerenityOS, tutorials, Stack Overflow, etc.), not even "with small changes".
- Reading public **specifications and manuals** is expected: ARM Architecture Reference Manual, BCM2711 peripherals, PL011 / GIC TRMs, Device Tree spec, virtio spec, UEFI/ACPI specs. Implement from the spec, in our own words and structure.
- Reading **research papers** is encouraged (see [docs/RESEARCH.md](docs/RESEARCH.md)). Reuse ideas, never code. When a design follows a paper, cite it in a comment, e.g. `/* Design follows Liedtke, "On micro-kernel construction" (SOSP 1995). */`
- Third-party code may only enter later, in **user space**, after an explicit team decision. It must be kept under `third_party/` with its original license, never mixed into our own sources.

## 2. Author header: every file

Every file we write starts with this header, in that file's comment syntax:

```c
/*
 * Project: OS-001
 * Authors: kwan, oda, sokpheng001
 *
 * <one or two lines: what this file does>
 * SPDX-License-Identifier: <license, once chosen>
 */
```

- Assembly (`.S`), linker scripts (`.ld`): same `/* ... */` form.
- Makefile, shell, Python: `#` comments.
- Markdown: an HTML comment `<!-- ... -->`.

## 3. Code style (C and assembly)

- Language: **C17**, freestanding (`-ffreestanding -nostdlib`). No compiler extensions except where documented (`__attribute__` in `include/compiler.h`).
- Formatting is defined by [`.clang-format`](.clang-format) (Linux-kernel-like: tabs, width 8, 100 columns). Run `make format` before committing.
- Naming: `snake_case` for functions and variables, `UPPER_CASE` for macros and constants, module prefix on public symbols (`uart_`, `gic_`, `pmm_`, `sched_`).
- Fixed-width types from `include/types.h` (`u8`, `u32`, `u64`, `usize`). Hardware registers are accessed only through `mmio_read32` / `mmio_write32` helpers.
- One responsibility per file. Public interfaces live in `include/`, private helpers are `static`.
- Comment the **why**, not the what. Every hardware magic number gets a named constant and a reference to the manual section.
- No hardware access in `kernel/`: go through the HAL (`include/hal.h`).

## 4. Build system (industry conventions)

- `make` with **out-of-tree builds**: all output goes to `build/<board>/`; the source tree stays clean.
- Standard targets: `all` (default), `run`, `debug`, `test`, `format`, `check-format`, `clean`, `distclean`, `help`.
- Standard variables, overridable from the command line:
  - `BOARD=virt|rpi4` (default `virt`)
  - `CROSS_COMPILE=aarch64-elf-` (CI on Linux uses `aarch64-linux-gnu-`)
  - `V=1` for verbose commands (quiet `CC  file.c` lines by default)
  - `CFLAGS_EXTRA` for local experiments
- Warnings: `-Wall -Wextra -Werror -std=c17`. Header dependencies are generated with `-MMD -MP`.
- Build must be **reproducible**: no timestamps or absolute paths embedded in the binary.
- Every build artifact (`*.o`, `*.elf`, `*.img`) is ignored by git.

## 5. Testing

- `make test` boots every board headless in QEMU and checks the expected output (`tests/`).
- Continuous integration (GitHub Actions) runs build + tests for all boards on every push to `dev` and `main`.
- No commit may break the build or the tests.

## 6. Git workflow

- `dev`: daily work. Every commit builds and passes tests.
- `main`: only finished, tested roadmap phases, merged from `dev` with `--ff-only` and tagged `vX.Y.Z`.
- Commit messages: imperative subject ≤ 72 characters ("Add PL011 UART driver"), blank line, body explaining why.
- Commit authorship: author `kwan`, with `Co-authored-by: oda` and `Co-authored-by: sokpheng001` trailers.
- Every change updates [CHANGELOG.md](CHANGELOG.md) under `## [Unreleased]`.
- Never force-push or rewrite published history.

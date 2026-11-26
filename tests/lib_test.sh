#!/bin/sh
# Project: OS-001
# Date: 2026-11-25
# Authors: kwan, oda, sokpheng001
#
# Builds lib/, the page allocator and the heap for the host and runs their unit tests (with a test DTB from dtc). Our memset/memcpy/strlen are
# renamed so they do not collide with the host C library linked into the test.

set -eu

HOSTCC=${HOSTCC:-cc}
DTC=${DTC:-dtc}
OUT=build/host
mkdir -p "$OUT"

$HOSTCC -std=c17 -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin \
    -Dmemset=lib_memset -Dmemcpy=lib_memcpy -Dstrlen=lib_strlen \
    -Iinclude \
    tests/lib_test.c lib/string.c lib/kprintf.c lib/fdt.c kernel/pmm.c kernel/heap.c -o "$OUT/lib_test"

$DTC -q -I dts -O dtb -o "$OUT/fdt_test.dtb" tests/fdt_test.dts

"$OUT/lib_test" "$OUT/fdt_test.dtb"

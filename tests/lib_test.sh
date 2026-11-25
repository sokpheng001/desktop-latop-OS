#!/bin/sh
# Project: OS-001
# Date: 2026-11-25
# Authors: kwan, oda, sokpheng001
#
# Builds lib/ for the host and runs its unit tests. Our memset/memcpy/strlen are
# renamed so they do not collide with the host C library linked into the test.

set -eu

HOSTCC=${HOSTCC:-cc}
OUT=build/host
mkdir -p "$OUT"

$HOSTCC -std=c17 -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin \
    -Dmemset=lib_memset -Dmemcpy=lib_memcpy -Dstrlen=lib_strlen \
    -Iinclude \
    tests/lib_test.c lib/string.c lib/kprintf.c -o "$OUT/lib_test"

"$OUT/lib_test"

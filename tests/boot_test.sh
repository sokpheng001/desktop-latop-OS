#!/bin/sh
# Project: OS-001
# Date: 2026-11-25
# Authors: kwan, oda, sokpheng001
#
# Boot test: builds every board, boots it headless in QEMU and checks the serial
# console for the boot banner and the exception level. QEMU never exits on its
# own, so each run is stopped as soon as the banner appears, or after
# BOOT_TIMEOUT seconds.
#
# A run is "<board>" or "<board>-<variant>"; virt-el2 boots virt with the
# virtualization extensions, so the kernel is entered at EL2 like on a Pi.

set -u

QEMU=${QEMU:-qemu-system-aarch64}
BOOT_TIMEOUT=${BOOT_TIMEOUT:-10}
RUNS=${RUNS:-"virt virt-el2 rpi4"}
EXPECT="Hello from OS-001!"
LOG_DIR=build/test
MAKE=${MAKE:-make}

mkdir -p "$LOG_DIR"
status=0

qemu_args() {
    case "$1" in
    virt) echo "-M virt -cpu cortex-a72 -m 1G -nic none -kernel build/virt/os001.elf" ;;
    virt-el2) echo "-M virt,virtualization=on -cpu cortex-a72 -m 1G -nic none -kernel build/virt/os001.elf" ;;
    rpi4) echo "-M raspi4b -kernel build/rpi4/kernel8.img" ;;
    *) return 1 ;;
    esac
}

board_name() {
    case "$1" in
    virt) echo "qemu-virt" ;;
    rpi4) echo "raspberry-pi-4" ;;
    esac
}

for run in $RUNS; do
    board=${run%%-*}
    log="$LOG_DIR/boot-$run.log"
    : > "$log"

    if ! $MAKE -s BOARD="$board" > "$LOG_DIR/build-$board.log" 2>&1; then
        echo "  FAIL    boot $run: build failed (see $LOG_DIR/build-$board.log)"
        status=1
        continue
    fi

    # shellcheck disable=SC2046 # word splitting of the QEMU arguments is intended
    $QEMU $(qemu_args "$run") -display none -monitor none -serial "file:$log" \
        2> "$LOG_DIR/qemu-$run.err" &
    pid=$!

    waited=0
    while [ "$waited" -lt "$BOOT_TIMEOUT" ] && ! grep -qF "$EXPECT" "$log"; do
        sleep 1
        waited=$((waited + 1))
    done
    kill "$pid" 2>/dev/null
    wait "$pid" 2>/dev/null

    # The kernel must always end up at EL1, whatever level the loader entered at.
    if grep -qF "$EXPECT" "$log" && grep -qF "Board:  $(board_name "$board")" "$log" &&
        grep -qF "EL:     1" "$log"; then
        echo "  PASS    boot $run"
    else
        echo "  FAIL    boot $run: banner, board or EL1 not seen within ${BOOT_TIMEOUT}s. Console output:"
        sed 's/^/          | /' "$log"
        status=1
    fi
done

exit $status

#!/bin/sh
# Project: OS-001
# Date: 2026-11-25
# Authors: kwan, oda, sokpheng001
#
# Boot test: builds each kernel, boots it headless in QEMU and checks the serial
# console. QEMU never exits on its own, so each run is stopped as soon as its
# last expected line appears, or after BOOT_TIMEOUT seconds.
#
# A run is "<board>" or "<board>-<variant>":
#   el2    boot virt with virtualization on, so the kernel is entered at EL2
#   fault  self-test kernel (make KERNEL_TEST=fault): a deliberate exception must
#          be reported by the exception handler

set -u

QEMU=${QEMU:-qemu-system-aarch64}
BOOT_TIMEOUT=${BOOT_TIMEOUT:-10}
RUNS=${RUNS:-"virt virt-el2 rpi4 virt-fault rpi4-fault"}
LOG_DIR=build/test
MAKE=${MAKE:-make}

mkdir -p "$LOG_DIR"
status=0

board_name() {
    case "$1" in
    virt) echo "qemu-virt" ;;
    rpi4) echo "raspberry-pi-4" ;;
    esac
}

# Lines every kernel must print; the kernel must always end up at EL1.
common_expect() {
    printf '%s\n' "Board:  $(board_name "$1")" "EL:     1" "Hello from OS-001!"
}

for run in $RUNS; do
    board=${run%%-*}
    variant=
    case "$run" in *-*) variant=${run#*-} ;; esac

    make_args="BOARD=$board"
    build_dir="build/$board"
    machine_opts=
    expect=$(common_expect "$board")
    case "$variant" in
    "") ;;
    el2) machine_opts=",virtualization=on" ;;
    fault)
        make_args="$make_args KERNEL_TEST=fault"
        build_dir="build/$board-test-fault"
        expect=$(printf '%s\n' "$expect" \
            "*** Unexpected exception: Synchronous (current EL, SP_ELx)" \
            "EC=0x3c Breakpoint (BRK)" \
            "*** System halted.")
        ;;
    *)
        echo "  FAIL    boot $run: unknown variant '$variant'"
        status=1
        continue
        ;;
    esac

    case "$board" in
    virt) qemu_args="-M virt$machine_opts -cpu cortex-a72 -m 1G -nic none -kernel $build_dir/os001.elf" ;;
    rpi4) qemu_args="-M raspi4b$machine_opts -kernel $build_dir/kernel8.img" ;;
    *)
        echo "  FAIL    boot $run: unknown board '$board'"
        status=1
        continue
        ;;
    esac

    log="$LOG_DIR/boot-$run.log"
    : > "$log"

    # shellcheck disable=SC2086 # make_args holds several words on purpose
    if ! $MAKE -s $make_args > "$LOG_DIR/build-$run.log" 2>&1; then
        echo "  FAIL    boot $run: build failed (see $LOG_DIR/build-$run.log)"
        status=1
        continue
    fi

    # shellcheck disable=SC2086 # qemu_args holds several words on purpose
    $QEMU $qemu_args -display none -monitor none -serial "file:$log" \
        2> "$LOG_DIR/qemu-$run.err" &
    pid=$!

    last=$(printf '%s\n' "$expect" | tail -n 1)
    waited=0
    while [ "$waited" -lt "$BOOT_TIMEOUT" ] && ! grep -qF -- "$last" "$log"; do
        sleep 1
        waited=$((waited + 1))
    done
    kill "$pid" 2>/dev/null
    wait "$pid" 2>/dev/null

    missing=$(printf '%s\n' "$expect" | while IFS= read -r line; do
        grep -qF -- "$line" "$log" || echo "$line"
    done)
    if [ -z "$missing" ]; then
        echo "  PASS    boot $run"
    else
        echo "  FAIL    boot $run: missing within ${BOOT_TIMEOUT}s:"
        printf '%s\n' "$missing" | sed 's/^/          - /'
        echo "          console output:"
        sed 's/^/          | /' "$log"
        status=1
    fi
done

exit $status

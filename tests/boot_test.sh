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
#   abort  self-test kernel (make KERNEL_TEST=abort): a load from an unmapped address
#          must be reported as a data abort (translation fault) with FAR = the address
#   rodata self-test kernel: a write to a kernel constant must be a permission fault
#   nx     self-test kernel: a jump into kernel data must be an instruction abort
#          (permission fault): data is never executable
#   null   self-test kernel: a load from address 0 must be a translation fault
#   irq    self-test kernel (make KERNEL_TEST=irq): a software interrupt must go
#          through the interrupt controller, the IRQ vector and dispatch, and return;
#          an interrupt without a handler must be reported once and disabled
#   timer  self-test kernel (make KERNEL_TEST=timer): 50 timer ticks must arrive at
#          100 Hz, checked against the hardware counter
#   heap   self-test kernel (make KERNEL_TEST=heap): objects of every size class and
#          whole pages must not overlap, be reused after kfree, and bad frees refused
#   thread self-test kernel (make KERNEL_TEST=thread): three threads must take turns
#          in order, keep their registers across switches, and be freed on exit
#   sched  self-test kernel (make KERNEL_TEST=sched): three threads that never yield
#          must all get regular turns from the timer, then stop and be freed
#   pmm    self-test kernel (make KERNEL_TEST=pmm): every free page must be handed out
#          once, written, and returned; double and misaligned frees are refused

set -u

QEMU=${QEMU:-qemu-system-aarch64}
BOOT_TIMEOUT=${BOOT_TIMEOUT:-10}
RUNS=${RUNS:-"virt virt-el2 rpi4 virt-fault rpi4-fault virt-abort rpi4-abort virt-irq rpi4-irq virt-timer rpi4-timer virt-pmm rpi4-pmm virt-rodata rpi4-rodata virt-nx rpi4-nx virt-null rpi4-null virt-heap rpi4-heap virt-thread rpi4-thread virt-sched rpi4-sched"}
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

# RAM the kernel must find in the Device Tree. virt: QEMU's own DTB (-m 1G).
# rpi4: QEMU fills the memory node with the RAM below the VideoCore split.
board_ram() {
    case "$1" in
    virt) echo "RAM:    1024 MiB in 1 region" ;;
    rpi4) echo "RAM:    960 MiB in 1 region" ;;
    esac
}

# Pages the allocator tracks: all of the RAM above, in 4 KB pages.
board_pages() {
    case "$1" in
    virt) echo "free of 262144 (4 KB each)" ;;
    rpi4) echo "free of 245760 (4 KB each)" ;;
    esac
}

# Lines every kernel must print; the kernel must always end up at EL1.
common_expect() {
    printf '%s\n' "Board:  $(board_name "$1")" "EL:     1" "$(board_ram "$1")" \
        "$(board_pages "$1")" "MMU:    on, caches on" "Hello from OS-001!" \
        "Timer:  100 Hz tick"
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
    abort)
        make_args="$make_args KERNEL_TEST=abort"
        build_dir="build/$board-test-abort"
        expect=$(printf '%s\n' "$expect" \
            "*** Unexpected exception: Synchronous (current EL, SP_ELx)" \
            "EC=0x25 Data abort from current EL" \
            "FSC:      0x05 Translation fault, level 1" \
            "*** System halted.")
        ;;
    irq)
        make_args="$make_args KERNEL_TEST=irq"
        build_dir="build/$board-test-irq"
        expect=$(printf '%s\n' "$expect" "Self-test: IRQ 1 received and returned" \
            "has no handler; disabling it" \
            "reported once and disabled")
        ;;
    timer)
        make_args="$make_args KERNEL_TEST=timer"
        build_dir="build/$board-test-timer"
        expect=$(printf '%s\n' "$expect" "Self-test: timer OK")
        ;;
    rodata)
        make_args="$make_args KERNEL_TEST=rodata"
        build_dir="build/$board-test-rodata"
        expect=$(printf '%s\n' "$expect" \
            "EC=0x25 Data abort from current EL" \
            "FSC:      0x0f Permission fault, level 3" \
            "*** System halted.")
        ;;
    nx)
        make_args="$make_args KERNEL_TEST=nx"
        build_dir="build/$board-test-nx"
        expect=$(printf '%s\n' "$expect" \
            "EC=0x21 Instruction abort from current EL" \
            "FSC:      0x0f Permission fault, level 3" \
            "*** System halted.")
        ;;
    null)
        make_args="$make_args KERNEL_TEST=null"
        build_dir="build/$board-test-null"
        expect=$(printf '%s\n' "$expect" \
            "EC=0x25 Data abort from current EL" \
            "Translation fault" \
            "*** System halted.")
        ;;
    thread)
        make_args="$make_args KERNEL_TEST=thread"
        build_dir="build/$board-test-thread"
        expect=$(printf '%s\n' "$expect" "Self-test: threads OK (order ABCABCABC")
        ;;
    sched)
        make_args="$make_args KERNEL_TEST=sched"
        build_dir="build/$board-test-sched"
        expect=$(printf '%s\n' "$expect" "Self-test: preemptive scheduler OK")
        ;;
    heap)
        make_args="$make_args KERNEL_TEST=heap"
        build_dir="build/$board-test-heap"
        expect=$(printf '%s\n' "$expect" "Self-test: heap OK")
        ;;
    pmm)
        make_args="$make_args KERNEL_TEST=pmm"
        build_dir="build/$board-test-pmm"
        expect=$(printf '%s\n' "$expect" "Self-test: page allocator OK")
        ;;
    *)
        echo "  FAIL    boot $run: unknown variant '$variant'"
        status=1
        continue
        ;;
    esac

    # Raw images: QEMU follows the arm64 boot protocol and passes the Device Tree in x0.
    case "$board" in
    virt) qemu_args="-M virt$machine_opts -cpu cortex-a72 -m 1G -nic none -kernel $build_dir/kernel8.img" ;;
    rpi4) qemu_args="-M raspi4b$machine_opts -kernel $build_dir/kernel8.img -dtb $build_dir/qemu.dtb" ;;
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
    # A fault report must name the exact address the self-test touched.
    case "$variant" in abort | rodata | nx | null) check_far=1 ;; *) check_far= ;; esac
    if [ -n "$check_far" ]; then
        addr=$(tr -d '\r' < "$log" | sed -n 's/^Self-test: .* at \(0x[0-9a-f]*\)$/\1/p' | head -n 1)
        if [ -z "$addr" ] || ! grep -qF "FAR_EL1:  $addr" "$log"; then
            missing=$(printf '%s\n' "$missing" "FAR_EL1 equal to the address the self-test touched")
            missing=${missing#"${missing%%[![:space:]]*}"}
        fi
    fi
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

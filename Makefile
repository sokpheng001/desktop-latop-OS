# Project: OS-001
# Date: 2026-11-25
# Authors: kwan, oda, sokpheng001
#
# Top-level build. Out-of-tree: everything is written to build/<board>/.
# Run `make help` for targets and variables.

# ---------------------------------------------------------------------------
# User-overridable variables
# ---------------------------------------------------------------------------
BOARD         ?= virt
CROSS_COMPILE ?= aarch64-elf-
V             ?= 0
CFLAGS_EXTRA  ?=
KERNEL_TEST   ?=
QEMU          ?= qemu-system-aarch64
CLANG_FORMAT  ?= clang-format

BOARDS := virt rpi4
ifeq ($(filter $(BOARD),$(BOARDS)),)
$(error Unknown BOARD '$(BOARD)'. Supported: $(BOARDS))
endif

ARCH := aarch64

# ---------------------------------------------------------------------------
# Quiet / verbose output
# ---------------------------------------------------------------------------
ifeq ($(V),1)
Q    :=
echo_cmd =
else
Q    := @
echo_cmd = @printf '  %-7s %s\n' $(1) $(2);
endif

# ---------------------------------------------------------------------------
# Toolchain
# ---------------------------------------------------------------------------
CC      := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy

# -mgeneral-regs-only: FP/SIMD is not enabled at boot, so the compiler must not use it.
# -mstrict-align: with the MMU off all memory is Device type, where unaligned access faults.
# -fno-tree-loop-distribute-patterns: stop GCC turning the loop inside memset into a call
# to memset (infinite recursion).
# -ffile-prefix-map: keep absolute build paths out of debug info (reproducible builds).
CPUFLAGS := -mcpu=cortex-a72 -mgeneral-regs-only -mstrict-align
CFLAGS   := -std=c17 -ffreestanding -fno-builtin -fno-stack-protector -fno-pie \
	    -fno-tree-loop-distribute-patterns \
	    -O2 -g -Wall -Wextra -Werror $(CPUFLAGS) \
	    -ffile-prefix-map=$(CURDIR)=. \
	    -Iinclude -Iboard/$(BOARD) \
	    -MMD -MP $(CFLAGS_EXTRA)
# Recursive (=) so assembly also sees flags added below, such as -DKERNEL_TEST_<name>.
ASFLAGS   = $(CFLAGS) -D__ASSEMBLY__
LDFLAGS  := -Wl,-L,arch/$(ARCH) -nostdlib -static -no-pie -Wl,--build-id=none -Wl,--fatal-warnings -Wl,-z,noexecstack
LDLIBS   := -lgcc

# ---------------------------------------------------------------------------
# Sources and outputs
# ---------------------------------------------------------------------------
BUILD_DIR := build/$(BOARD)

# Self-test kernels (kernel/selftest.c) build into their own directory.
ifneq ($(KERNEL_TEST),)
BUILD_DIR := build/$(BOARD)-test-$(KERNEL_TEST)
CFLAGS    += -DKERNEL_TEST_$(shell echo '$(KERNEL_TEST)' | tr 'a-z-' 'A-Z_')
endif
LDSCRIPT  := board/$(BOARD)/linker.ld
LDSCRIPTS := $(LDSCRIPT) arch/$(ARCH)/kernel.ld

SRC_DIRS  := arch/$(ARCH) board/$(BOARD) kernel lib
# Each board lists the drivers it needs in board/<board>/board.mk (DRIVERS := pl011 ...).
DRIVERS   :=
-include board/$(BOARD)/board.mk
SRC_DIRS  += $(addprefix drivers/,$(DRIVERS))

SRCS := $(sort $(foreach d,$(SRC_DIRS),$(wildcard $(d)/*.c $(d)/*.S)))
OBJS := $(patsubst %,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

ELF  := $(BUILD_DIR)/os001.elf
IMG  := $(BUILD_DIR)/kernel8.img

# Boards QEMU cannot describe on its own ship a Device Tree source for QEMU runs.
DTC       ?= dtc
QEMU_DTS  := $(wildcard board/$(BOARD)/qemu.dts)

# virt boots with a small test disk on a virtio block device: 2048 sectors of 512
# bytes (1 MiB), zero-filled except for a signature line in sector 0.
ifeq ($(BOARD),virt)
DISK         := $(BUILD_DIR)/disk.img
DISK_SECTORS := 2048
endif
QEMU_DTB  := $(if $(QEMU_DTS),$(BUILD_DIR)/qemu.dtb)

# Files checked by format / check-format.
FMT_FILES := $(shell find arch board kernel drivers lib include tests \
		-type f \( -name '*.c' -o -name '*.h' \) 2>/dev/null | sort)

# ---------------------------------------------------------------------------
# QEMU
# ---------------------------------------------------------------------------
# The raw image carries the arm64 image header, so QEMU passes the Device Tree in x0.
ifeq ($(BOARD),virt)
# force-legacy=false: QEMU's virtio-mmio devices use the modern (version 2) interface.
QEMU_ARGS := -M virt -cpu cortex-a72 -m 1G -nic none -nographic -kernel $(IMG) \
	     -global virtio-mmio.force-legacy=false \
	     -drive if=none,format=raw,file=$(DISK),id=disk0 -device virtio-blk-device,drive=disk0
else
QEMU_ARGS := -M raspi4b -serial stdio -display none -kernel $(IMG) -dtb $(QEMU_DTB)
endif

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
.PHONY: all run debug test format check-format clean distclean help
.DEFAULT_GOAL := all

ifeq ($(strip $(SRCS)),)
all:
	@echo "No sources for BOARD=$(BOARD) yet; nothing to build."
else
all: $(IMG) $(QEMU_DTB) $(DISK)
endif

$(BUILD_DIR)/disk.img:
	$(call echo_cmd,DISK,$@)
	$(Q)mkdir -p $(dir $@)
	$(Q)dd if=/dev/zero of=$@ bs=512 count=$(DISK_SECTORS) 2>/dev/null
	$(Q)printf 'OS-001 test disk\n' | dd of=$@ conv=notrunc 2>/dev/null

$(BUILD_DIR)/qemu.dtb: $(QEMU_DTS)
	$(call echo_cmd,DTC,$@)
	$(Q)mkdir -p $(dir $@)
	$(Q)$(DTC) -I dts -O dtb -o $@ $<

$(ELF): $(OBJS) $(LDSCRIPTS)
	$(call echo_cmd,LD,$@)
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $(OBJS) $(LDLIBS)

$(IMG): $(ELF)
	$(call echo_cmd,OBJCOPY,$@)
	$(Q)$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/%.c.o: %.c
	$(call echo_cmd,CC,$<)
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.S.o: %.S
	$(call echo_cmd,AS,$<)
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(ASFLAGS) -c $< -o $@

run: all
	$(QEMU) $(QEMU_ARGS)

# Waits for GDB on tcp::1234 before the first instruction.
debug: all
	@echo "Waiting for GDB: target remote :1234  (symbols: $(ELF))"
	$(QEMU) $(QEMU_ARGS) -s -S

test:
	@tests=$$(find tests -maxdepth 1 -name '*.sh' 2>/dev/null | sort); \
	if [ -z "$$tests" ]; then echo "No tests yet."; exit 0; fi; \
	for t in $$tests; do \
		echo "  TEST    $$t"; \
		sh $$t || exit 1; \
	done

format:
ifneq ($(strip $(FMT_FILES)),)
	$(Q)$(CLANG_FORMAT) -i $(FMT_FILES)
endif

check-format:
ifneq ($(strip $(FMT_FILES)),)
	$(Q)$(CLANG_FORMAT) --dry-run --Werror $(FMT_FILES)
endif
	@echo "Format OK."

clean:
	$(Q)rm -rf $(BUILD_DIR)

distclean:
	$(Q)rm -rf build

help:
	@echo "OS-001 build"
	@echo ""
	@echo "Targets:"
	@echo "  all           Build build/<board>/os001.elf, kernel8.img (and qemu.dtb if the board has one)"
	@echo "  run           Build and boot in QEMU"
	@echo "  debug         Boot in QEMU halted, waiting for GDB on :1234"
	@echo "  test          Run every tests/*.sh"
	@echo "  format        Format C sources and headers with clang-format"
	@echo "  check-format  Fail if any C source or header is not formatted"
	@echo "  clean         Remove build/<board>/"
	@echo "  distclean     Remove build/"
	@echo "  help          Show this help"
	@echo ""
	@echo "Variables:"
	@echo "  BOARD=virt|rpi4          Target board (default: virt)"
	@echo "  CROSS_COMPILE=<prefix>   Toolchain prefix (default: aarch64-elf-)"
	@echo "  V=1                      Show full commands"
	@echo "  CFLAGS_EXTRA=<flags>     Extra compiler flags"
	@echo "  KERNEL_TEST=<name>       Build a self-test kernel (e.g. fault) into build/<board>-test-<name>/"

-include $(DEPS)

AS := as
AS8086 := ./scripts/as8086.sh
LD := ld
OBJCOPY := objcopy
PYTHON := python3

BUILD_DIR := firmware/build
SRC_DIR := firmware/src
LINK_DIR := firmware/linker

BIOS_SRC_DIR := firmware/bios/src
BIOS_LINK_DIR := firmware/bios/linker

BOOT_SRC := $(SRC_DIR)/boot/boot.s
KERNEL_SRC := $(SRC_DIR)/kernel/kernel.s

BOOT_OBJ := $(BUILD_DIR)/boot.o
BOOT_ELF := $(BUILD_DIR)/boot.elf
BOOT_BIN := $(BUILD_DIR)/boot.bin

KERNEL_OBJ := $(BUILD_DIR)/kernel.o
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BUILD_DIR)/kernel.bin

HELLO_SRC := $(SRC_DIR)/dos/hello.s
HELLO_OBJ := $(BUILD_DIR)/hello.o
HELLO_ELF := $(BUILD_DIR)/hello.elf
HELLO_COM := $(BUILD_DIR)/hello.com

DIR_SRC := $(SRC_DIR)/dos/dir.s
DIR_OBJ := $(BUILD_DIR)/dir.o
DIR_ELF := $(BUILD_DIR)/dir.elf
DIR_COM := $(BUILD_DIR)/dir.com

TYPE_SRC := $(SRC_DIR)/dos/type.s
TYPE_OBJ := $(BUILD_DIR)/type.o
TYPE_ELF := $(BUILD_DIR)/type.elf
TYPE_COM := $(BUILD_DIR)/type.com

COMMAND_SRC := $(SRC_DIR)/dos/command.s
COMMAND_OBJ := $(BUILD_DIR)/command.o
COMMAND_ELF := $(BUILD_DIR)/command.elf
COMMAND_COM := $(BUILD_DIR)/command.com

COPY_SRC := $(SRC_DIR)/dos/copy.s
COPY_OBJ := $(BUILD_DIR)/copy.o
COPY_ELF := $(BUILD_DIR)/copy.elf
COPY_COM := $(BUILD_DIR)/copy.com

DEL_SRC := $(SRC_DIR)/dos/del.s
DEL_OBJ := $(BUILD_DIR)/del.o
DEL_ELF := $(BUILD_DIR)/del.elf
DEL_COM := $(BUILD_DIR)/del.com

FIND_SRC := $(SRC_DIR)/dos/find.s
FIND_OBJ := $(BUILD_DIR)/find.o
FIND_ELF := $(BUILD_DIR)/find.elf
FIND_COM := $(BUILD_DIR)/find.com

CHOICE_SRC := $(SRC_DIR)/dos/choice.s
CHOICE_OBJ := $(BUILD_DIR)/choice.o
CHOICE_ELF := $(BUILD_DIR)/choice.elf
CHOICE_COM := $(BUILD_DIR)/choice.com

MORE_SRC := $(SRC_DIR)/dos/more.s
MORE_OBJ := $(BUILD_DIR)/more.o
MORE_ELF := $(BUILD_DIR)/more.elf
MORE_COM := $(BUILD_DIR)/more.com

COMPAT_SRC := $(SRC_DIR)/dos/compat.s
COMPAT_OBJ := $(BUILD_DIR)/compat.o
COMPAT_ELF := $(BUILD_DIR)/compat.elf
COMPAT_COM := $(BUILD_DIR)/compat.com

PING_SRC := $(SRC_DIR)/dos/ping.s
PING_OBJ := $(BUILD_DIR)/ping.o
PING_ELF := $(BUILD_DIR)/ping.elf
PING_COM := $(BUILD_DIR)/ping.com

DHCP_SRC := $(SRC_DIR)/dos/dhcp.s
DHCP_OBJ := $(BUILD_DIR)/dhcp.o
DHCP_ELF := $(BUILD_DIR)/dhcp.elf
DHCP_COM := $(BUILD_DIR)/dhcp.com

STAR_C := $(SRC_DIR)/dos/starfield.c
STAR_ASM := $(BUILD_DIR)/starfield.s
STAR_OBJ := $(BUILD_DIR)/starfield.o
STAR_ELF := $(BUILD_DIR)/starfield.elf
STAR_COM := $(BUILD_DIR)/star.com
DOS_INC := $(SRC_DIR)/dos/inc
WCC := $(PYTHON) -m scripts.wcc

SAMPLE_TXT := fixtures/guest/SAMPLE.TXT
EMPTY_AUTOEXEC := fixtures/guest/AUTOEXEC.BAT

HELLO_EXE := $(BUILD_DIR)/hello.exe

IMAGE := $(BUILD_DIR)/os.img
COMPAT_IMAGE := $(BUILD_DIR)/os-compat.img
COMPAT_AUTOEXEC := fixtures/guest/AUTOEXEC.TEST.BAT
PING_IMAGE := $(BUILD_DIR)/os-ping.img
PING_AUTOEXEC := fixtures/guest/AUTOEXEC.PING.BAT
DHCP_IMAGE := $(BUILD_DIR)/os-dhcp.img
DHCP_AUTOEXEC := fixtures/guest/AUTOEXEC.DHCP.BAT
STAR_IMAGE := $(BUILD_DIR)/os-star.img
STAR_AUTOEXEC := fixtures/guest/AUTOEXEC.STAR.BAT

BIOS_MODULES := post init video keyboard timer disk misc bios_entries bios_font
BIOS_OBJS := $(addprefix $(BUILD_DIR)/,$(addsuffix .o,$(BIOS_MODULES)))
U18_ELF := $(BUILD_DIR)/u18.elf
U18_BIN := $(BUILD_DIR)/u18.bin
U19_BIN := $(BUILD_DIR)/u19.bin

BIOS_TEST_NAMES := bt_equip bt_bda bt_video bt_scroll bt_disk bt_timer bt_int1c bt_kbd_flags bt_modes_text bt_modes_gfx bt_mode4 bt_mode6
BIOS_TEST_DIR := firmware/bios/tests/boot
BIOS_TEST_LINK := firmware/bios/tests/linker/boot_test.ld
BIOS_TEST_BUILD := $(BUILD_DIR)/bios_tests
BIOS_TEST_IMGS := $(addprefix $(BIOS_TEST_BUILD)/,$(addsuffix .img,$(BIOS_TEST_NAMES)))

FD_IMG := emulator/k8086/disks/fd.img

K8086_ROMS_DIR := emulator/k8086/roms

.PHONY: all bios os bios-tests clean run run-fd setup test test-fd-img test-dos-compat test-ping test-dhcp test-star install-roms install-floppy

all: bios os

bios: $(U18_BIN) $(U19_BIN) install-roms

os: $(IMAGE) install-floppy

bios-tests: bios $(BIOS_TEST_IMGS)

install-roms: $(U18_BIN) $(U19_BIN)
	cp -f $(U18_BIN) $(K8086_ROMS_DIR)/u18.bin
	cp -f $(U19_BIN) $(K8086_ROMS_DIR)/u19.bin
	chmod 644 $(K8086_ROMS_DIR)/u18.bin $(K8086_ROMS_DIR)/u19.bin

install-floppy: $(IMAGE)
	cp -f $(IMAGE) $(FD_IMG)
	chmod 644 $(FD_IMG)

$(BUILD_DIR):
	mkdir -p $@

# --- System BIOS (U18 / U19) -------------------------------------------------

$(BIOS_OBJS): $(BUILD_DIR)/%.o: $(BIOS_SRC_DIR)/%.s firmware/bios/inc/equates.inc | $(BUILD_DIR)
	$(AS8086) --32 -o $@ $<

$(U18_ELF): $(BIOS_OBJS) $(BIOS_LINK_DIR)/u18.ld
	$(LD) -m elf_i386 -T $(BIOS_LINK_DIR)/u18.ld -o $@ $(BIOS_OBJS)

$(U18_BIN): $(U18_ELF) scripts/pack_roms.py
	$(OBJCOPY) -O binary $< $@
	$(PYTHON) scripts/pack_roms.py --u18 $@

$(U19_BIN): scripts/pack_roms.py | $(BUILD_DIR)
	$(PYTHON) scripts/pack_roms.py --u19-out $@

# --- OS floppy image ---------------------------------------------------------

KERNEL_INCS := $(wildcard $(SRC_DIR)/kernel/inc/*.inc)

$(KERNEL_OBJ): $(KERNEL_SRC) $(KERNEL_INCS) | $(BUILD_DIR)
	$(AS8086) --32 -o $@ $(KERNEL_SRC)

$(KERNEL_ELF): $(KERNEL_OBJ) $(LINK_DIR)/kernel.ld
	$(LD) -m elf_i386 -T $(LINK_DIR)/kernel.ld -o $@ $<

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

$(HELLO_OBJ): $(HELLO_SRC) | $(BUILD_DIR)
	$(AS8086) --32 -o $@ $(HELLO_SRC)

$(HELLO_ELF): $(HELLO_OBJ) $(LINK_DIR)/com.ld
	$(LD) -m elf_i386 -T $(LINK_DIR)/com.ld -o $@ $<

$(HELLO_COM): $(HELLO_ELF)
	$(OBJCOPY) -O binary $< $@

$(DIR_OBJ): $(DIR_SRC) | $(BUILD_DIR)
	$(AS8086) --32 -o $@ $(DIR_SRC)

$(DIR_ELF): $(DIR_OBJ) $(LINK_DIR)/com.ld
	$(LD) -m elf_i386 -T $(LINK_DIR)/com.ld -o $@ $<

$(DIR_COM): $(DIR_ELF)
	$(OBJCOPY) -O binary $< $@

$(TYPE_OBJ): $(TYPE_SRC) | $(BUILD_DIR)
	$(AS8086) --32 -o $@ $(TYPE_SRC)

$(TYPE_ELF): $(TYPE_OBJ) $(LINK_DIR)/com.ld
	$(LD) -m elf_i386 -T $(LINK_DIR)/com.ld -o $@ $<

$(TYPE_COM): $(TYPE_ELF)
	$(OBJCOPY) -O binary $< $@

$(COMMAND_OBJ): $(COMMAND_SRC) | $(BUILD_DIR)
	$(AS8086) --32 -o $@ $(COMMAND_SRC)

$(COMMAND_ELF): $(COMMAND_OBJ) $(LINK_DIR)/com.ld
	$(LD) -m elf_i386 -T $(LINK_DIR)/com.ld -o $@ $<

$(COMMAND_COM): $(COMMAND_ELF)
	$(OBJCOPY) -O binary $< $@

$(COPY_OBJ): $(COPY_SRC) | $(BUILD_DIR)
	$(AS8086) --32 -o $@ $(COPY_SRC)

$(COPY_ELF): $(COPY_OBJ) $(LINK_DIR)/com.ld
	$(LD) -m elf_i386 -T $(LINK_DIR)/com.ld -o $@ $<

$(COPY_COM): $(COPY_ELF)
	$(OBJCOPY) -O binary $< $@

$(DEL_OBJ): $(DEL_SRC) | $(BUILD_DIR)
	$(AS8086) --32 -o $@ $(DEL_SRC)

$(DEL_ELF): $(DEL_OBJ) $(LINK_DIR)/com.ld
	$(LD) -m elf_i386 -T $(LINK_DIR)/com.ld -o $@ $<

$(DEL_COM): $(DEL_ELF)
	$(OBJCOPY) -O binary $< $@

$(FIND_OBJ): $(FIND_SRC) | $(BUILD_DIR)
	$(AS8086) --32 -o $@ $(FIND_SRC)

$(FIND_ELF): $(FIND_OBJ) $(LINK_DIR)/com.ld
	$(LD) -m elf_i386 -T $(LINK_DIR)/com.ld -o $@ $<

$(FIND_COM): $(FIND_ELF)
	$(OBJCOPY) -O binary $< $@

$(CHOICE_OBJ): $(CHOICE_SRC) | $(BUILD_DIR)
	$(AS8086) --32 -o $@ $(CHOICE_SRC)

$(CHOICE_ELF): $(CHOICE_OBJ) $(LINK_DIR)/com.ld
	$(LD) -m elf_i386 -T $(LINK_DIR)/com.ld -o $@ $<

$(CHOICE_COM): $(CHOICE_ELF)
	$(OBJCOPY) -O binary $< $@

$(MORE_OBJ): $(MORE_SRC) | $(BUILD_DIR)
	$(AS8086) --32 -o $@ $(MORE_SRC)

$(MORE_ELF): $(MORE_OBJ) $(LINK_DIR)/com.ld
	$(LD) -m elf_i386 -T $(LINK_DIR)/com.ld -o $@ $<

$(MORE_COM): $(MORE_ELF)
	$(OBJCOPY) -O binary $< $@

$(COMPAT_OBJ): $(COMPAT_SRC) | $(BUILD_DIR)
	$(AS8086) --32 -o $@ $(COMPAT_SRC)

$(COMPAT_ELF): $(COMPAT_OBJ) $(LINK_DIR)/com.ld
	$(LD) -m elf_i386 -T $(LINK_DIR)/com.ld -o $@ $<

$(COMPAT_COM): $(COMPAT_ELF)
	$(OBJCOPY) -O binary $< $@

$(PING_OBJ): $(PING_SRC) | $(BUILD_DIR)
	$(AS8086) --32 -o $@ $(PING_SRC)

$(PING_ELF): $(PING_OBJ) $(LINK_DIR)/com.ld
	$(LD) -m elf_i386 -T $(LINK_DIR)/com.ld -o $@ $<

$(PING_COM): $(PING_ELF)
	$(OBJCOPY) -O binary $< $@

$(DHCP_OBJ): $(DHCP_SRC) | $(BUILD_DIR)
	$(AS8086) --32 -o $@ $(DHCP_SRC)

$(DHCP_ELF): $(DHCP_OBJ) $(LINK_DIR)/com.ld
	$(LD) -m elf_i386 -T $(LINK_DIR)/com.ld -o $@ $<

$(DHCP_COM): $(DHCP_ELF)
	$(OBJCOPY) -O binary $< $@

$(STAR_ASM): $(STAR_C) $(DOS_INC)/dos.h scripts/wcc.py scripts/wcc_preprocess.py | $(BUILD_DIR)
	$(WCC) $< -o $@ --com -I $(DOS_INC)

$(STAR_OBJ): $(STAR_ASM) | $(BUILD_DIR)
	$(AS8086) --32 -o $@ $(STAR_ASM)

$(STAR_ELF): $(STAR_OBJ) $(LINK_DIR)/com.ld
	$(LD) -m elf_i386 -T $(LINK_DIR)/com.ld -o $@ $<

$(STAR_COM): $(STAR_ELF)
	$(OBJCOPY) -O binary $< $@

$(HELLO_EXE): $(HELLO_COM) scripts/pack_mz.py
	$(PYTHON) scripts/pack_mz.py --com $(HELLO_COM) --out $@

$(BOOT_OBJ): $(BOOT_SRC) | $(BUILD_DIR)
	$(AS8086) --32 -o $@ $(BOOT_SRC)

$(BOOT_ELF): $(BOOT_OBJ) $(LINK_DIR)/boot.ld
	$(LD) -m elf_i386 -T $(LINK_DIR)/boot.ld -o $@ $<

$(BOOT_BIN): $(BOOT_ELF)
	$(OBJCOPY) -O binary $< $@

# Shared FAT12 contents: root = COMMAND + AUTOEXEC; tools in BIN/; demos; SAMPLE in TEST/.
OS_IMAGE_COMMON_DEPS := $(BOOT_BIN) $(KERNEL_BIN) $(HELLO_COM) $(HELLO_EXE) \
	$(DIR_COM) $(TYPE_COM) $(COMMAND_COM) $(COPY_COM) $(DEL_COM) \
	$(FIND_COM) $(CHOICE_COM) $(MORE_COM) \
	$(COMPAT_COM) $(PING_COM) $(DHCP_COM) $(STAR_COM) $(SAMPLE_TXT) $(EMPTY_AUTOEXEC) \
	scripts/mkfs_fat12.py scripts/fat12.py scripts/disk.py

define PACK_OS_IMAGE
	$(PYTHON) -m scripts.mkfs_fat12 --output $(1) --boot $(BOOT_BIN) --kernel $(KERNEL_BIN) \
		--file COMMAND.COM=$(COMMAND_COM) \
		--file BIN/DIR.COM=$(DIR_COM) \
		--file BIN/TYPE.COM=$(TYPE_COM) \
		--file BIN/COPY.COM=$(COPY_COM) \
		--file BIN/DEL.COM=$(DEL_COM) \
		--file BIN/FIND.COM=$(FIND_COM) \
		--file BIN/CHOICE.COM=$(CHOICE_COM) \
		--file BIN/MORE.COM=$(MORE_COM) \
		--file BIN/PING.COM=$(PING_COM) \
		--file BIN/DHCP.COM=$(DHCP_COM) \
		--file DEMO/HELLO.COM=$(HELLO_COM) \
		--file DEMO/HELLO.EXE=$(HELLO_EXE) \
		--file DEMO/COMPAT.COM=$(COMPAT_COM) \
		--file DEMO/STAR.COM=$(STAR_COM) \
		--file TEST/SAMPLE.TXT=$(SAMPLE_TXT) \
		--file AUTOEXEC.BAT=$(2)
endef

$(IMAGE): $(OS_IMAGE_COMMON_DEPS)
	$(call PACK_OS_IMAGE,$@,$(EMPTY_AUTOEXEC))

$(COMPAT_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(COMPAT_AUTOEXEC)
	$(call PACK_OS_IMAGE,$@,$(COMPAT_AUTOEXEC))

$(PING_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(PING_AUTOEXEC)
	$(call PACK_OS_IMAGE,$@,$(PING_AUTOEXEC))

$(DHCP_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(DHCP_AUTOEXEC)
	$(call PACK_OS_IMAGE,$@,$(DHCP_AUTOEXEC))

$(STAR_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(STAR_AUTOEXEC)
	$(call PACK_OS_IMAGE,$@,$(STAR_AUTOEXEC))

# --- BIOS boot-sector unit-test images ---------------------------------------

$(BIOS_TEST_BUILD):
	mkdir -p $@

define BIOS_TEST_RULE
$(BIOS_TEST_BUILD)/$(1).o: $(BIOS_TEST_DIR)/$(1).s $(BIOS_TEST_DIR)/common.inc | $(BIOS_TEST_BUILD)
	$$(AS8086) --32 -o $$@ $$<

$(BIOS_TEST_BUILD)/$(1).elf: $(BIOS_TEST_BUILD)/$(1).o $(BIOS_TEST_LINK)
	$$(LD) -m elf_i386 -T $(BIOS_TEST_LINK) -o $$@ $$<

$(BIOS_TEST_BUILD)/$(1).bin: $(BIOS_TEST_BUILD)/$(1).elf
	$$(OBJCOPY) -O binary $$< $$@
	@sz=$$$$(wc -c < $$@); if [ $$$$sz -ne 512 ]; then echo "$(1).bin size $$$$sz != 512" >&2; exit 1; fi

$(BIOS_TEST_BUILD)/$(1).img: $(BIOS_TEST_BUILD)/$(1).bin scripts/mk_bios_test_img.py
	$$(PYTHON) scripts/mk_bios_test_img.py --output $$@ --boot $$<
endef

$(foreach t,$(BIOS_TEST_NAMES),$(eval $(call BIOS_TEST_RULE,$(t))))

run: all
	./scripts/run-k8086.sh --display cga

run-fd: bios $(FD_IMG)
	./scripts/run-k8086.sh --display cga --turbo --image $(CURDIR)/$(FD_IMG)

setup:
	./setup.sh

test: all bios-tests $(COMPAT_IMAGE) $(PING_IMAGE) $(DHCP_IMAGE) $(STAR_IMAGE)
	$(PYTHON) -m tests.test_bios_roms
	$(PYTHON) -m tests.test_bios_services
	$(PYTHON) -m tests.test_boot_e2e
	$(PYTHON) -m tests.test_dos_compat
	$(PYTHON) -m tests.test_ping_e2e
	$(PYTHON) -m tests.test_dhcp_e2e
	$(PYTHON) -m tests.test_star_e2e
	$(PYTHON) -m tests.starfield_alg_test

test-dos-compat: $(COMPAT_IMAGE)
	$(PYTHON) -m tests.test_dos_compat

test-ping: $(PING_IMAGE)
	$(PYTHON) -m tests.test_ping_e2e

test-dhcp: $(DHCP_IMAGE)
	$(PYTHON) -m tests.test_dhcp_e2e

test-star: $(STAR_IMAGE)
	$(PYTHON) -m tests.test_star_e2e

test-fd-img: bios $(FD_IMG)
	$(PYTHON) -m tests.test_fd_img_e2e

clean:
	rm -f $(BUILD_DIR)/*.bin $(BUILD_DIR)/*.elf $(BUILD_DIR)/*.img $(BUILD_DIR)/*.log $(BUILD_DIR)/*.o $(BUILD_DIR)/as8086.*.s $(BUILD_DIR)/starfield.s $(BUILD_DIR)/*.com
	rm -rf $(BIOS_TEST_BUILD)

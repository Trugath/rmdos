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

# Asm-only COM tools (hardware / smoke)
SYS_COM := $(BUILD_DIR)/sys.com
PARTEDIT_COM := $(BUILD_DIR)/partedit.com
FORMAT_COM := $(BUILD_DIR)/format.com
COMPAT_COM := $(BUILD_DIR)/compat.com
INT21X_COM := $(BUILD_DIR)/int21x.com
PING_COM := $(BUILD_DIR)/ping.com
DHCP_COM := $(BUILD_DIR)/dhcp.com
TELNET_COM := $(BUILD_DIR)/telnet.com
NET_COM := $(BUILD_DIR)/net.com
NETTEST_COM := $(BUILD_DIR)/nettest.com
GZIP_COM := $(BUILD_DIR)/gzip.com
GUNZIP_COM := $(BUILD_DIR)/gunzip.com
ANSI_SYS := $(BUILD_DIR)/ansi.sys
ANSITST_COM := $(BUILD_DIR)/ansitst.com

# wcc C COM tools (same basename: foo.c -> foo.com), plus starfield.c -> star.com
DOS_C_TOOLS := command dir type copy del attrib label move xcopy chkdsk find choice more mem fc tree sort edit debug diskcopy diskcomp mode subst
COMMAND_COM := $(BUILD_DIR)/command.com
DIR_COM := $(BUILD_DIR)/dir.com
TYPE_COM := $(BUILD_DIR)/type.com
COPY_COM := $(BUILD_DIR)/copy.com
DEL_COM := $(BUILD_DIR)/del.com
ATTRIB_COM := $(BUILD_DIR)/attrib.com
LABEL_COM := $(BUILD_DIR)/label.com
MOVE_COM := $(BUILD_DIR)/move.com
XCOPY_COM := $(BUILD_DIR)/xcopy.com
CHKDSK_COM := $(BUILD_DIR)/chkdsk.com
FIND_COM := $(BUILD_DIR)/find.com
CHOICE_COM := $(BUILD_DIR)/choice.com
MORE_COM := $(BUILD_DIR)/more.com
MEM_COM := $(BUILD_DIR)/mem.com
FC_COM := $(BUILD_DIR)/fc.com
TREE_COM := $(BUILD_DIR)/tree.com
SORT_COM := $(BUILD_DIR)/sort.com
EDIT_COM := $(BUILD_DIR)/edit.com
DEBUG_COM := $(BUILD_DIR)/debug.com
DISKCOPY_COM := $(BUILD_DIR)/diskcopy.com
DISKCOMP_COM := $(BUILD_DIR)/diskcomp.com
MODE_COM := $(BUILD_DIR)/mode.com
SUBST_COM := $(BUILD_DIR)/subst.com

STAR_C := $(SRC_DIR)/dos/starfield.c
STAR_ASM := $(BUILD_DIR)/starfield.s
STAR_OBJ := $(BUILD_DIR)/starfield.o
STAR_ELF := $(BUILD_DIR)/starfield.elf
STAR_COM := $(BUILD_DIR)/star.com
DOS_INC := $(SRC_DIR)/dos/inc
WCC := $(PYTHON) -m scripts.wcc
WCC_DEPS := $(DOS_INC)/dos.h scripts/wcc.py scripts/wcc_preprocess.py


SAMPLE_TXT := fixtures/guest/SAMPLE.TXT
DBG_SCR := fixtures/guest/DBG.SCR
BIG_TXT := fixtures/guest/BIG.TXT
SHIFT_BAT := fixtures/guest/SHIFT.BAT
CALLTST_BAT := fixtures/guest/CALLTST.BAT
CALLTST2_BAT := fixtures/guest/CALLTST2.BAT
EMPTY_AUTOEXEC := fixtures/guest/AUTOEXEC.BAT

HELLO_EXE := $(BUILD_DIR)/hello.exe
BIGEXE_SRC := $(SRC_DIR)/dos/bigexe.s
BIGEXE_OBJ := $(BUILD_DIR)/bigexe.o
BIGEXE_ELF := $(BUILD_DIR)/bigexe.elf
BIGEXE_COM := $(BUILD_DIR)/bigexe.com
BIGEXE_EXE := $(BUILD_DIR)/bigexe.exe
BIGEXE_IMAGE := $(BUILD_DIR)/os-bigexe.img
BIGEXE_AUTOEXEC := fixtures/guest/AUTOEXEC.BIGEXE.BAT
ELITE_DIR := fixtures/guest/elite
ELITE_EXE := $(ELITE_DIR)/ELITE.EXE
ELITE_AUTOEXEC := fixtures/guest/AUTOEXEC.ELITE.BAT
ELITE_IMAGE := $(BUILD_DIR)/os-elite.img

IMAGE := $(BUILD_DIR)/os.img
COMPAT_IMAGE := $(BUILD_DIR)/os-compat.img
COMPAT_AUTOEXEC := fixtures/guest/AUTOEXEC.TEST.BAT
PING_IMAGE := $(BUILD_DIR)/os-ping.img
PING_AUTOEXEC := fixtures/guest/AUTOEXEC.PING.BAT
DHCP_IMAGE := $(BUILD_DIR)/os-dhcp.img
DHCP_AUTOEXEC := fixtures/guest/AUTOEXEC.DHCP.BAT
TELNET_IMAGE := $(BUILD_DIR)/os-telnet.img
TELNET_AUTOEXEC := fixtures/guest/AUTOEXEC.TELNET.BAT
STAR_IMAGE := $(BUILD_DIR)/os-star.img
STAR_AUTOEXEC := fixtures/guest/AUTOEXEC.STAR.BAT
DIR_IMAGE := $(BUILD_DIR)/os-dir.img
DIR_AUTOEXEC := fixtures/guest/AUTOEXEC.DIR.BAT
FORMAT_IMAGE := $(BUILD_DIR)/os-format.img
FORMAT_AUTOEXEC := fixtures/guest/AUTOEXEC.FORMAT.BAT
FORMAT_HD_IMAGE := $(BUILD_DIR)/os-format-hd.img
FORMAT_HD_AUTOEXEC := fixtures/guest/AUTOEXEC.FORMAT.HD.BAT
FAT16_HD_IMAGE := $(BUILD_DIR)/os-fat16-hd.img
FAT16_HD_AUTOEXEC := fixtures/guest/AUTOEXEC.FAT16.HD.BAT
PARTEDIT_HD_IMAGE := $(BUILD_DIR)/os-partedit-hd.img
PARTEDIT_HD_AUTOEXEC := fixtures/guest/AUTOEXEC.PARTEDIT.BAT
MULTILET_HD_IMAGE := $(BUILD_DIR)/os-multilet-hd.img
MULTILET_HD_AUTOEXEC := fixtures/guest/AUTOEXEC.MULTILET.BAT
EXTPART_HD_IMAGE := $(BUILD_DIR)/os-extpart-hd.img
EXTPART_HD_AUTOEXEC := fixtures/guest/AUTOEXEC.EXTPART.BAT
SUBST_IMAGE := $(BUILD_DIR)/os-subst.img
SUBST_AUTOEXEC := fixtures/guest/AUTOEXEC.SUBST.BAT
BATCH_IMAGE := $(BUILD_DIR)/os-batch.img
BATCH_AUTOEXEC := fixtures/guest/AUTOEXEC.BATCH.BAT
DISK_IMAGE := $(BUILD_DIR)/os-disk.img
DISK_AUTOEXEC := fixtures/guest/AUTOEXEC.DISK.BAT
INSTALL_BAT := fixtures/guest/INSTALL.BAT
INSTALL_IMAGE := $(BUILD_DIR)/os-install.img
INSTALL_AUTOEXEC := fixtures/guest/AUTOEXEC.INSTALL.BAT
NET_IMAGE := $(BUILD_DIR)/os-net.img
NET_AUTOEXEC := fixtures/guest/AUTOEXEC.NET.BAT
NET_CONFIG := fixtures/guest/CONFIG.NET.SYS
GZIP_IMAGE := $(BUILD_DIR)/os-gzip.img
GZIP_AUTOEXEC := fixtures/guest/AUTOEXEC.GZIP.BAT
UTILS_IMAGE := $(BUILD_DIR)/os-utils.img
UTILS_AUTOEXEC := fixtures/guest/AUTOEXEC.UTILS.BAT
DISKCOPY_IMAGE := $(BUILD_DIR)/os-diskcopy.img
DISKCOPY_AUTOEXEC := fixtures/guest/AUTOEXEC.DISKCOPY.BAT
DISKCOMP_IMAGE := $(BUILD_DIR)/os-diskcomp.img
DISKCOMP_AUTOEXEC := fixtures/guest/AUTOEXEC.DISKCOMP.BAT
ANSI_IMAGE := $(BUILD_DIR)/os-ansi.img
ANSI_AUTOEXEC := fixtures/guest/AUTOEXEC.ANSI.BAT
ANSI_CONFIG := fixtures/guest/CONFIG.ANSI.SYS
STUBCFG_IMAGE := $(BUILD_DIR)/os-stubcfg.img
STUBCFG_AUTOEXEC := fixtures/guest/AUTOEXEC.STUB.BAT
STUBCFG_CONFIG := fixtures/guest/CONFIG.STUB.SYS

BIOS_MODULES := post init video keyboard timer disk fdc misc bios_entries bios_font
BIOS_OBJS := $(addprefix $(BUILD_DIR)/,$(addsuffix .o,$(BIOS_MODULES)))
U18_ELF := $(BUILD_DIR)/u18.elf
U18_BIN := $(BUILD_DIR)/u18.bin
U19_BIN := $(BUILD_DIR)/u19.bin

BIOS_TEST_NAMES := bt_equip bt_bda bt_video bt_scroll bt_disk bt_disk144 bt_disk120 bt_disk360 bt_disk_stat bt_disk_upgrade bt_timer bt_int1c bt_kbd_flags bt_kbd_ext bt_modes_text bt_modes_gfx bt_mode4 bt_mode6 bt_serial bt_int15 bt_pixel bt_misc bt_ctype bt_gfx_scroll bt_pixel6 bt_prtsc bt_ident bt_entry bt_cad bt_fdc_rw bt_fdc_fmt bt_fdc_type bt_page bt_palette bt_bel bt_int1a_set bt_hd_params bt_hd_rw bt_kbd_irq bt_kbd_prtsc bt_brk bt_int18 bt_chgline bt_str bt_cfg bt_readchar bt_writech bt_kbd_read bt_kbd_shift bt_int13_err bt_hd_verify bt_motor bt_timer_of
BIOS_TEST_DIR := firmware/bios/tests/boot
BIOS_TEST_LINK := firmware/bios/tests/linker/boot_test.ld
BIOS_TEST_BUILD := $(BUILD_DIR)/bios_tests
BIOS_TEST_IMGS := $(addprefix $(BIOS_TEST_BUILD)/,$(addsuffix .img,$(BIOS_TEST_NAMES)))
# Image size (sectors): default 1440=720K; HD floppies need full media size for BDA hints.
BIOS_TEST_SECTORS_bt_disk144 := 2880
BIOS_TEST_SECTORS_bt_disk120 := 2400
BIOS_TEST_SECTORS_bt_disk360 := 720
BIOS_TEST_SECTORS_bt_disk_upgrade := 720

FD_IMG := emulator/k8086/disks/fd.img

K8086_ROMS_DIR := emulator/k8086/roms

.PHONY: all bios os os-disk.img bios-tests clean run run-fd run-elite setup test test-fd-img test-dos-compat test-ping test-dhcp test-telnet test-net test-star test-bigexe test-elite test-dir test-format test-format-options test-sys test-format-hd test-fat16-hd test-partedit-hd test-multilet-hd test-extpart-hd test-subst test-batch test-disk test-gzip test-utils test-diskcopy test-diskcomp test-ansi test-stubcfg test-install-hd install-roms install-floppy

all: bios os

bios: $(U18_BIN) $(U19_BIN) $(BUILD_DIR)/fdrom.bin install-roms

os: $(IMAGE) install-floppy

os-disk.img: $(DISK_IMAGE)

bios-tests: bios $(BIOS_TEST_IMGS)

install-roms: $(U18_BIN) $(U19_BIN) $(BUILD_DIR)/fdrom.bin
	cp -f $(U18_BIN) $(K8086_ROMS_DIR)/u18.bin
	cp -f $(U19_BIN) $(K8086_ROMS_DIR)/u19.bin
	cp -f $(BUILD_DIR)/fdrom.bin $(K8086_ROMS_DIR)/fdrom.bin
	chmod 644 $(K8086_ROMS_DIR)/u18.bin $(K8086_ROMS_DIR)/u19.bin $(K8086_ROMS_DIR)/fdrom.bin

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

# Fixed Disk option ROM (C800:)
FDROM_SRC := firmware/bios/fdrom/fdrom.s
FDROM_LD := firmware/bios/fdrom/fdrom.ld
$(BUILD_DIR)/fdrom.o: $(FDROM_SRC) firmware/bios/fdrom/inc/equates.inc | $(BUILD_DIR)
	$(AS8086) --32 -o $@ $(FDROM_SRC)
$(BUILD_DIR)/fdrom.elf: $(BUILD_DIR)/fdrom.o $(FDROM_LD)
	$(LD) -m elf_i386 -T $(FDROM_LD) -o $@ $(BUILD_DIR)/fdrom.o
$(BUILD_DIR)/fdrom.bin: $(BUILD_DIR)/fdrom.elf scripts/pack_fdrom.py
	$(OBJCOPY) -O binary $< $(BUILD_DIR)/fdrom.raw.bin
	$(PYTHON) scripts/pack_fdrom.py --input $(BUILD_DIR)/fdrom.raw.bin --output $@

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

# Asm COM pattern (hardware / smoke tools)
define DOS_ASM_COM_RULE
$(BUILD_DIR)/$(1).o: $(SRC_DIR)/dos/$(1).s $(wildcard $(SRC_DIR)/dos/inc/*.inc) | $(BUILD_DIR)
	$$(AS8086) --32 -o $$@ $$<
$(BUILD_DIR)/$(1).elf: $(BUILD_DIR)/$(1).o $(LINK_DIR)/com.ld
	$$(LD) -m elf_i386 -T $(LINK_DIR)/com.ld -o $$@ $$<
$(BUILD_DIR)/$(1).com: $(BUILD_DIR)/$(1).elf
	$$(OBJCOPY) -O binary $$< $$@
endef
$(foreach t,sys partedit format compat int21x ping dhcp telnet net nettest gzip gunzip ansitst,$(eval $(call DOS_ASM_COM_RULE,$(t))))

# ANSI.SYS (device driver — linked at offset 0)
$(BUILD_DIR)/ansi.o: $(SRC_DIR)/dos/ansi.sys.s | $(BUILD_DIR)
	$(AS8086) --32 -o $@ $<
$(BUILD_DIR)/ansi.elf: $(BUILD_DIR)/ansi.o $(LINK_DIR)/sys.ld
	$(LD) -m elf_i386 -T $(LINK_DIR)/sys.ld -o $@ $<
$(ANSI_SYS): $(BUILD_DIR)/ansi.elf
	$(OBJCOPY) -O binary $< $@

# C COM pattern: foo.c -> build/foo.s -> .o -> .elf -> .com
define DOS_C_COM_RULE
$(BUILD_DIR)/$(1).s: $(SRC_DIR)/dos/$(1).c $$(WCC_DEPS) | $(BUILD_DIR)
	$$(WCC) $$< -o $$@ --com -I $$(DOS_INC)
$(BUILD_DIR)/$(1).o: $(BUILD_DIR)/$(1).s | $(BUILD_DIR)
	$$(AS8086) --32 -o $$@ $$<
$(BUILD_DIR)/$(1).elf: $(BUILD_DIR)/$(1).o $(LINK_DIR)/com.ld
	$$(LD) -m elf_i386 -T $(LINK_DIR)/com.ld -o $$@ $$<
$(BUILD_DIR)/$(1).com: $(BUILD_DIR)/$(1).elf
	$$(OBJCOPY) -O binary $$< $$@
endef
$(foreach t,$(DOS_C_TOOLS),$(eval $(call DOS_C_COM_RULE,$(t))))

$(STAR_ASM): $(STAR_C) $(WCC_DEPS) | $(BUILD_DIR)
	$(WCC) $< -o $@ --com -I $(DOS_INC)

$(STAR_OBJ): $(STAR_ASM) | $(BUILD_DIR)
	$(AS8086) --32 -o $@ $(STAR_ASM)

$(STAR_ELF): $(STAR_OBJ) $(LINK_DIR)/com.ld
	$(LD) -m elf_i386 -T $(LINK_DIR)/com.ld -o $@ $<

$(STAR_COM): $(STAR_ELF)
	$(OBJCOPY) -O binary $< $@

$(HELLO_EXE): $(HELLO_COM) scripts/pack_mz.py
	$(PYTHON) scripts/pack_mz.py --com $(HELLO_COM) --out $@

$(BIGEXE_OBJ): $(BIGEXE_SRC) | $(BUILD_DIR)
	$(AS8086) --32 -o $@ $(BIGEXE_SRC)

$(BIGEXE_ELF): $(BIGEXE_OBJ) $(LINK_DIR)/com.ld
	$(LD) -m elf_i386 -T $(LINK_DIR)/com.ld -o $@ $<

$(BIGEXE_COM): $(BIGEXE_ELF)
	$(OBJCOPY) -O binary $< $@

$(BIGEXE_EXE): $(BIGEXE_COM) scripts/pack_mz.py
	$(PYTHON) scripts/pack_mz.py --com $(BIGEXE_COM) --out $@ --min-size 75000

$(BIGEXE_IMAGE): $(BOOT_BIN) $(KERNEL_BIN) $(COMMAND_COM) $(BIGEXE_EXE) $(BIGEXE_AUTOEXEC) scripts/mkfs_fat12.py
	$(PYTHON) -m scripts.mkfs_fat12 --output $@ --boot $(BOOT_BIN) --kernel $(KERNEL_BIN) \
		--file COMMAND.COM=$(COMMAND_COM) \
		--file DEMO/BIGEXE.EXE=$(BIGEXE_EXE) \
		--file AUTOEXEC.BAT=$(BIGEXE_AUTOEXEC)

# Lean Elite image — only built when fixtures/guest/elite/ELITE.EXE is present.
$(ELITE_IMAGE): $(BOOT_BIN) $(KERNEL_BIN) $(COMMAND_COM) $(ELITE_AUTOEXEC) $(ELITE_EXE) scripts/mkfs_fat12.py
	$(PYTHON) -m scripts.mkfs_fat12 --output $@ --boot $(BOOT_BIN) --kernel $(KERNEL_BIN) \
		--file COMMAND.COM=$(COMMAND_COM) \
		--file ELITE.EXE=$(ELITE_EXE) \
		--file AUTOEXEC.BAT=$(ELITE_AUTOEXEC)

$(BOOT_OBJ): $(BOOT_SRC) | $(BUILD_DIR)
	$(AS8086) --32 -o $@ $(BOOT_SRC)

$(BOOT_ELF): $(BOOT_OBJ) $(LINK_DIR)/boot.ld
	$(LD) -m elf_i386 -T $(LINK_DIR)/boot.ld -o $@ $<

$(BOOT_BIN): $(BOOT_ELF)
	$(OBJCOPY) -O binary $< $@

# Shared FAT12 contents: root = COMMAND + AUTOEXEC; tools in BIN/; demos; SAMPLE in TEST/.
OS_IMAGE_COMMON_DEPS := $(BOOT_BIN) $(KERNEL_BIN) $(HELLO_COM) $(HELLO_EXE) \
	$(DIR_COM) $(TYPE_COM) $(COMMAND_COM) $(COPY_COM) $(DEL_COM) $(ATTRIB_COM) $(LABEL_COM) \
	$(MOVE_COM) $(XCOPY_COM) $(CHKDSK_COM) $(SYS_COM) $(PARTEDIT_COM) $(FORMAT_COM) \
	$(FIND_COM) $(CHOICE_COM) $(MORE_COM) $(MEM_COM) $(FC_COM) $(TREE_COM) $(SORT_COM) \
	$(EDIT_COM) $(DEBUG_COM) $(DISKCOPY_COM) $(DISKCOMP_COM) $(MODE_COM) $(SUBST_COM) \
	$(COMPAT_COM) $(INT21X_COM) $(PING_COM) $(DHCP_COM) $(TELNET_COM) $(NET_COM) $(GZIP_COM) $(GUNZIP_COM) \
	$(ANSI_SYS) $(ANSITST_COM) \
	$(STAR_COM) $(SAMPLE_TXT) $(DBG_SCR) $(BIG_TXT) $(SHIFT_BAT) $(INSTALL_BAT) \
	$(EMPTY_AUTOEXEC) scripts/mkfs_fat12.py scripts/fat12.py scripts/disk.py

define PACK_OS_IMAGE
	$(PYTHON) -m scripts.mkfs_fat12 --output $(1) --boot $(BOOT_BIN) --kernel $(KERNEL_BIN) \
		--file COMMAND.COM=$(COMMAND_COM) \
		--file INSTALL.BAT=$(INSTALL_BAT) \
		--file BIN/DIR.COM=$(DIR_COM) \
		--file BIN/TYPE.COM=$(TYPE_COM) \
		--file BIN/COPY.COM=$(COPY_COM) \
		--file BIN/DEL.COM=$(DEL_COM) \
		--file BIN/ATTRIB.COM=$(ATTRIB_COM) \
		--file BIN/LABEL.COM=$(LABEL_COM) \
		--file BIN/MOVE.COM=$(MOVE_COM) \
		--file BIN/XCOPY.COM=$(XCOPY_COM) \
		--file BIN/CHKDSK.COM=$(CHKDSK_COM) \
		--file BIN/SYS.COM=$(SYS_COM) \
		--file BIN/PARTEDIT.COM=$(PARTEDIT_COM) \
		--file BIN/FORMAT.COM=$(FORMAT_COM) \
		--file BIN/FIND.COM=$(FIND_COM) \
		--file BIN/CHOICE.COM=$(CHOICE_COM) \
		--file BIN/MORE.COM=$(MORE_COM) \
		--file BIN/MEM.COM=$(MEM_COM) \
		--file BIN/FC.COM=$(FC_COM) \
		--file BIN/TREE.COM=$(TREE_COM) \
		--file BIN/SORT.COM=$(SORT_COM) \
		--file BIN/EDIT.COM=$(EDIT_COM) \
		--file BIN/DEBUG.COM=$(DEBUG_COM) \
		--file BIN/DISKCOPY.COM=$(DISKCOPY_COM) \
		--file BIN/DISKCOMP.COM=$(DISKCOMP_COM) \
		--file BIN/MODE.COM=$(MODE_COM) \
		--file BIN/SUBST.COM=$(SUBST_COM) \
		--file BIN/PING.COM=$(PING_COM) \
		--file BIN/DHCP.COM=$(DHCP_COM) \
		--file BIN/TELNET.COM=$(TELNET_COM) \
		--file BIN/NET.COM=$(NET_COM) \
		--file BIN/GZIP.COM=$(GZIP_COM) \
		--file BIN/GUNZIP.COM=$(GUNZIP_COM) \
		--file BIN/ANSI.SYS=$(ANSI_SYS) \
		--file DEMO/HELLO.COM=$(HELLO_COM) \
		--file DEMO/HELLO.EXE=$(HELLO_EXE) \
		--file DEMO/COMPAT.COM=$(COMPAT_COM) \
		--file DEMO/INT21X.COM=$(INT21X_COM) \
		--file DEMO/ANSITST.COM=$(ANSITST_COM) \
		--file DEMO/STAR.COM=$(STAR_COM) \
		--file TEST/SAMPLE.TXT=$(SAMPLE_TXT) \
		--file TEST/DBG.SCR=$(DBG_SCR) \
		--file TEST/BIG.TXT=$(BIG_TXT) \
		--file SHIFT.BAT=$(SHIFT_BAT) \
		--file AUTOEXEC.BAT=$(2)
endef

define PACK_OS_IMAGE_CFG
	$(PYTHON) -m scripts.mkfs_fat12 --output $(1) --boot $(BOOT_BIN) --kernel $(KERNEL_BIN) \
		--file COMMAND.COM=$(COMMAND_COM) \
		--file INSTALL.BAT=$(INSTALL_BAT) \
		--file CONFIG.SYS=$(3) \
		--file BIN/DIR.COM=$(DIR_COM) \
		--file BIN/TYPE.COM=$(TYPE_COM) \
		--file BIN/COPY.COM=$(COPY_COM) \
		--file BIN/DEL.COM=$(DEL_COM) \
		--file BIN/ATTRIB.COM=$(ATTRIB_COM) \
		--file BIN/LABEL.COM=$(LABEL_COM) \
		--file BIN/MOVE.COM=$(MOVE_COM) \
		--file BIN/XCOPY.COM=$(XCOPY_COM) \
		--file BIN/CHKDSK.COM=$(CHKDSK_COM) \
		--file BIN/SYS.COM=$(SYS_COM) \
		--file BIN/PARTEDIT.COM=$(PARTEDIT_COM) \
		--file BIN/FORMAT.COM=$(FORMAT_COM) \
		--file BIN/FIND.COM=$(FIND_COM) \
		--file BIN/CHOICE.COM=$(CHOICE_COM) \
		--file BIN/MORE.COM=$(MORE_COM) \
		--file BIN/MEM.COM=$(MEM_COM) \
		--file BIN/FC.COM=$(FC_COM) \
		--file BIN/TREE.COM=$(TREE_COM) \
		--file BIN/SORT.COM=$(SORT_COM) \
		--file BIN/EDIT.COM=$(EDIT_COM) \
		--file BIN/DEBUG.COM=$(DEBUG_COM) \
		--file BIN/DISKCOPY.COM=$(DISKCOPY_COM) \
		--file BIN/DISKCOMP.COM=$(DISKCOMP_COM) \
		--file BIN/MODE.COM=$(MODE_COM) \
		--file BIN/SUBST.COM=$(SUBST_COM) \
		--file BIN/PING.COM=$(PING_COM) \
		--file BIN/DHCP.COM=$(DHCP_COM) \
		--file BIN/TELNET.COM=$(TELNET_COM) \
		--file BIN/NET.COM=$(NET_COM) \
		--file BIN/GZIP.COM=$(GZIP_COM) \
		--file BIN/GUNZIP.COM=$(GUNZIP_COM) \
		--file BIN/ANSI.SYS=$(ANSI_SYS) \
		--file BIN/NETTEST.COM=$(NETTEST_COM) \
		--file DEMO/HELLO.COM=$(HELLO_COM) \
		--file DEMO/HELLO.EXE=$(HELLO_EXE) \
		--file DEMO/COMPAT.COM=$(COMPAT_COM) \
		--file DEMO/INT21X.COM=$(INT21X_COM) \
		--file DEMO/ANSITST.COM=$(ANSITST_COM) \
		--file DEMO/STAR.COM=$(STAR_COM) \
		--file TEST/SAMPLE.TXT=$(SAMPLE_TXT) \
		--file TEST/DBG.SCR=$(DBG_SCR) \
		--file TEST/BIG.TXT=$(BIG_TXT) \
		--file SHIFT.BAT=$(SHIFT_BAT) \
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

$(TELNET_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(TELNET_AUTOEXEC)
	$(call PACK_OS_IMAGE,$@,$(TELNET_AUTOEXEC))

$(STAR_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(STAR_AUTOEXEC)
	$(call PACK_OS_IMAGE,$@,$(STAR_AUTOEXEC))

$(DIR_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(DIR_AUTOEXEC)
	$(call PACK_OS_IMAGE,$@,$(DIR_AUTOEXEC))

$(FORMAT_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(FORMAT_AUTOEXEC)
	$(call PACK_OS_IMAGE,$@,$(FORMAT_AUTOEXEC))

$(FORMAT_HD_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(FORMAT_HD_AUTOEXEC)
	$(call PACK_OS_IMAGE,$@,$(FORMAT_HD_AUTOEXEC))

$(FAT16_HD_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(FAT16_HD_AUTOEXEC)
	$(call PACK_OS_IMAGE,$@,$(FAT16_HD_AUTOEXEC))

$(PARTEDIT_HD_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(PARTEDIT_HD_AUTOEXEC)
	$(call PACK_OS_IMAGE,$@,$(PARTEDIT_HD_AUTOEXEC))

$(MULTILET_HD_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(MULTILET_HD_AUTOEXEC)
	$(call PACK_OS_IMAGE,$@,$(MULTILET_HD_AUTOEXEC))

$(EXTPART_HD_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(EXTPART_HD_AUTOEXEC)
	$(call PACK_OS_IMAGE,$@,$(EXTPART_HD_AUTOEXEC))

$(SUBST_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(SUBST_AUTOEXEC)
	$(call PACK_OS_IMAGE,$@,$(SUBST_AUTOEXEC))

$(BATCH_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(BATCH_AUTOEXEC) $(CALLTST_BAT) $(CALLTST2_BAT)
	$(PYTHON) -m scripts.mkfs_fat12 --output $@ --boot $(BOOT_BIN) --kernel $(KERNEL_BIN) \
		--file COMMAND.COM=$(COMMAND_COM) \
		--file INSTALL.BAT=$(INSTALL_BAT) \
		--file BIN/DIR.COM=$(DIR_COM) \
		--file BIN/TYPE.COM=$(TYPE_COM) \
		--file BIN/COPY.COM=$(COPY_COM) \
		--file BIN/DEL.COM=$(DEL_COM) \
		--file BIN/ATTRIB.COM=$(ATTRIB_COM) \
		--file BIN/LABEL.COM=$(LABEL_COM) \
		--file BIN/MOVE.COM=$(MOVE_COM) \
		--file BIN/XCOPY.COM=$(XCOPY_COM) \
		--file BIN/CHKDSK.COM=$(CHKDSK_COM) \
		--file BIN/SYS.COM=$(SYS_COM) \
		--file BIN/PARTEDIT.COM=$(PARTEDIT_COM) \
		--file BIN/FORMAT.COM=$(FORMAT_COM) \
		--file BIN/FIND.COM=$(FIND_COM) \
		--file BIN/CHOICE.COM=$(CHOICE_COM) \
		--file BIN/MORE.COM=$(MORE_COM) \
		--file BIN/MEM.COM=$(MEM_COM) \
		--file BIN/FC.COM=$(FC_COM) \
		--file BIN/TREE.COM=$(TREE_COM) \
		--file BIN/SORT.COM=$(SORT_COM) \
		--file BIN/EDIT.COM=$(EDIT_COM) \
		--file BIN/DEBUG.COM=$(DEBUG_COM) \
		--file BIN/DISKCOPY.COM=$(DISKCOPY_COM) \
		--file BIN/DISKCOMP.COM=$(DISKCOMP_COM) \
		--file BIN/MODE.COM=$(MODE_COM) \
		--file BIN/SUBST.COM=$(SUBST_COM) \
		--file BIN/PING.COM=$(PING_COM) \
		--file BIN/DHCP.COM=$(DHCP_COM) \
		--file BIN/TELNET.COM=$(TELNET_COM) \
		--file BIN/NET.COM=$(NET_COM) \
		--file BIN/GZIP.COM=$(GZIP_COM) \
		--file BIN/GUNZIP.COM=$(GUNZIP_COM) \
		--file BIN/ANSI.SYS=$(ANSI_SYS) \
		--file DEMO/HELLO.COM=$(HELLO_COM) \
		--file DEMO/HELLO.EXE=$(HELLO_EXE) \
		--file DEMO/COMPAT.COM=$(COMPAT_COM) \
		--file DEMO/INT21X.COM=$(INT21X_COM) \
		--file DEMO/ANSITST.COM=$(ANSITST_COM) \
		--file DEMO/STAR.COM=$(STAR_COM) \
		--file TEST/SAMPLE.TXT=$(SAMPLE_TXT) \
		--file TEST/DBG.SCR=$(DBG_SCR) \
		--file TEST/BIG.TXT=$(BIG_TXT) \
		--file SHIFT.BAT=$(SHIFT_BAT) \
		--file CALLTST.BAT=$(CALLTST_BAT) \
		--file CALLTST2.BAT=$(CALLTST2_BAT) \
		--file AUTOEXEC.BAT=$(BATCH_AUTOEXEC)

$(DISK_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(DISK_AUTOEXEC)
	$(call PACK_OS_IMAGE,$@,$(DISK_AUTOEXEC))

$(INSTALL_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(INSTALL_AUTOEXEC)
	$(call PACK_OS_IMAGE,$@,$(INSTALL_AUTOEXEC))

$(NET_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(NETTEST_COM) $(NET_AUTOEXEC) $(NET_CONFIG)
	$(call PACK_OS_IMAGE_CFG,$@,$(NET_AUTOEXEC),$(NET_CONFIG))

$(GZIP_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(GZIP_AUTOEXEC)
	$(call PACK_OS_IMAGE,$@,$(GZIP_AUTOEXEC))

$(UTILS_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(UTILS_AUTOEXEC)
	$(call PACK_OS_IMAGE,$@,$(UTILS_AUTOEXEC))

$(DISKCOPY_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(DISKCOPY_AUTOEXEC)
	$(call PACK_OS_IMAGE,$@,$(DISKCOPY_AUTOEXEC))

$(DISKCOMP_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(DISKCOMP_AUTOEXEC)
	$(call PACK_OS_IMAGE,$@,$(DISKCOMP_AUTOEXEC))

$(ANSI_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(ANSI_AUTOEXEC) $(ANSI_CONFIG)
	$(call PACK_OS_IMAGE_CFG,$@,$(ANSI_AUTOEXEC),$(ANSI_CONFIG))

$(STUBCFG_IMAGE): $(OS_IMAGE_COMMON_DEPS) $(NETTEST_COM) $(STUBCFG_AUTOEXEC) $(STUBCFG_CONFIG)
	$(call PACK_OS_IMAGE_CFG,$@,$(STUBCFG_AUTOEXEC),$(STUBCFG_CONFIG))

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
	$$(PYTHON) scripts/mk_bios_test_img.py --output $$@ --boot $$< --sectors $$(or $$(BIOS_TEST_SECTORS_$(1)),1440)
endef

$(foreach t,$(BIOS_TEST_NAMES),$(eval $(call BIOS_TEST_RULE,$(t))))

run: all
	./scripts/run-k8086.sh --display cga

run-fd: bios $(FD_IMG)
	./scripts/run-k8086.sh --display cga --turbo --image $(CURDIR)/$(FD_IMG)

# Nonturbo CGA boot of lean Elite floppy (requires fixtures/guest/elite/ELITE.EXE).
run-elite: bios $(ELITE_IMAGE)
	./scripts/run-k8086.sh --display cga --image $(CURDIR)/$(ELITE_IMAGE)

setup:
	./setup.sh

test: all bios-tests $(COMPAT_IMAGE) $(PING_IMAGE) $(DHCP_IMAGE) $(TELNET_IMAGE) $(NET_IMAGE) $(STAR_IMAGE) $(BIGEXE_IMAGE) $(DIR_IMAGE) $(FORMAT_IMAGE) $(FORMAT_HD_IMAGE) $(FAT16_HD_IMAGE) $(PARTEDIT_HD_IMAGE) $(MULTILET_HD_IMAGE) $(EXTPART_HD_IMAGE) $(SUBST_IMAGE) $(BATCH_IMAGE) $(DISK_IMAGE) $(GZIP_IMAGE) $(UTILS_IMAGE) $(DISKCOPY_IMAGE) $(DISKCOMP_IMAGE) $(ANSI_IMAGE) $(STUBCFG_IMAGE) $(INSTALL_IMAGE)
	$(PYTHON) -m tests.test_wcc
	$(PYTHON) -m tests.test_bios_roms
	$(PYTHON) -m tests.test_bios_services
	$(PYTHON) -m tests.test_boot_e2e
	$(PYTHON) -m tests.test_dos_compat
	$(PYTHON) -m tests.test_ping_e2e
	$(PYTHON) -m tests.test_dhcp_e2e
	$(PYTHON) -m tests.test_telnet_e2e
	$(PYTHON) -m tests.test_net_resident_e2e
	$(PYTHON) -m tests.test_star_e2e
	$(PYTHON) -m tests.test_bigexe_e2e
	$(PYTHON) -m tests.test_dir_e2e
	$(PYTHON) -m tests.test_format_e2e
	$(PYTHON) -m tests.test_format_options_e2e
	$(PYTHON) -m tests.test_sys_e2e
	$(PYTHON) -m tests.test_format_hd_e2e
	$(PYTHON) -m tests.test_fat16_hd_e2e
	$(PYTHON) -m tests.test_partedit_hd_e2e
	$(PYTHON) -m tests.test_multilet_hd_e2e
	$(PYTHON) -m tests.test_extpart_hd_e2e
	$(PYTHON) -m tests.test_subst_e2e
	$(PYTHON) -m tests.test_batch_e2e
	$(PYTHON) -m tests.test_disk_tools_e2e
	$(PYTHON) -m tests.test_gzip_e2e
	$(PYTHON) -m tests.test_utils_e2e
	$(PYTHON) -m tests.test_diskcopy_e2e
	$(PYTHON) -m tests.test_diskcomp_e2e
	$(PYTHON) -m tests.test_ansi_e2e
	$(PYTHON) -m tests.test_stubcfg_e2e
	$(PYTHON) -m tests.test_install_hd_e2e
	$(PYTHON) -m tests.starfield_alg_test

test-dos-compat: $(COMPAT_IMAGE)
	$(PYTHON) -m tests.test_dos_compat

test-ping: $(PING_IMAGE)
	$(PYTHON) -m tests.test_ping_e2e

test-dhcp: $(DHCP_IMAGE)
	$(PYTHON) -m tests.test_dhcp_e2e

test-telnet: $(TELNET_IMAGE)
	$(PYTHON) -m tests.test_telnet_e2e

test-net: $(NET_IMAGE)
	$(PYTHON) -m tests.test_net_resident_e2e

test-star: $(STAR_IMAGE)
	$(PYTHON) -m tests.test_star_e2e

test-bigexe: $(BIGEXE_IMAGE)
	$(PYTHON) -m tests.test_bigexe_e2e

# Optional: requires fixtures/guest/elite/ELITE.EXE (gitignored).
test-elite: bios
	@if [ ! -f "$(ELITE_EXE)" ]; then echo "test-elite: SKIP (no $(ELITE_EXE))"; exit 0; fi
	$(MAKE) $(ELITE_IMAGE)
	$(PYTHON) -m tests.test_elite_e2e

test-dir: $(DIR_IMAGE)
	$(PYTHON) -m tests.test_dir_e2e

test-format: $(FORMAT_IMAGE)
	$(PYTHON) -m tests.test_format_e2e

test-format-options: all
	$(PYTHON) -m tests.test_format_options_e2e

test-sys: all
	$(PYTHON) -m tests.test_sys_e2e

test-format-hd: $(FORMAT_HD_IMAGE)
	$(PYTHON) -m tests.test_format_hd_e2e

test-fat16-hd: $(FAT16_HD_IMAGE)
	$(PYTHON) -m tests.test_fat16_hd_e2e

test-partedit-hd: $(PARTEDIT_HD_IMAGE)
	$(PYTHON) -m tests.test_partedit_hd_e2e

test-multilet-hd: $(MULTILET_HD_IMAGE)
	$(PYTHON) -m tests.test_multilet_hd_e2e

test-extpart-hd: $(EXTPART_HD_IMAGE)
	$(PYTHON) -m tests.test_extpart_hd_e2e

test-subst: $(SUBST_IMAGE)
	$(PYTHON) -m tests.test_subst_e2e

test-batch: $(BATCH_IMAGE)
	$(PYTHON) -m tests.test_batch_e2e

test-disk: $(DISK_IMAGE)
	$(PYTHON) -m tests.test_disk_tools_e2e

test-gzip: $(GZIP_IMAGE)
	$(PYTHON) -m tests.test_gzip_e2e

test-utils: $(UTILS_IMAGE)
	$(PYTHON) -m tests.test_utils_e2e

test-diskcopy: $(DISKCOPY_IMAGE)
	$(PYTHON) -m tests.test_diskcopy_e2e

test-diskcomp: $(DISKCOMP_IMAGE)
	$(PYTHON) -m tests.test_diskcomp_e2e

test-ansi: $(ANSI_IMAGE)
	$(PYTHON) -m tests.test_ansi_e2e

test-stubcfg: $(STUBCFG_IMAGE)
	$(PYTHON) -m tests.test_stubcfg_e2e

test-install-hd: $(INSTALL_IMAGE)
	$(PYTHON) -m tests.test_install_hd_e2e

test-fd-img: bios $(FD_IMG)
	$(PYTHON) -m tests.test_fd_img_e2e

clean:
	rm -f $(BUILD_DIR)/*.bin $(BUILD_DIR)/*.elf $(BUILD_DIR)/*.img $(BUILD_DIR)/*.log $(BUILD_DIR)/*.o $(BUILD_DIR)/as8086.*.s $(BUILD_DIR)/starfield.s $(BUILD_DIR)/*.com $(BUILD_DIR)/*.sys
	rm -rf $(BIOS_TEST_BUILD)

# Copyright (c) 2020--2023 TK Chia
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifeq "" "$(wildcard config.cache)"
$(error you must configure this project first!)
endif

-include config.cache
-include $(conf_Lolwutconf_dir)/lolwutconf.mk

EFISRCDIR := $(abspath $(conf_Srcdir))/efi.krinkinmu
LAISRCDIR := $(abspath $(conf_Srcdir))/lai
CFLAGS_COMMON = -pie -fPIC -ffreestanding -static -nostdlib -MMD \
		-mno-red-zone -O2 -std=c11 -Wall -Werror -pedantic
CFLAGS = $(CFLAGS_COMMON)
CFLAGS2 = $(CFLAGS_COMMON)
LDFLAGS2 += -Wl,--hash-style=sysv

QEMUFLAGS = -m 224m -serial stdio -usb -device usb-ehci -device qemu-xhci \
	    $(QEMUEXTRAFLAGS)

ifneq "" "$(SBSIGN_MOK)"
STAGE1 = $(STAGE1_SIGNED)
else
STAGE1 = $(STAGE1_UNSIGNED)
endif
STAGE1_SIGNED = bootx64.signed.efi
STAGE1_UNSIGNED = bootx64.efi
STAGE1_CONFIG = config.txt
STAGE2 = stage2.sys
STAGE2_BINDIR = /EFI/biefirc
LEGACY_MBR = legacy-mbr.bin

default: $(STAGE1) $(STAGE1_CONFIG) $(STAGE2) hd.img hd.img.zip
.PHONY: default

ifneq "" "$(SBSIGN_MOK)"
$(STAGE1_SIGNED): $(STAGE1_UNSIGNED)
	sbsign --key $(SBSIGN_MOK:=.key) --cert $(SBSIGN_MOK:=.crt) \
	       --output $@ $<
endif

$(STAGE1_UNSIGNED): $(wildcard $(EFISRCDIR)/* $(EFISRCDIR)/*/*)
	$(RM) -r efi.build
	cp -r $(EFISRCDIR) efi.build
	$(MAKE) -C efi.build -e CC='$(CC)' CFLAGS='$(CFLAGS)' boot.efi
	cp efi.build/boot.efi $@

$(STAGE1_CONFIG):
	echo 'kernel: $(subst /,\,$(STAGE2_BINDIR))\$(STAGE2)' >$@

$(STAGE2): stage2/start.o stage2/stage2.ld
	$(CC2) $(CFLAGS2) $(LDFLAGS2) $(patsubst %,-T %,$(filter %.ld,$^)) \
	       -o $@ $(filter-out %.ld,$^) $(LDLIBS2)

$(LEGACY_MBR): legacy-mbr.o legacy-mbr.ld
	$(CC2) $(CFLAGS2) $(LDFLAGS2) $(patsubst %,-T %,$(filter %.ld,$^)) \
	       -o $@ $(filter-out %.ld,$^) $(LDLIBS2)

%.o: %.c
	mkdir -p $(@D)
	$(CC2) $(CPPFLAGS2) $(CFLAGS2) -c -o $@ $<

%.o: %.S
	mkdir -p $(@D)
	$(CC2) $(CPPFLAGS2) $(CFLAGS2) -c -o $@ $<

hd.img.zip: hd.img
	$(RM) $@.tmp
	zip -9 $@.tmp $^
	mv $@.tmp $@

# mkdosfs only understands a --offset option starting from version 4.2 (Jan
# 2021).  For older versions of mkdosfs, we need to use a workaround.
hd.img: $(STAGE1) $(STAGE1_CONFIG) $(STAGE2) $(LEGACY_MBR)
	$(RM) $@.tmp
	dd if=/dev/zero of=$@.tmp bs=1048576 count=32
	dd if=$(LEGACY_MBR) of=$@.tmp conv=notrunc
	echo start=32K type=0B bootable | sfdisk $@.tmp
	mkdosfs -v -F16 --offset 64 $@.tmp || ( \
	    dd if=$@.tmp of=$@.2.tmp ibs=1024 skip=32 obs=1048576 && \
	    mkdosfs -v -F16 $@.2.tmp && \
	    dd if=$@.2.tmp of=$@.tmp obs=1024 seek=32 ibs=1048576 \
				     conv=notrunc && \
	    $(RM) $@.2.tmp \
	)
	mmd -i $@.tmp@@32K ::/EFI ::/EFI/BOOT ::$(STAGE2_BINDIR)
	mcopy -i $@.tmp@@32K $< ::/EFI/BOOT/bootx64.efi
	mcopy -i $@.tmp@@32K $(STAGE1_CONFIG) ::/EFI/BOOT/
	mcopy -i $@.tmp@@32K $(STAGE2) ::$(STAGE2_BINDIR)
	mv $@.tmp $@

hd.vdi: hd.img
	qemu-img convert $< -O vdi $@.tmp
	mv $@.tmp $@

distclean: clean
	$(RM) config.cache
ifeq "$(conf_Separate_build_dir)" "yes"
	-$(RM) GNUmakefile
endif
.PHONY: distclean

clean:
	$(RM) -r $(STAGE1_CONFIG) efi.build
	set -e; \
	for d in . stage2; do \
		if test -d "$$d"; then \
			(cd "$$d" && \
			 $(RM) *.[ods] *.so *.efi *.img *.img.zip *.vdi \
			       *.map *.stamp *.sys *.elf *.bin *~); \
		fi; \
	done
ifeq "$(conf_Separate_build_dir)" "yes"
	$(RM) -r efi
else
	$(MAKE) -C efi clean
endif
.PHONY: clean

run-qemu: hd.img
	qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -hda $< $(QEMUFLAGS)
.PHONY: run-qemu

-include *.d stage2/*.d

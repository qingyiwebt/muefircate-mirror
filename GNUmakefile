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

EFISRCDIR := '$(abspath $(conf_Srcdir))'/efi
LAISRCDIR := '$(abspath $(conf_Srcdir))'/lai
LIBEFI = efi/boot.efi
CFLAGS = -ffreestanding -MMD -mno-red-zone -std=c11 -Wall -Werror -pedantic -O2
CFLAGS2 = $(CFLAGS)
AS2 = nasm

QEMUFLAGS = -m 224m -serial stdio -usb -device usb-ehci -device qemu-xhci \
	    $(QEMUEXTRAFLAGS)

ifneq "" "$(SBSIGN_MOK)"
STAGE1 = bootx64.signed.efi
else
STAGE1 = bootx64.efi
endif
STAGE2 = stage2.sys
LEGACY_MBR = legacy-mbr.bin

default: $(STAGE1) hd.img hd.img.zip
.PHONY: default

ifneq "" "$(SBSIGN_MOK)"
bootx64.signed.efi: bootx64.efi
	sbsign --key $(SBSIGN_MOK:=.key) --cert $(SBSIGN_MOK:=.crt) \
	       --output $@ $<
endif

bootx64.efi:
ifeq "$(conf_Separate_build_dir)" "yes"
	$(RM) -r efi
	cp -r $(EFISRCDIR) efi
endif
	$(MAKE) -C efi -e CC='$(CC)' CFLAGS='$(CFLAGS)' boot.efi
	cp efi/boot.efi $@

$(LEGACY_MBR): legacy-mbr.asm
	$(AS2) -f bin -MD $(@:.bin=.d) -o $@ $< 

hd.img.zip: hd.img
	$(RM) $@.tmp
	zip -9 $@.tmp $^
	mv $@.tmp $@

# mkdosfs only understands a --offset version starting from version 4.2 (Jan
# 2021).  For older versions of mkdosfs, we need to use a workaround.
hd.img: $(STAGE1) $(LEGACY_MBR)
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
	mmd -i $@.tmp@@32K ::/EFI ::/EFI/BOOT ::/EFI/biefirc
	mcopy -i $@.tmp@@32K $< ::/EFI/BOOT/bootx64.efi
	# mcopy -i $@.tmp@@32K $(STAGE2) ::/EFI/biefirc/
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

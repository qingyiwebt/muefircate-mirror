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
EFISRCDEPS := $(wildcard $(EFISRCDIR)/* $(EFISRCDIR)/*/*)
CFLAGS_COMMON = -ffreestanding -static -nostdlib -MMD \
		-mno-red-zone -O2 -std=c11 -Wall -Werror -pedantic
CFLAGS += $(CFLAGS_COMMON) -fPIE -fno-unwind-tables
LD = lld
CC2_GCC_INCLUDE := $(patsubst %,-isystem %, \
		     $(wildcard \
		       $(shell $(CC2) $(CFLAGS_COMMON) \
				      -print-file-name=include-fixed) \
		       $(shell $(CC2) $(CFLAGS_COMMON) \
				      -print-file-name=include)))
CFLAGS2 += $(CFLAGS_COMMON) -fPIE -nostdinc $(CC2_GCC_INCLUDE) \
			    -isystem $(MACRON2_LIBC_PREFIX)/include
LDFLAGS2 += -static-pie -s -Wl,--hash-style=sysv,-Map=$(@:=.map)
NINJA = ninja
NINJAFLAGS =

QEMUFLAGS = -m 224m -serial stdio -usb -device usb-ehci -device qemu-xhci \
	    $(QEMUEXTRAFLAGS)

ifneq "" "$(SBSIGN_MOK)"
MUON = $(MUON_SIGNED)
MACRON1 = $(MACRON1_SIGNED)
else
MUON = $(MUON_UNSIGNED)
MACRON1 = $(MACRON1_UNSIGNED)
endif
MUON_SIGNED = muon.signed.efi
MUON_UNSIGNED = muon.efi
MACRON1_SIGNED = macron1.signed.efi
MACRON1_UNSIGNED = macron1.efi
MACRON1_CONFIG = config.txt
MACRON2 = macron2.sys
MACRON2_BINDIR = /EFI/biefirc
MACRON2_LIBC_PREFIX = picolibc.build/staging/picolibc/x86_64-linux-gnu
MACRON2_LIBC = $(MACRON2_LIBC_PREFIX)/lib/libc.a
LEGACY_MBR = legacy-mbr.bin

default: $(MUON) muon.img muon.img.zip \
	 $(MACRON1) $(MACRON1_CONFIG) $(MACRON2) macron.img macron.img.zip
.PHONY: default

ifneq "" "$(SBSIGN_MOK)"
%.signed.efi: %.efi
	sbsign --key $(SBSIGN_MOK:=.key) --cert $(SBSIGN_MOK:=.crt) \
	       --output $@ $<
endif

$(MUON_UNSIGNED): muon/main.o muon/boot-dev.o muon/clib.o muon/log.o
	$(LD) -flavor link -subsystem:efi_application -entry:efi_main \
	      -filealign:16 -out:$@ $^

$(MACRON1_UNSIGNED): $(EFISRCDEPS)
	$(RM) -r efi.build
	cp -r $(EFISRCDIR) efi.build
	$(MAKE) -C efi.build -e CC='$(CC)' CFLAGS='$(CFLAGS)' boot.efi
	cp efi.build/boot.efi $@

$(MACRON1_CONFIG):
	echo 'kernel: $(subst /,\,$(MACRON2_BINDIR))\$(MACRON2)' >$@

$(MACRON2): macron2/start.o macron2/cons.early.o macron2/cons-font-default.o \
	    macron2/cons-klog.early.o macron2/macron2.ld $(MACRON2_LIBC)
	$(CC2) $(CFLAGS2) $(LDFLAGS2) $(patsubst %,-T %,$(filter %.ld,$^)) \
	       -o $@ $(filter-out %.ld,$^) $(LDLIBS2)

$(LEGACY_MBR): legacy-mbr.o legacy-mbr.ld
	$(CC2) $(CFLAGS2) $(LDFLAGS2) $(patsubst %,-T %,$(filter %.ld,$^)) \
	       -o $@ $(filter-out %.ld,$^) $(LDLIBS2)

muon/%.o: muon/%.c $(EFISRCDEPS)
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -I $(EFISRCDIR) $(CFLAGS) -c -o $@ $<

muon/%.o: $(EFISRCDIR)/%.c $(EFISRCDEPS)
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -I $(EFISRCDIR) $(CFLAGS) -c -o $@ $<

macron2/%.early.o: macron2/%.early.c $(MACRON2_LIBC)
	mkdir -p $(@D)
	$(CC2) $(CPPFLAGS2) $(CFLAGS2) -c -o $@ $<

macron2/%.o: macron2/%.c $(MACRON2_LIBC)
	mkdir -p $(@D)
	$(CC2) $(CPPFLAGS2) $(CFLAGS2) -c -o $@ $<

%.o: %.S $(MACRON2_LIBC)
	mkdir -p $(@D)
	$(CC2) $(CPPFLAGS2) $(CFLAGS2) -c -o $@ $<

$(MACRON2_LIBC):
	$(NINJA) $(NINJAFLAGS) -C picolibc.build
	$(NINJA) $(NINJAFLAGS) -C picolibc.build install

%.img.zip: %.img
	$(RM) $@.tmp
	zip -9 $@.tmp $^
	mv $@.tmp $@

# mkdosfs only understands a --offset option starting from version 4.2 (Jan
# 2021).  For older versions of mkdosfs, we need to use a workaround.
muon.img: $(MUON) $(LEGACY_MBR)
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
	mmd -i $@.tmp@@32K ::/EFI ::/EFI/BOOT
	mcopy -i $@.tmp@@32K $< ::/EFI/BOOT/bootx64.efi
	mv $@.tmp $@

macron.img: $(MACRON1) $(MACRON1_CONFIG) $(MACRON2) $(LEGACY_MBR)
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
	mmd -i $@.tmp@@32K ::/EFI ::/EFI/BOOT ::$(MACRON2_BINDIR)
	mcopy -i $@.tmp@@32K $< ::/EFI/BOOT/bootx64.efi
	mcopy -i $@.tmp@@32K $(MACRON1_CONFIG) ::/EFI/BOOT/
	mcopy -i $@.tmp@@32K $(MACRON2) ::$(MACRON2_BINDIR)
	mv $@.tmp $@

%.vdi: %.img
	qemu-img convert $< -O vdi $@.tmp
	mv $@.tmp $@

distclean: clean
	$(RM) -r config.cache cross-x86_64.pic.txt picolibc.build
ifeq "$(conf_Separate_build_dir)" "yes"
	-$(RM) GNUmakefile
endif
.PHONY: distclean

clean:
	$(RM) -r $(MACRON1_CONFIG) efi.build
	set -e; \
	for d in . muon macron2; do \
		if test -d "$$d"; then \
			(cd "$$d" && \
			 $(RM) *.[ods] *.so *.efi *.img *.img.zip *.vdi \
			       *.map *.stamp *.sys *.elf *.bin *~); \
		fi; \
	done
ifeq "$(conf_Separate_build_dir)" "yes"
	$(RM) -r muon macron2
endif
	$(NINJA) -C picolibc.build clean
.PHONY: clean

run-muon run-muon-qemu: muon.img
	qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -hda $< $(QEMUFLAGS)
.PHONY: run-muon run-muon-qemu

run-macron run-macron-qemu: macron.img
	qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -hda $< $(QEMUFLAGS)
.PHONY: run-macron run-macron-qemu

-include *.d muon/*.d macron2/*.d

/*
 * Copyright (c) 2021--2023 TK Chia
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* GNU ld compatible linker script for stage 2. */

OUTPUT_FORMAT ("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH (i386:x86-64)
ENTRY (_start)

PHDRS
{
  text PT_LOAD FILEHDR PHDRS;
  data PT_LOAD;
}

SECTIONS
{

  HIDDEN (_PGSZ = CONSTANT (COMMONPAGESIZE));

  . = ALIGN (0x1000000 + SIZEOF_HEADERS, 0x10);
  .text . :
  {
    *(.text .stub .text.* .gnu.linkonce.t.*)
    *(.gnu.warning)
    . = ALIGN (0x10);
    PROVIDE (_etext = .);

    *(.rodata .rodata.* .gnu.linkonce.r.*)
    . = ALIGN (0x10);
    PROVIDE (_erodata = .);
  } :text

  .data (. % _PGSZ ? . + _PGSZ : .) :
  {
    *(.data .data.* .gnu.linkonce.d.*)
    . = ALIGN (0x10);
    PROVIDE (_edata = .);

    *(.bss .bss.* .gnu.linkonce.b.*)
    *(COMMON)
    PROVIDE (_end = .);
  } :data

  /DISCARD/ :
  {
    *(.note.GNU-stack .gnu_* .gnu.* .note.gnu.build-id
      .stab* .debug_* .eh_frame .comment)
  }
}

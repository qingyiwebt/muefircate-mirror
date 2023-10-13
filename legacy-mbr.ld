/*
 * Copyright (c) 2021--2023 TK Chia
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* GNU ld compatible linker script for the dummy MBR. */

OUTPUT_FORMAT ("binary")
OUTPUT_ARCH (i386:x86-64)
ENTRY (_start)

SECTIONS
{

  .text 0x7c00 :
  {
    *(.text .stub .text.* .gnu.linkonce.t.*)
    *(.rodata .rodata.* .gnu.linkonce.r.*)
    *(.data .data.* .gnu.linkonce.d.*)
    *(.bss .bss.* .gnu.linkonce.b.*)
    *(COMMON)
  }

  /DISCARD/ :
  {
    *(.note.GNU-stack .gnu_* .gnu.* .stab* .debug_* .eh_frame .comment)
  }
}

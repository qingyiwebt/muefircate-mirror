/*
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors of
 * this software dedicate any and all copyright interest in the software to
 * the public domain.  We make this dedication for the benefit of the public
 * at large and to the detriment of our heirs and successors.  We intend
 * this dedication to be an overt act of relinquishment in perpetuity of all
 * present and future rights to this software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __EFI_BLOCK_IO_PROTOCOL_H__
#define __EFI_BLOCK_IO_PROTOCOL_H__

#include "efi/types.h"

#define EFI_BLOCK_IO_PROTOCOL_GUID \
    { 0x964e5b21, 0x6459, 0x11d2, \
      { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } }

typedef uint64_t efi_lba_t;

struct efi_block_io_media
{
  uint32_t media_id;
  efi_boolean_t removable_media;
  efi_boolean_t media_present;
  efi_boolean_t logical_partition;
  efi_boolean_t read_only;
  efi_boolean_t write_caching;
  uint32_t block_size;
  uint32_t io_align;
  efi_lba_t last_block;
};

struct efi_block_io_protocol
{
  uint64_t revision;
  struct efi_block_io_media *media;
  efi_status_t (*reset) (struct efi_block_io_protocol *, efi_boolean_t);
  efi_status_t (*read_blocks) (struct efi_block_io_protocol *,
			       uint32_t, efi_lba_t, uint64_t, void *);
  efi_status_t (*write_blocks) (struct efi_block_io_protocol *,
			        uint32_t, efi_lba_t, uint64_t, const void *);
  efi_status_t (*flush_blocks) (struct efi_block_io_protocol *);
};

#endif

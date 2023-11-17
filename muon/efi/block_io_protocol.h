#ifndef __EFI_BLOCK_IO_PROTOCOL_H__
#define __EFI_BLOCK_IO_PROTOCOL_H__

#include "efi/types.h"

#define EFI_BLOCK_IO_PROTOCOL_GUID \
    { 0x0964e5b21, 0x6459, 0x11d2, \
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

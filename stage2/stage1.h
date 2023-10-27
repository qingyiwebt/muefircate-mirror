/*
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <https://unlicense.org>
 */

/**
 * @internal Definitions for interfacing with the stage 1 bootloader.
 */

#ifndef _H_STAGE2_STAGE1
#define _H_STAGE2_STAGE1

#include <stddef.h>
#include <stdint.h>

enum efi_graphics_pixel_format
{
  EFI_PIXEL_RED_GREEN_BLUE_RESERVED_8_BIT_PER_COLOR,
  EFI_PIXEL_BLUE_GREEN_RED_RESERVED_8_BIT_PER_COLOR,
  EFI_PIXEL_BIT_MASK,
  EFI_PIXEL_BLT_ONLY,
  EFI_PIXEL_FORMAT_MAX
};

struct efi_pixel_bitmask
{
  uint32_t red_mask;
  uint32_t green_mask;
  uint32_t blue_mask;
  uint32_t reserved_mask;
};

struct efi_graphics_output_mode_information
{
  uint32_t version;
  uint32_t horizontal_resolution;
  uint32_t vertical_resolution;
  enum efi_graphics_pixel_format pixel_format;
  struct efi_pixel_bitmask pixel_information;
  uint32_t pixels_per_scan_line;
};

struct boot_video
{
  struct efi_graphics_output_mode_information info;
  uint64_t frame_buffer_base;
};

struct boot_reserve
{
  const char *name;
  uint64_t begin;
  uint64_t end;
};

enum efi_memory_type
{
  EFI_RESERVED_MEMORY_TYPE,
  EFI_LOADER_CODE,
  EFI_LOADER_DATA,
  EFI_BOOT_SERVICES_CODE,
  EFI_BOOT_SERVICES_DATA,
  EFI_RUNTIME_SERVICES_CODE,
  EFI_RUNTIME_SERVICES_DATA,
  EFI_CONVENTIAL_MEMORY,
  EFI_UNUSABLE_MEMORY,
  EFI_ACPI_RECLAIM_MEMORY,
  EFI_ACPI_MEMORY_NVS,
  EFI_MEMORY_MAPPED_IO,
  EFI_MEMORY_MAPPED_IO_PORT_SPACE,
  EFI_PAL_CODE,
  EFI_PERSISTENT_MEMORY,
  EFI_MAX_MEMORY_TYPE
};

struct efi_memory_descriptor
{
  uint32_t type;
  uint64_t physical_start;
  uint64_t virtual_start;
  uint64_t pages;
  uint64_t attributes;
};

typedef uint64_t efi_uint_t;

struct stage1
{
  struct boot_reserve *reserve;
  size_t reserves;
  struct efi_memory_descriptor *mem_map;
  efi_uint_t mem_map_size;
  efi_uint_t mem_map_desc_size;
};

#endif

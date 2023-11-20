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

#ifndef __EFI_DEVICE_PATH_UTILITIES_PROTOCOL_H__
#define __EFI_DEVICE_PATH_UTILITIES_PROTOCOL_H__

#include "efi/types.h"

#define EFI_DEVICE_PATH_UTILITIES_PROTOCOL_GUID \
    { 0x0379be4e, 0xd706, 0x437d, \
      { 0xb0, 0x37, 0xed, 0xb8, 0x2f, 0xb7, 0x72, 0xa4 } }

struct efi_device_path_utilities_protocol
{
  uint64_t (*get_device_path_size) (const struct efi_device_path_protocol *);
  struct efi_device_path_protocol *(*duplicate_device_path)
				     (const struct efi_device_path_protocol *);
  struct efi_device_path_protocol *(*append_device_path)
				     (const struct efi_device_path_protocol *,
				      const struct efi_device_path_protocol *);
  struct efi_device_path_protocol *(*append_device_node)
				     (const struct efi_device_path_protocol *,
				      const struct efi_device_path_protocol *);
  struct efi_device_path_protocol *(*append_device_path_instance)
				     (const struct efi_device_path_protocol *,
				      const struct efi_device_path_protocol *);
  struct efi_device_path_protocol *(*get_next_device_path_instance)
				     (struct efi_device_path_protocol **,
				      uint64_t *);
  efi_boolean_t (*is_device_path_multi_instance)
		  (const struct efi_device_path_protocol *);
  struct efi_device_path_protocol *(*create_device_node)
				     (uint8_t, uint8_t, uint16_t);
};

#endif

/*
 * Copyright (c) 2023 TK Chia
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "efi/efi.h"
#include "loader.h"
#include "log.h"
#include "efi/block_io_protocol.h"

static efi_status_t
find_boot_dev (efi_handle_t handle, struct efi_system_table *system,
	       struct efi_block_io_protocol **bio)
{
  struct efi_guid guid1 = EFI_LOADED_IMAGE_PROTOCOL_GUID;
  struct efi_guid guid2 = EFI_DEVICE_PATH_PROTOCOL_GUID;
  struct efi_guid guid3 = EFI_BLOCK_IO_PROTOCOL_GUID;
  struct efi_loaded_image_protocol *image;
  struct efi_device_path_protocol *path_node;
  efi_handle_t device;
  efi_status_t status
    = system->boot->handle_protocol (handle, &guid1, (void **) &image);
  if (status != EFI_SUCCESS)
    {
      err (system, "failed to get loader image protocol\r\n");
      return status;
    }
  device = image->device;
  status = system->boot->handle_protocol (device, &guid2,
					  (void **) &path_node);
  if (status != EFI_SUCCESS)
    {
      err (system, "failed to get device's device path protocol\r\n");
      return status;
    }
  do
    {
      info (system, "%u %u %u\r\n",
		    (unsigned) path_node->type,
		    (unsigned) path_node->subtype,
		    (unsigned) path_node->length);
      path_node = (void *) ((char *) path_node + path_node->length);
    } while (path_node->type != 0x7f);
  status = system->boot->handle_protocol (device, &guid3, (void **) bio);
  if (status != EFI_SUCCESS)
    {
      err (system, "failed to get block I/O protocol\r\n");
      return status;
    }
  return EFI_SUCCESS;
}

efi_status_t
efi_main (efi_handle_t handle, struct efi_system_table *system)
{
  efi_status_t status;
  struct efi_block_io_protocol *bio;

  info (system, "Finding boot device...\r\n");
  status = find_boot_dev (handle, system, &bio);
  if (status != EFI_SUCCESS)
    return status;

  return EFI_SUCCESS;
}

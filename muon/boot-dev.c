/*
 * Copyright (c) 2023 TK Chia
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "efi/efi.h"
#include "log.h"
#include "efi/block_io_protocol.h"
#include "efi/device_path_to_text_protocol.h"
#include "efi/device_path_utilities_protocol.h"

enum efi_device_path_node_type
{
  EFI_HARDWARE_DEVICE_PATH = 0x01,
  EFI_ACPI_DEVICE_PATH,
  EFI_MESSAGING_DEVICE_PATH,
  EFI_MEDIA_DEVICE_PATH,
  EFI_BBS_DEVICE_PATH,
  EFI_END_DEVICE_PATH = 0x7f
};

enum efi_media_device_path_subtype
{
  EFI_MEDIA_HARDDRIVE_DP = 0x01
};

struct efi_harddrive_device_path
{
  struct efi_device_path_protocol header;
  uint8_t partition_number[4];
  uint8_t partition_start[8];
  uint8_t partition_size[8];
  uint8_t signature[16];
  uint8_t mbr_type;
  uint8_t signature_type;
};

static const efi_status_t EFI_OUT_OF_RESOURCES = 0x8000000000000009,
			  EFI_NOT_FOUND = 0x800000000000000e;
static struct efi_guid guid1 = EFI_DEVICE_PATH_PROTOCOL_GUID;
static struct efi_guid guid2 = EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID;
static struct efi_guid guid3 = EFI_DEVICE_PATH_UTILITIES_PROTOCOL_GUID;
static struct efi_guid guid4 = EFI_BLOCK_IO_PROTOCOL_GUID;
static struct efi_guid guid5 = EFI_LOADED_IMAGE_PROTOCOL_GUID;

static uint16_t
dev_node_length (const struct efi_device_path_protocol *node)
{
  return (uint16_t) node->length[1] << 8 | node->length[0];
}

static efi_status_t
find_whole_drive (struct efi_system_table *system,
		  efi_handle_t boot_dev, efi_handle_t *whole_dev)
{
  struct efi_device_path_protocol *dev_path, *node;
  char *p;
  uint16_t *text;
  struct efi_device_path_to_text_protocol *path_to_text;
  struct efi_harddrive_device_path *hd_node;
  struct efi_device_path_utilities_protocol *path_utils;
  uint8_t type, subtype, *q;

  struct efi_boot_table *boot = system->boot;
  efi_status_t status = boot->handle_protocol (boot_dev, &guid1,
					       (void **) &dev_path);
  if (status != EFI_SUCCESS)
    {
      info (system, "  no path protocol for device?\r\n");
      return status;
    }

  status = boot->locate_protocol (&guid2, NULL, (void **) &path_to_text);
  if (status != EFI_SUCCESS)
    {
      info (system, "  no device path to text protocol\r\n");
      return status;
    }

  text = path_to_text->convert_device_path_to_text (dev_path, true, true);
  if (text)
    {
      info (system, "  boot device is %w\r\n", text);
      boot->free_pool (text);
    }

  node = dev_path;
  p = (char *) dev_path;
  type = node->type;
  subtype = node->subtype;
  while (type != EFI_MEDIA_DEVICE_PATH
	 || subtype != EFI_MEDIA_HARDDRIVE_DP)
    {
      if (type == EFI_END_DEVICE_PATH)
	{
	  info (system, "  boot device is not a hard drive partition\r\n");
	  return EFI_NOT_FOUND;
	}
      p += dev_node_length (node);
      node = (struct efi_device_path_protocol *) p;
      type = node->type;
      subtype = node->subtype;
    }

  hd_node = (struct efi_harddrive_device_path *) p;
  q = hd_node->partition_number;
  if (! q[0] && ! q[1] && ! q[2] && ! q[3])
    {
      info (system, "  boot device is whole hard drive\r\n");
      *whole_dev = boot_dev;
      return EFI_SUCCESS;
    }

  status = boot->locate_protocol (&guid3, NULL, (void **) &path_utils);
  if (status != EFI_SUCCESS)
    {
      info (system, "  no device path utilities protocol\r\n");
      return status;
    }

  dev_path = path_utils->duplicate_device_path (dev_path);
  if (! dev_path)
    {
      info (system, "  cannot duplicate device path");
      return EFI_OUT_OF_RESOURCES;
    }

  node = dev_path;
  p = (char *) dev_path;
  type = node->type;
  subtype = node->subtype;
  while (type != EFI_MEDIA_DEVICE_PATH
	 || subtype != EFI_MEDIA_HARDDRIVE_DP)
    {
      p += dev_node_length (node);
      node = (struct efi_device_path_protocol *) p;
      type = node->type;
      subtype = node->subtype;
    }

  hd_node = (struct efi_harddrive_device_path *) p;
  q = hd_node->partition_number;
  q[0] = q[1] = q[2] = q[3] = 0;
  q = hd_node->partition_start;
  q[0] = q[1] = q[2] = q[3] = q[4] = q[5] = q[6] = q[7] = 0;
  q = hd_node->partition_size;
  q[0] = q[1] = q[2] = q[3] = q[4] = q[5] = q[6] = q[7] = 0xff;  /* FIXME? */

  status = boot->locate_device_path (&guid4, &dev_path, whole_dev);
  boot->free_pool (dev_path);

  if (status == EFI_SUCCESS)
    {
      if (boot->handle_protocol (*whole_dev, &guid1, (void **) &dev_path)
	  == EFI_SUCCESS)
	{
	  text = path_to_text->convert_device_path_to_text (dev_path,
							    true, true);
	  if (text)
	    {
	      info (system, "  whole hard drive is %w\r\n", text);
	      boot->free_pool (text);
	    }
	}
    }
  else
    info (system, "  cannot locate whole hard drive\r\n");

  return status;
}

efi_status_t
find_boot_dev (efi_handle_t handle, struct efi_system_table *system,
	       struct efi_block_io_protocol **bio)
{
  struct efi_loaded_image_protocol *image;
  efi_handle_t boot_dev;

  struct efi_boot_table *boot = system->boot;
  efi_status_t status
    = boot->handle_protocol (handle, &guid5, (void **) &image);
  if (status != EFI_SUCCESS)
    {
      err (system, "failed to get loader image protocol\r\n");
      return status;
    }

  status = find_whole_drive (system, image->device, &boot_dev);
  if (status != EFI_SUCCESS)
    boot_dev = image->device;

  status = boot->handle_protocol (boot_dev, &guid4, (void **) bio);
  if (status != EFI_SUCCESS)
    err (system, "failed to get block I/O protocol\r\n");

  return status;
}

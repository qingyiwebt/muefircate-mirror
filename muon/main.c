/*
 * Copyright (c) 2023 TK Chia
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "efi/efi.h"
#include "efi/block_io_protocol.h"
#include "log.h"

static const uint32_t EFI_EVT_TIMER = 0x80000000;
static const efi_tpl_t EFI_TPL_APPLICATION = 4;

extern efi_status_t find_boot_dev (efi_handle_t, struct efi_system_table *,
				   struct efi_block_io_protocol **);

static efi_status_t
snooze_1 (struct efi_boot_table *boot, uint64_t us)
{
  while (us > 10000)
    {
      boot->stall (10000);
      us -= 10000;
    }
  return boot->stall (us);
}

static efi_status_t
snooze (struct efi_system_table *system, uint64_t us)
{
  struct efi_boot_table *boot = system->boot;
  efi_event_t event;
  uint64_t index;

  if (boot->create_event (EFI_EVT_TIMER, EFI_TPL_APPLICATION,
			  NULL, NULL, &event) != EFI_SUCCESS)
    return snooze_1 (boot, us);

  if (boot->set_timer (event, EFI_TIMER_RELATIVE, us * 10) != EFI_SUCCESS
      || boot->wait_for_event (1, &event, &index) != EFI_SUCCESS)
    {
      boot->close_event (event);
      return snooze_1 (boot, us);
    }

  return boot->close_event (event);
}

static efi_status_t
efi_main_1 (efi_handle_t handle, struct efi_system_table *system)
{
  efi_status_t status;
  struct efi_block_io_protocol *bio;

  info (system, "Finding boot device...\r\n");
  status = find_boot_dev (handle, system, &bio);
  if (status != EFI_SUCCESS)
    return status;

  return EFI_SUCCESS;
}

efi_status_t
efi_main (efi_handle_t handle, struct efi_system_table *system)
{
  efi_status_t status;
  do
    {
      status = efi_main_1 (handle, system);
      snooze (system, 3000000);
    } while (status == EFI_SUCCESS);
  snooze (system, 3000000);
  return status;
}

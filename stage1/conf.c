/*
 * Copyright (c) 2021 TK Chia
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the developer(s) nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdbool.h>
#include <string.h>
#include "stage1/stage1.h"

static EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *inputx;
static volatile bool slow_step = false;

static EFI_STATUS EFIAPI
key_slow_step (IN EFI_KEY_DATA * key)
{
  EFI_KEY_DATA reap_key;
  slow_step = true;
  inputx->ReadKeyStrokeEx (inputx, &reap_key);
  return EFI_SUCCESS;
}

void
conf_init (void)
{
  EFI_KEY_DATA key1 = { {0, u's'}, {0, 0} }, key2 = { {0, u'S'}, {0, 0} };
  VOID *notify1, *notify2;
  EFI_STATUS status = LibLocateProtocol (&SimpleTextInputExProtocol,
					 (void **) &inputx);
  if (EFI_ERROR (status))
    error_with_status (u"cannot get EFI_SIMPLE_TEXT_INPUT_EX_"
		       "PROTOCOL", status);
  status = inputx->RegisterKeyNotify (inputx, &key1, key_slow_step, &notify1);
  if (EFI_ERROR (status))
    error_with_status (u"lolwut?", status);
  status = inputx->RegisterKeyNotify (inputx, &key2, key_slow_step, &notify2);
  if (EFI_ERROR (status))
    error_with_status (u"lolwut?", status);
  infof (u"%Hpress `S' within 2 seconds to enable slow-stepping mode%N");
  sleepx (2, &slow_step);
  inputx->UnregisterKeyNotify (inputx, notify1);
  inputx->UnregisterKeyNotify (inputx, notify2);
  info (u"\r                                                       \r");
}

void
conf_slow_step_pause (void)
{
  EFI_KEY_DATA key = { {0, 0}, {0, 0} };
  EFI_STATUS status;
  if (slow_step)
    {
      Print (u"%Hpress any key to continue%N");
      do
	{
	  hlt ();
	  status = inputx->ReadKeyStrokeEx (inputx, &key);
	  if (EFI_ERROR (status) && status != EFI_NOT_READY)
	    error_with_status (u"lolwut?", status);
	}
      while (EFI_ERROR (status)
	     || (!key.Key.ScanCode && !key.Key.UnicodeChar));
      Output (u"\r                         \r");
    }
}

void
conf_fini (void)
{
}

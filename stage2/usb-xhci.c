/*
 * Copyright (c) 2022--2023 TK Chia
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

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "stage2/stage2.h"
#include "stage2/pci.h"

#define XHCI_XECP_LEGACY		0x01
#define XHCI_USBLEGSUP_BIOS_OWNED	(1UL << 16)
#define XHCI_USBLEGSUP_OS_OWNED		(1UL << 24)
#define XHCI_USBLEGCTLSTS_SMI_USB	(1UL <<  0)
#define XHCI_USBLEGCTLSTS_SMI_OS_ENA	(1UL << 13)

/* XHCI Host Controller Capability Registers. */
typedef volatile struct __attribute__ ((packed))
{
  uint8_t CAPLENGTH;			/* capability registers length */
  uint8_t : 8;				/* reserved */
  uint16_t HCIVERSION;			/* interface version number */
  /* structural parameters 1, 2, 3 */
  uint32_t HCSPARAMS1, HCSPARAMS2, HCSPARAMS3;
  uint32_t HCCPARAMS1;			/* capability parameters 1 */
  uint32_t DBOFF;			/* doorbell offset */
  uint32_t RTSOFF;			/* runtime register space offset */
  uint32_t HCCPARAMS2;			/* capability parameters 2 */
} usb_xhci_t;

/* XHCI Extended Capability. */
typedef volatile union __attribute__ ((packed))
{
  volatile struct __attribute__ ((packed))
  {
    uint8_t CAPID;			/* capability id. */
    uint8_t NXT;			/* next capability pointer */
  };
  volatile struct __attribute__ ((packed))
  {
    uint32_t USBLEGSUP;
    uint32_t USBLEGCTLSTS;
  } legacy;
} usb_xhci_xec_t;

static void
xhci_start_legacy (uint32_t locn, uint64_t hc_pa, uint32_t hccp1)
{
  uint64_t pa = hc_pa;
  uint16_t off = (uint16_t) (hccp1 >> 16);
  uint8_t cap_id;
  while (off != 0)
    {
      usb_xhci_xec_t *xec;
      pa += (uint64_t) off *4;
      xec = mem_va_map (pa, sizeof (usb_xhci_xec_t), PTE_CD);
      cap_id = xec->CAPID;
      if (cap_id == XHCI_XECP_LEGACY)
	{
	  uint32_t cap1 = xec->legacy.USBLEGSUP,
	    cap2 = xec->legacy.USBLEGCTLSTS,
	    new_cap1 = cap1 | XHCI_USBLEGSUP_OS_OWNED,
	    new_cap2 = cap2 | XHCI_USBLEGCTLSTS_SMI_USB
			    | XHCI_USBLEGCTLSTS_SMI_OS_ENA;
	  if (new_cap1 != cap1 || new_cap2 != cap2)
	    {
	      cprintf ("  USBLEGCTLSTS: 0x%" PRIx32
		       " \x1a 0x%" PRIx32 "\n", cap2, new_cap2);
	      xec->legacy.USBLEGCTLSTS = new_cap2;
	      cprintf ("  USBLEGSUP @ 0x%" PRIx32 "%08" PRIx32 ": "
		       "0x%" PRIx32 " \x1a 0x%" PRIx32 "\n",
		       (uint32_t) (pa >> 32), (uint32_t) pa, cap1, new_cap1);
	      xec->legacy.USBLEGSUP = new_cap1;
	    }
	  mem_va_unmap (xec, sizeof (usb_xhci_xec_t));
	  return;
	}
      off = xec->NXT;
      cprintf ("  cap. @ 0x%" PRIx32 "%08" PRIx32 ": "
	       "(0x%" PRIx16 ") 0x%" PRIx8 "\n",
	       (uint32_t) (pa >> 32), (uint32_t) pa, off, cap_id);
      mem_va_unmap (xec, sizeof (usb_xhci_xec_t));
    }
}

void
usb_xhci_init_bus (bdat_pci_dev_t * pd)
{
  uint32_t locn = pd->pci_locn, hccp1;
  usb_xhci_t *hc;
  uint64_t hc_pa;
  unsigned seg = locn >> 16, bus = (locn >> 8) & 0xff,
	   dev = (locn >> 3) & 0x1f, fn = locn & 7;
  hc = pci_va_map (locn, 0, sizeof (usb_xhci_t), &hc_pa);
  cprintf ("USB XHCI @ %04x:%02x:%02x.%x  BASE: @0x%" PRIx32 "%08" PRIx32 "\n",
	   seg, bus, dev, fn, (uint32_t) (hc_pa >> 32), (uint32_t) hc_pa);
  hccp1 = hc->HCCPARAMS1;
  cprintf ("  CAPLENGTH: 0x%" PRIx8 "  HCIVERSION: 0x%" PRIx16 "\n"
	   "  HCSPARAMS: 0x%" PRIx32 " 0x%" PRIx32 " 0x%" PRIx32 "  "
	   "HCCPARAMS: 0x%" PRIx32 " 0x%" PRIx32 "\n",
	   hc->CAPLENGTH, hc->HCIVERSION,
	   hc->HCSPARAMS1, hc->HCSPARAMS2, hc->HCSPARAMS3,
	   hccp1, hc->HCCPARAMS2);
  xhci_start_legacy (locn, hc_pa, hccp1);
  mem_va_unmap (hc, sizeof (usb_xhci_t));
}

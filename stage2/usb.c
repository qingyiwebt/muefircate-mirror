/*
 * Copyright (c) 2022 TK Chia
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

/* EHCI Host Controller Capability Registers. */
typedef volatile struct __attribute__((packed)) {
	uint8_t CAPLENGTH;		/* capability registers length */
	uint8_t : 8;			/* reserved */
	uint16_t HCIVERSION;		/* interface version number */
	uint32_t HCSPARAMS;		/* structural parameters */
	uint32_t HCCPARAMS;		/* capability parameters */
} usb_ehci_t;

static void ehci_init_bus(bdat_pci_dev_t *pd)
{
	uint32_t locn = pd->pci_locn, hccp;
	usb_ehci_t *hc;
	uint64_t hc_pa;
	unsigned seg = locn >> 16, bus = (locn >> 8) & 0xff,
		 dev = (locn >> 3) & 0x1f, fn = locn & 7;
	hc = pci_va_map(locn, 0, 0x200, &hc_pa);
	cprintf("USB EHCI @ %04x:%02x:%02x.%x  "
		"USBBASE: @0x%" PRIx32 "%08" PRIx32 "\n",
	    seg, bus, dev, fn, (uint32_t)(hc_pa >> 32), (uint32_t)hc_pa);
	hccp = hc->HCCPARAMS;
	cprintf("  CAPLENGTH: 0x%" PRIx8 "  HCIVERSION: 0x%" PRIx16 "  "
		  "HCSPARAMS: 0x%" PRIx32 "  HCCPARAMS: 0x%" PRIx32 "\n",
	    hc->CAPLENGTH, hc->HCIVERSION, hc->HCSPARAMS, hccp);
}

void usb_init(bparm_t *bparms)
{
	bparm_t *bp;
	for (bp = bparms; bp; bp = bp->next) {
		bdat_pci_dev_t *pd;
		if (bp->type != BP_PCID)
			continue;
		pd = &bp->u->pci_dev;
		switch (pd->class_if) {
		    case PCI_CIF_BUS_USB_EHCI:
			ehci_init_bus(pd);
			break;
		    default:
			;
		}
	}
}

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

uint32_t in_pci_d_maybe_unaligned(uint32_t locn, uint8_t off)
{
	uint8_t aoff = off & ~(uint8_t)3;
	switch (off & 3) {
	    default:
		return in_pci_d_aligned(locn, off);
	    case 1:
		return in_pci_d_aligned(locn, aoff) >> 8 |
		       in_pci_d_aligned(locn, aoff + 4) << 24;
	    case 2:
		return in_pci_d_aligned(locn, aoff) >> 16 |
		       in_pci_d_aligned(locn, aoff + 4) << 16;
	    case 3:
		return in_pci_d_aligned(locn, aoff) >> 24 |
		       in_pci_d_aligned(locn, aoff + 4) << 8;
	}
}

void out_pci_d_maybe_unaligned(uint32_t locn, uint8_t off, uint32_t v)
{
	uint8_t aoff = off & ~(uint8_t)3;
	switch (off & 3) {
	    default:
		out_pci_d_aligned(locn, off, v);
	    case 1:
		out_pci_d_aligned(locn, aoff,
		    (in_pci_d_aligned(locn, aoff) & 0x000000ffU) | v << 8);
		out_pci_d_aligned(locn, aoff + 4,
		    (in_pci_d_aligned(locn, aoff + 4) & 0xffffff00U) |
		    v >> 24);
		break;
	    case 2:
		out_pci_d_aligned(locn, aoff,
		    (in_pci_d_aligned(locn, aoff) & 0x0000ffffU) | v << 16);
		out_pci_d_aligned(locn, aoff + 4,
		    (in_pci_d_aligned(locn, aoff + 4) & 0xffff0000U) |
		    v>> 16);
		break;
	    case 3:
		out_pci_d_aligned(locn, aoff,
		    (in_pci_d_aligned(locn, aoff) & 0x00ffffffU) | v << 24);
		out_pci_d_aligned(locn, aoff + 4,
		    (in_pci_d_aligned(locn, aoff + 4) & 0xff000000U) | v >> 8);
	}
}

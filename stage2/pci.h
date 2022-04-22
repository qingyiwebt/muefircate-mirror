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

#ifndef H_STAGE2_PCI
#define H_STAGE2_PCI

#include "stage2/stage2.h"
#include "pci-common.h"

#define PCI_ADDR	0x0cf8
#define PCI_DATA	0x0cfc

/* pci.c functions. */

extern uint32_t in_pci_d_maybe_unaligned(uint32_t, uint8_t);
extern void out_pci_d_maybe_unaligned(uint32_t, uint8_t, uint32_t);
extern void *pci_va_map(uint32_t, uint8_t, size_t, uint64_t *);

/* Read an aligned longword from a PCI device's PCI configuration space. */
static inline uint32_t in_pci_d_aligned(uint32_t locn, uint8_t off)
{
	outpd_w(PCI_ADDR, 1 << 31 | (locn & 0xffffU) << 8 | off);
	return inpd_w(PCI_DATA);
}

/* Read a longword from a PCI device's PCI configuration space. */
static inline uint32_t in_pci_d(uint32_t locn, uint8_t off)
{
	if (__builtin_constant_p(off & 3) && (off & 3) == 0)
		return in_pci_d_aligned(locn, off);
	else
		return in_pci_d_maybe_unaligned(locn, off);
}

/* Write an aligned longword to a PCI device's PCI configuration space. */
static inline void out_pci_d_aligned(uint32_t locn, uint8_t off, uint32_t v)
{
	outpd_w(PCI_ADDR, 1 << 31 | locn << 8 | off);
	outpd_w(PCI_DATA, v);
}

/* Write a longword to a PCI device's PCI configuration space. */
static inline void out_pci_d(uint32_t locn, uint8_t off, uint32_t v)
{
	if (__builtin_constant_p(off & 3) && (off & 3) == 0)
		out_pci_d_aligned(locn, off, v);
	else
		out_pci_d_maybe_unaligned(locn, off, v);
}

#endif

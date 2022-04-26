/*
 * Copyright (c) 2021--2022 TK Chia
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

#ifndef H_APIC
#define H_APIC

#include <inttypes.h>

#define ALIGN_APIC	__attribute__((aligned(0x10)))

typedef volatile struct __attribute__((packed)) {
	uint32_t value ALIGN_APIC;
	uint32_t : 0 ALIGN_APIC;
} wrapped_apic_reg32_t;

/*
 * Local APIC memory-mapped registers.  I capitalize the field names to
 * highlight that these do not refer to ordinary memory.
 */
typedef volatile struct __attribute__((packed)) {
	uint32_t : 32 ALIGN_APIC;	/* 0x0000 */
	uint32_t : 32 ALIGN_APIC;
	uint32_t ID ALIGN_APIC;
	uint32_t VERSION ALIGN_APIC;
	uint32_t : 32 ALIGN_APIC;	/* 0x0040 */
	uint32_t : 32 ALIGN_APIC;
	uint32_t : 32 ALIGN_APIC;
	uint32_t : 32 ALIGN_APIC;
	uint32_t TPR ALIGN_APIC;	/* 0x0080 */
	uint32_t APR ALIGN_APIC;
	uint32_t PPR ALIGN_APIC;
	uint32_t EOI ALIGN_APIC;
	uint32_t RRD ALIGN_APIC;	/* 0x00c0 */
	uint32_t LDR ALIGN_APIC;
	uint32_t DFR ALIGN_APIC;
	uint32_t SVR ALIGN_APIC;
	wrapped_apic_reg32_t ISR[8];	/* 0x0100 */
	wrapped_apic_reg32_t TMR[8];	/* 0x0180 */
	wrapped_apic_reg32_t IRR[8];	/* 0x0200 */
	uint32_t ESR ALIGN_APIC;	/* 0x0280 */
	uint32_t : 32 ALIGN_APIC;
	uint32_t : 32 ALIGN_APIC;
	uint32_t : 32 ALIGN_APIC;
	uint32_t : 32 ALIGN_APIC;	/* 0x02c0 */
	uint32_t : 32 ALIGN_APIC;
	uint32_t : 32 ALIGN_APIC;
	uint32_t LVT_CMCI ALIGN_APIC;
	wrapped_apic_reg32_t ICR[2];	/* 0x0300 */
	uint32_t LVT_TMR ALIGN_APIC;
	uint32_t LVT_THRM ALIGN_APIC;
	uint32_t LVT_PMC ALIGN_APIC;	/* 0x0340 */
	wrapped_apic_reg32_t LVT_LINT[2];
	uint32_t LVT_ERR ALIGN_APIC;
	uint32_t TMR_IC ALIGN_APIC;	/* 0x0380 */
	uint32_t TMR_CC ALIGN_APIC;
	uint32_t : 32 ALIGN_APIC;
	uint32_t : 32 ALIGN_APIC;
	uint32_t : 32 ALIGN_APIC;	/* 0x03c0 */
	uint32_t : 32 ALIGN_APIC;
	uint32_t TMR_DC ALIGN_APIC;
	uint32_t : 32 ALIGN_APIC;
} lapic_t;

/* I/O APIC memory-mapped registers. */
typedef volatile struct __attribute__((packed)) {
	uint32_t IOREGSEL ALIGN_APIC;	/* 0x0000 --- I/O register select */
	uint32_t IOREGWIN ALIGN_APIC;	/* 0x0010 --- I/O window */
} ioapic_t;

/* I/O APIC indirectly-addressed registers --- ioapic_t::IOREGSEL values. */
#define IOAPICID	0x00		/* identification */
#define IOAPICVER	0x01		/* version */
#define IOAPICARB	0x02		/* arbitration */
#define IOREDTBLLO(idx)	(0x10 + 2 * (idx))  /* I/O redirection table */
#define IOREDTBLHI(idx)	(0x10 + 2 * (idx) + 1)

/* Field values for I/O APIC redirection table entries. */
#define IOAPIC_RTLO_MASKED 0x00010000U	/* whether interrupt is masked */

#endif

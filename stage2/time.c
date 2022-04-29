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

#include "stage2/stage2.h"

/* 8253/8254 programmable interval timer (PIT) I/O port numbers. */
#define PIT_DATA0	0x0040
#define PIT_DATA1	0x0041
#define PIT_DATA2	0x0042
#define PIT_CMD		0x0043

/* PIT command bit fields. */
#define PITC_SEL0	0x00		/* select channel 0 */
#define PITC_SEL1	0x40		/* select channel 1 */
#define PITC_SEL2	0x80		/* select channel 2 */
#define PITC_LATCH	0x00		/* counter latch (?) */
#define PITC_LO		0x10		/* low byte only */
#define PITC_HI		0x20		/* high byte only */
#define PITC_LOHI	0x30		/* low byte then high byte */
#define PITC_MODE3	0x06		/* mode 3 (square wave) */
#define PITC_BCD	0x01		/* BCD (vs. binary) mode */

static uint8_t cmos_read(uint8_t idx)
{
	outp_w(PORT_CMOS_IDX, idx);
	return inp_w(PORT_CMOS_DATA);
}

static void cmos_write(uint8_t idx, uint8_t v)
{
	outp_w(PORT_CMOS_IDX, idx);
	outp_w(PORT_CMOS_DATA, v);
}

static void cmos_home(void)
{
	outp_w(PORT_CMOS_IDX, CMOS_RTC_STA_D);
}

void time_init(bparm_t *bparms)
{
	uint16_t retries = 0xffff;
	uint8_t sta;
	/* Program the 8253/8254 PIT for 18.2 Hz operation on IRQ 0. */
	outp_w(PIT_CMD, PITC_SEL0 | PITC_LOHI | PITC_MODE3);
	outp_w(PIT_DATA0, 0x00);
	outp_w(PIT_DATA0, 0x00);
	/*
	 * Program the RTC CMOS to produce periodic interrupts at 1024 Hz,
	 * on IRQ 8.  However, disable periodic interrupts, while enabling
	 * the alarm interrupt.  If the clock is frozen, unfreeze it.
	 */
	do
		sta = cmos_read(CMOS_RTC_STA_A | CMOS_NMI_DIS);
	while ((sta & RTC_A_UIP) != 0 && retries-- != 0);
	cmos_write(CMOS_RTC_STA_A | CMOS_NMI_DIS,
		   (sta & ~RTC_A_RATE_MASK) | RTC_A_RATE_1024HZ);
	sta = cmos_read(CMOS_RTC_STA_B | CMOS_NMI_DIS);
	sta &= ~(RTC_B_FREEZE | RTC_B_UPDE_ENA | RTC_B_TICK_ENA);
	cmos_write(CMOS_RTC_STA_B | CMOS_NMI_DIS, sta | RTC_B_ALRM_ENA);
	/* Clear any IRQs from the RTC that are still not serviced. */
	cmos_read(CMOS_RTC_STA_C | CMOS_NMI_DIS);
	cmos_home();
}

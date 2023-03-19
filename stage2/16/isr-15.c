/*
 * Copyright (c) 2022 TK Chia
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 *   * The above copyright notice and this permission notice shall be
 *     included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
 * OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stddef.h>
#include "stage2/stage2.h"

#define E15_WAIT_ACTIVE	0x83
#define E15_UNSUPP	0x86

/* Helper routine for int 0x15, ax = 0x8300, & int 0x15, ah = 0x86. */
static void
wait_us (isr16_regs_t * regs, uint16_t wait_flag_seg, uint16_t wait_flag_off)
{
  uint32_t interval;
  if (bda.wait_active & BDA_WAIT_ACTIVE)
    {
      regs->ah = E15_WAIT_ACTIVE;
      regs->flags |= EFL_C;
      return;
    }
  interval = (uint32_t) regs->cx << 16 | regs->dx;
  if (interval)
    {
      uint8_t sta;
      bda.p_wait_flag = MK_FP16 (wait_flag_seg, wait_flag_off);
      bda.wait_cntdn = interval - 1;
      bda.wait_cntdn_low = -(uint8_t) 1;
      sta = cmos_read (CMOS_RTC_STA_B | CMOS_NMI_DIS);
      cmos_write (CMOS_RTC_STA_B | CMOS_NMI_DIS, sta | RTC_B_TICK_ENA);
      cmos_home ();
      bda.wait_active = BDA_WAIT_ACTIVE;
    }
  else
    {
      /* No wait actually needed; set the wait-complete flag immediately. */
      pokeb (wait_flag_seg, wait_flag_off,
	     peekb (wait_flag_seg, wait_flag_off) | BDA_WAIT_FIN);
    }
  regs->flags &= ~EFL_C;
}

/* Helper routine for int 0x15, ax = 0x8301. */
static void
cancel_wait (isr16_regs_t * regs)
{
  if ((bda.wait_active & BDA_WAIT_ACTIVE) != 0)
    {
      uint8_t sta = cmos_read (CMOS_RTC_STA_B | CMOS_NMI_DIS);
      cmos_write (CMOS_RTC_STA_B, sta & ~RTC_B_TICK_ENA);
      cmos_home ();
      bda.wait_active = 0;
    }
  regs->flags &= ~EFL_C;
}

void
isr16_0x15_impl (isr16_regs_t * regs)
{
  switch (regs->ah)
    {
    case 0x83:
      switch (regs->al)
	{
	case 0x00:
	  wait_us (regs, regs->es, regs->bx);
	  break;
	case 0x01:
	  cancel_wait (regs);
	  break;
	default:
	  regs->ah = E15_UNSUPP;
	  regs->flags |= EFL_C;
	}
      break;
    case 0x86:
      wait_us (regs, BDA_SEG, offsetof (bda_t, wait_active));
      if ((regs->flags & EFL_C) == 0)
	while (!(bda.wait_active & BDA_WAIT_FIN))
	  yield_to_irq ();
      break;
    default:
      isr16_unimpl (regs->eax, regs->edx, 0x15);
    }
}

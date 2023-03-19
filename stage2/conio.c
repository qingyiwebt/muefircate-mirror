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

#define NANOPRINTF_VISIBILITY_STATIC 1
#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 0
/*
 * Having "large" length modifiers (e.g. %jx) would be nice.  For now
 * though, this means we would need to implement the __udivmoddi4 64-bit
 * integer division routine for our register calling convention, & this
 * seems rather overkill.  -- 20220421
 */
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS 0
#define NANOPRINTF_IMPLEMENTATION 1
#include <string.h>
#include "stage2/stage2.h"
#include "nanoprintf/nanoprintf.h"

struct our_pf_ctx
{
  size_t pos;
  char buf[TB_SZ];
};

static void
outmem_1 (const char *str, size_t n)
{
  extern int outmem16f (/* ... */);
  copy_to_tb (str, n);
  rm16_cs_call ((uint32_t) tb16, n, 0, 0, outmem16f);
}

static void
our_putc_1 (int c, void *pv)
{
  struct our_pf_ctx *pctx = pv;
  if (pctx->pos >= TB_SZ)
    {
      outmem_1 (pctx->buf, TB_SZ);
      pctx->pos = 0;
    }
  pctx->buf[pctx->pos] = c;
  ++pctx->pos;
}

static void
our_putc (int c, void *pv)
{
  if ((char) c == '\n')
    our_putc_1 ('\r', pv);
  our_putc_1 (c, pv);
}

int
vcprintf (const char *fmt, va_list ap)
{
  struct our_pf_ctx ctx = { 0, };
  int res = npf_vpprintf (our_putc, &ctx, fmt, ap);
  if (res >= 0 && ctx.pos)
    outmem_1 (ctx.buf, ctx.pos);
  return res;
}

int
cprintf (const char *fmt, ...)
{
  va_list ap;
  int res;
  va_start (ap, fmt);
  res = vcprintf (fmt, ap);
  va_end (ap);
  return res;
}

int
cputs (const char *str)
{
  return cprintf ("%s", str);
}

int
putch (char ch)
{
  return cprintf ("%c", ch);
}

int
wherex (void)
{
  extern int wherex16f (/* ... */);
  return rm16_cs_call (0, 0, 0, 0, wherex16f);
}

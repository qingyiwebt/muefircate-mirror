/*
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors of
 * this software dedicate any and all copyright interest in the software to
 * the public domain.  We make this dedication for the benefit of the public
 * at large and to the detriment of our heirs and successors.  We intend
 * this dedication to be an overt act of relinquishment in perpetuity of all
 * present and future rights to this software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * @internal
 * @fileoverview Template for routines to support output in graphical video
 * modes for bare metal VGA.
 */

static COLOR
MAPCOLOR (struct cons *cons, cons_std_color_t ic)
{
#if BPP == 16
  if (cons->type == CONS_BGR565)
    return htole16 (ic.bgr.b >> 3 | ic.bgr.g >> 2 << 5 | ic.bgr.r >> 3 << 11);
  else
    return htole16 (ic.bgr.b >> 3 | ic.bgr.g >> 2 << 5 | ic.bgr.r >> 3 << 10);
#else
  if (cons->type == CONS_BGRX8888)
    return ic.w;
  else
    {
      cons_rgbx_color_t oc;
      oc.rgb.r = ic.bgr.r;
      oc.rgb.g = ic.bgr.g;
      oc.rgb.b = ic.bgr.b;
      oc.rgb.x = 0xff;
      return oc.w;
    }
#endif
}

static void
DRAWBITMAP (struct cons *cons, size_t gy, size_t gx, COLOR fg, COLOR bg,
	    const uint8_t *bitmap, size_t bm_ht, size_t bm_wid)
{
  size_t xs = cons->xs;
  char *cplotter = cons->canvas + gy * xs + gx * sizeof (COLOR);
  size_t y_left = bm_ht, x_left;
  while (y_left-- != 0)
    {
      COLOR *plotter = (COLOR *) cplotter;
      uint8_t accum;
      x_left = bm_wid;
      __init1b (&accum);
      while (x_left-- != 0)
	{
	  bool plot_fg = __get1b (&accum, &bitmap);
	  *plotter++ = plot_fg ? fg : bg;
	}
      cplotter += xs;
    }
}

static void
FILLRECT (struct cons *cons, size_t gy, size_t gx,
			     size_t fill_ht, size_t fill_wid, COLOR bg)
{
  size_t xs = cons->xs;
  char *cplotter = cons->canvas + gy * xs + gx * sizeof (COLOR);
  size_t y_left = fill_ht;
  while (y_left-- != 0)
    {
      COLOR *plotter = (COLOR *) cplotter;
      size_t i;
      for (i = 0; i < fill_wid; ++i)
	*plotter++ = bg;
      cplotter += xs;
    }
}

static void
MOVERECT (struct cons *cons, size_t dgy, size_t dgx, size_t sgy, size_t sgx,
	  size_t ht, size_t wid)
{
  size_t xs = cons->xs, xm = wid * sizeof (COLOR);
  char *canvas = cons->canvas;
  if (dgy < sgy)
    {
      char *dest = canvas + dgy * xs + dgx * sizeof (COLOR);
      const char *src = canvas + sgy * xs + sgx * sizeof (COLOR);
      while (ht-- != 0)
	{
	  memmove (dest, src, xm);
	  dest += xs;
	  src  += xs;
	}
    }
  else if (dgy > sgy)
    {
      char *dest = canvas + (dgy + ht) * xs + dgx * sizeof (COLOR);
      const char *src = canvas + (sgy + ht) * xs + sgx * sizeof (COLOR);
      while (ht-- != 0)
	{
	  dest -= xs;
	  src  -= xs;
	  memmove (dest, src, xm);
	}
    }
}

void
DRAWCHAR (struct cons *cons, size_t y, size_t x, wchar_t wc)
{
  const uint8_t *glyph;
  const size_t glyph_ht = CONS_ASSUME_CHAR_HEIGHT_PX,
	       glyph_wid = CONS_ASSUME_CHAR_WIDTH_PX;
  COLOR fg = MAPCOLOR (cons, cons->fg), bg = MAPCOLOR (cons, cons->bg);
  if (wc < L' ' || wc >= L' ' + __ARRAYLEN (__cons_font_default_direct))
    glyph = __cons_font_default_direct[0];
  else
    glyph = __cons_font_default_direct[wc - L' '];
  DRAWBITMAP (cons, y * cons->yc, x * cons->xc, fg, bg, glyph,
	      glyph_ht, glyph_wid);
}

void
ERASELINECELLS (struct cons *cons, size_t y, size_t x, size_t n)
{
  size_t yc = cons->yc, xc = cons->xc;
  FILLRECT (cons, y * yc, x * xc, yc, n * xc, MAPCOLOR (cons, cons->bg));
}

void
MOVELINECELLS (struct cons *cons, size_t dty, size_t dtx,
				  size_t sty, size_t stx, size_t n)
{
  size_t yc = cons->yc, xc = cons->xc;
  MOVERECT (cons, dty * yc, dtx * xc, sty * yc, stx * xc, yc, n * xc);
}

#undef COLOR
#undef BPP
#undef MAPCOLOR
#undef DRAWBITMAP
#undef FILLRECT
#undef MOVERECT
#undef DRAWCHAR
#undef ERASELINECELLS
#undef MOVELINECELLS

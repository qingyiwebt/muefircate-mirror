/*
 * Portions copyright (c) 2023 TK Chia
 * Copyright (c) 2021--2023 Justine Alexandra Roberts Tunney
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <machine/endian.h>
#include "cons.h"
#include "pc.h"
#include "stage1.h"

struct cons __console;

static bool
__cons_iscntrl (unsigned char c)
{
  return c <= 0x1f || (c >= 0x80 && c<= 0x9f);
}

static bool
__cons_isascii (unsigned char c)
{
  return c <= 0x7f;
}

static void
__cons_set_type (struct cons *cons, enum cons_typ type)
{
  cons->type = type;
  switch (type)
    {
    default:
      cons->draw_char = __cons_klog_32_draw_char;
      cons->erase_line_cells = __cons_klog_32_erase_line_cells;
      cons->move_line_cells = __cons_klog_32_move_line_cells;
      break;
    case CONS_BGR565:
    case CONS_BGR555:
      cons->draw_char = __cons_klog_16_draw_char;
      cons->erase_line_cells = __cons_klog_16_erase_line_cells;
      cons->move_line_cells = __cons_klog_16_move_line_cells;
    }
}

static void
__early_init_uefi_cons (struct cons *cons, const struct boot_reserve *rs)
{
  const struct boot_video *vid = __early_map_memory (rs->begin,
						     rs->end - rs->begin);
  unsigned short yp = vid->info.vertical_resolution;
  unsigned short xp = vid->info.horizontal_resolution;
  size_t cpp = sizeof (cons_bgrx_color_t);  /* `char's per pixel */
  unsigned short xs = vid->info.pixels_per_scan_line;  /* to be scaled */
  enum cons_typ type = CONS_BGRX8888;  /* assume BGRX if format unknown */
  char *fb;
  switch (vid->info.pixel_format)
    {
    default:
      break;
    case EFI_PIXEL_RED_GREEN_BLUE_RESERVED_8_BIT_PER_COLOR:
      type = CONS_RGBX8888;
      cpp = sizeof (cons_rgbx_color_t);
      break;
    case EFI_PIXEL_BIT_MASK:
      {
	const struct efi_pixel_bitmask *mask = &vid->info.pixel_information;
	switch (le32toh (mask->red_mask))
	  {
	  case 0x000000ff:
	    type = CONS_RGBX8888;
	    cpp = sizeof (cons_rgbx_color_t);
	    break;
	  case 0x0000f800:
	    type = CONS_BGR565;
	    cpp = sizeof (cons_bgr565_color_t);
	    break;
	  case 0x00007c00:
	    type = CONS_BGR555;
	    cpp = sizeof (cons_bgr555_color_t);
	    break;
	  default:
	    ;
	  }
      }
    }
  xs *= cpp;
  cons->yn = yp / CONS_ASSUME_CHAR_HEIGHT_PX;
  cons->xn = xp / CONS_ASSUME_CHAR_WIDTH_PX;
  cons->yp = yp;
  cons->xp = xp;
  cons->xs = cons->xsfb = xs;
  __cons_set_type (cons, type);
  fb = __early_map_memory (vid->frame_buffer_base, yp * xs);
  cons->fb = cons->canvas = fb;
}

static void
__early_init_dummy_cons (struct cons *cons)
{
  static char dummy_fb[CONS_ASSUME_CHAR_HEIGHT_PX * CONS_ASSUME_CHAR_WIDTH_PX
		       * sizeof (cons_bgrx_color_t)];
  cons->yn = cons->xn = 1;
  cons->yp = CONS_ASSUME_CHAR_HEIGHT_PX;
  cons->xp = cons->xs = cons->xsfb = CONS_ASSUME_CHAR_WIDTH_PX;
  __cons_set_type (cons, CONS_BGRX8888);
  cons->fb = cons->canvas = dummy_fb;
}

static void
__cons_reset_output_mode (struct cons *cons)
{
  cons->state = TTY_NORM;
  cons->fg = CONS_DEFAULT_FG;
  cons->bg = CONS_DEFAULT_BG;
}

static void
__cons_full_reset (struct cons *cons)
{
  __cons_reset_output_mode (cons);
  cons->y = cons->x = 0;
  memset (cons->fb, 0, cons->yp * cons->xs);
}

static void
__early_init_cons_1 (struct cons *cons, const struct stage1 *stage1)
{
  const struct boot_reserve *rs;
  size_t nr;
  memset (cons, 0, sizeof (*cons));
  rs = stage1->reserve;
  nr = stage1->reserves;
  while (strcmp (rs->name, "video") != 0)
    {
      ++rs;
      --nr;
    }
  if (nr)
    __early_init_uefi_cons (cons, rs);
  else
    __early_init_dummy_cons (cons);
  cons->yc = CONS_ASSUME_CHAR_HEIGHT_PX;
  cons->xc = CONS_ASSUME_CHAR_WIDTH_PX;
  __cons_full_reset (cons);
  __cons_write (cons, "hello world\n", 12);
}

static void
__cons_erase_line_cells (struct cons *cons, size_t y, size_t x, size_t n)
{
  cons->erase_line_cells (cons, y, x, n);
}

static void
__cons_erase_lines (struct cons *cons, size_t y, size_t n)
{
  size_t xn = cons->xn;
  while (n-- != 0)
    {
      __cons_erase_line_cells (cons, y, 0, xn);
      ++y;
    }
}

static void
__cons_move_line_cells (struct cons *cons, size_t dst_y, size_t dst_x,
					   size_t src_y, size_t src_x,
					   size_t n)
{
  cons->move_line_cells (cons, dst_y, dst_x, src_y, src_x, n);
}

static void
__cons_move_lines (struct cons *cons, size_t dst_y, size_t src_y, size_t n)
{
  size_t xn = cons->xn;
  if (dst_y < src_y)
    {
      while (n-- != 0)
	{
	  __cons_move_line_cells (cons, dst_y, 0, src_y, 0, xn);
	  ++dst_y;
	  ++src_y;
	}
    }
  else if (dst_y > src_y)
    {
      dst_y += n;
      src_y += n;
      while (n-- != 0)
	{
	  --dst_y;
	  --src_y;
	  __cons_move_line_cells (cons, dst_y, 0, src_y, 0, xn);
	}
    }
}

static void
__cons_scroll (struct cons *cons, size_t n)
{
  __cons_move_lines (cons, n, 0, cons->yn - n);
  __cons_erase_lines (cons, cons->yn - n, n);
}

static void
__cons_index (struct cons *cons)
{
  size_t y = cons->y;
  if (y < cons->yn - 1)
    cons->y = y + 1;
  else
    __cons_scroll (cons, 1);
}

static void
__cons_advance (struct cons *cons)
{
  cons->red_zone = false;
  cons->x = 0;
  __cons_index (cons);
}

static void
__cons_newline (struct cons *cons)
{
  __cons_advance (cons);
}

static void
__cons_cntrl (struct cons *cons, unsigned char c)
{
  switch (c)
    {
    case '\n':
      __cons_newline (cons);
      break;
    case 0x1b:
      cons->state = TTY_ESC;
      break;
    }
}

static void
__cons_write_glyph (struct cons *cons, wchar_t wc, unsigned width)
{
  unsigned short x;
  if (! width)
    return;
  if (cons->red_zone || cons->x + width > cons->xn)
    __cons_advance (cons);
  cons->draw_char (cons, cons->y, cons->x, wc);
  x = cons->x += width;
  if (x >= cons->xn)
    {
      cons->x = cons->xn - 1;
      cons->red_zone = true;
    }
}

static void
__cons_write_bad_glyph (struct cons *cons)
{
  __cons_write_glyph (cons, UNICODE_BAD, 1);
}

static void
__cons_putch_norm (struct cons *cons, unsigned char c)
{
  switch (c)
    {
    default:
      if (__cons_iscntrl (c))
	__cons_cntrl (cons, c);
      else if (__cons_isascii (c))
	__cons_write_glyph (cons, (wchar_t) c, 1);
      else
	__cons_write_bad_glyph (cons);
      break;
    case 0xc2:  case 0xc3:
    case 0xc4:  case 0xc5:
    case 0xc6:  case 0xc7:
    case 0xc8:  case 0xc9:
    case 0xca:  case 0xcb:
    case 0xcc:  case 0xcd:
    case 0xce:  case 0xcf:
    case 0xd0:  case 0xd1:
    case 0xd2:  case 0xd3:
    case 0xd4:  case 0xd5:
    case 0xd6:  case 0xd7:
    case 0xd8:  case 0xd9:
    case 0xda:  case 0xdb:
    case 0xdc:  case 0xdd:
    case 0xde:  case 0xdf:
      cons->state = TTY_UTF8_1;
      cons->u8 = c & 0x1f;
      cons->e8 = L'\x80';
      break;
    case 0xe0:  case 0xe1:
    case 0xe2:  case 0xe3:
    case 0xe4:  case 0xe5:
    case 0xe6:  case 0xe7:
    case 0xe8:  case 0xe9:
    case 0xea:  case 0xeb:
    case 0xec:  case 0xed:
    case 0xee:  case 0xef:
      cons->state = TTY_UTF8_2;
      cons->u8 = c & 0x0f;
      cons->e8 = L'\x800';
      break;
    case 0xf0:  case 0xf1:
    case 0xf2:  case 0xf3:
    case 0xf4:  case 0xf5:
    case 0xf6:  case 0xf7:
      cons->state = TTY_UTF8_3;
      cons->u8 = c & 0x07;
      cons->e8 = L'\x10000';
    }
}

static void
__cons_putch_reparse (struct cons *cons, unsigned char c)
{
  __cons_write_bad_glyph (cons);
  cons->state = TTY_NORM;
  __cons_putch_norm (cons, c);
}

static void
__cons_putch_utf8_1 (struct cons *cons, unsigned char c)
{
  if (c >= 0x80 && c <= 0xbf)
    {
      wchar_t u8 = cons->u8, e8 = cons->e8;
      u8 = (u8 << 6) | (c & 0x3f);
      if (u8 < e8)
	u8 = UNICODE_BAD;
      else if (u8 >= 0xd800 && u8 <= 0xdfff)
	u8 = UNICODE_BAD;
      __cons_write_glyph (cons, u8, 1);
      cons->state = TTY_NORM;
    }
  else
    __cons_putch_reparse (cons, c);
}

static void
__cons_putch_utf8_mid (struct cons *cons, unsigned char c)
{
  if (c >= 0x80 && c <= 0xbf)
    {
      cons->u8 = (cons->u8 << 6) | (c & 0x3f);
      --cons->state;
    }
  else
    __cons_putch_reparse (cons, c);
}

static void
__cons_putch_esc (struct cons *cons, unsigned char c)
{
  cons->state = TTY_NORM;
  switch (c)
    {
    case 'c':
      __cons_full_reset (cons);
      break;
    default:
      ;
    }
}

void
__early_init_cons (const struct stage1 *stage1)
{
  __early_init_cons_1 (&__console, stage1);
}

void
__cons_write (struct cons *cons, const void *data, size_t n)
{
  const unsigned char *p = data;
  while (n-- != 0)
    {
      unsigned char c = *p++;
      switch (cons->state)
	{
	default:
	  __cons_putch_norm (cons, c);
	  break;
	case TTY_UTF8_1:
	  __cons_putch_utf8_1 (cons, c);
	  break;
	case TTY_UTF8_2:
	case TTY_UTF8_3:
	  __cons_putch_utf8_mid (cons, c);
	  break;
	case TTY_ESC:
	  __cons_putch_esc (cons, c);
	}
    }
}

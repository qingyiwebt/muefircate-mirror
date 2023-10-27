/*
 * Copyright (c) 2023 TK Chia
 * Copyright (c) 2021--2023 Justine Alexandra Roberts Tunney
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <limits.h>
#include <string.h>
#include <machine/endian.h>
#include "cons.h"
#include "pc.h"
#include "stage1.h"

/**
 * @internal
 * Assumed height of each character in pixels.
 */
#define CONS_ASSUME_CHAR_HEIGHT_PX \
	sizeof (_cons_font_default_direct[0])
/**
 * @internal
 * Assumed width of each character in pixels.
 */
#define CONS_ASSUME_CHAR_WIDTH_PX \
	(sizeof (_cons_font_default_direct[0][0]) * CHAR_BIT)

struct cons _console;

static void
_preinit_uefi_cons (const struct boot_reserve *rs)
{
  const struct boot_video *vid = _early_map_memory (rs->begin,
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
  _console.yn = yp / CONS_ASSUME_CHAR_HEIGHT_PX;
  _console.xn = xp / CONS_ASSUME_CHAR_WIDTH_PX;
  _console.yp = yp;
  _console.xp = xp;
  _console.xs = _console.xsfb = xs;
  _console.type = type;
  fb = _early_map_memory (vid->frame_buffer_base, yp * xs);
  _console.fb = _console.canvas = fb;
  memset (fb, 0, yp * xs);
}

static void
_preinit_dummy_cons (void)
{
  static char dummy_fb[CONS_ASSUME_CHAR_HEIGHT_PX * CONS_ASSUME_CHAR_WIDTH_PX
		       * sizeof (cons_bgrx_color_t)];
  _console.yn = _console.xn = 1;
  _console.yp = CONS_ASSUME_CHAR_HEIGHT_PX;
  _console.xp = _console.xs = _console.xsfb = CONS_ASSUME_CHAR_WIDTH_PX;
  _console.type = CONS_BGRX8888;
  _console.fb = _console.canvas = dummy_fb;
}

void
_preinit_cons (const struct stage1 *stage1)
{
  const struct boot_reserve *rs = stage1->reserve;
  size_t nr = stage1->reserves;
  while (strcmp (rs->name, "video") != 0)
    {
      ++rs;
      --nr;
    }
  if (nr)
    _preinit_uefi_cons (rs);
  else
    _preinit_dummy_cons ();
  _console.yc = CONS_ASSUME_CHAR_HEIGHT_PX;
  _console.xc = CONS_ASSUME_CHAR_WIDTH_PX;
  _console.y = _console.x = 0;
}

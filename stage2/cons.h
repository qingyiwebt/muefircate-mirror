/*
 * Copyright (c) 2023 TK Chia
 * Copyright (c) 2022--2023 Justine Alexandra Roberts Tunney
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

#ifndef _H_STAGE2_CONS
#define _H_STAGE2_CONS

#include <stdint.h>

enum cons_typ
{
  CONS_BGR565,
  CONS_BGR555,
  CONS_BGRX8888,
  CONS_RGBX8888
};

struct cons
{
  /**
   * Cursor position.  (y, x) = (0, 0) means the cursor is on the top left
   * character cell of the terminal.
   */
  unsigned short y, x;
  /** Height and width of terminal, in character units. */
  unsigned short yn, xn;
  /** Height and width of terminal, in pixels. */
  unsigned short yp, xp;
  /**
   * Number of bytes (NOTE) occupied by each row of pixels, including any
   * invisible "pixels", in the video frame buffer.
   */
  unsigned short xsfb;
  /**
   * Number of bytes (NOTE) occupied by each row of pixels, including any
   * invisible "pixels", in the canvas.
   */
  unsigned short xs;
  /** Type of video buffer. */
  enum cons_typ type;
  /**
   * Height and width of each character in pixels.
   */
  uint8_t yc, xc;
  /** Actual video frame buffer as provided by the video card. */
  char *fb;
  /**
   * Canvas to draw into.  This should be separate from the frame buffer, &
   * possibly allocated from main memory; we must arrange to update the
   * frame buffer from the canvas every now & then.
   *
   * During normal operation, the canvas's pixel format is given by
   * _cons_(), & may be different from the frame buffer's.
   */
  char *canvas;
};

typedef union
  {
    uint32_t w;
    struct
      {
	uint8_t b, g, r, x;
      } bgr;
  } cons_bgrx_color_t;

typedef union
  {
    uint32_t w;
    struct
      {
	uint8_t r, g, b, x;
      } rgb;
  } cons_rgbx_color_t;

typedef union
  {
    uint16_t w;
  } cons_bgr565_color_t;

typedef union
  {
    uint16_t w;
  } cons_bgr555_color_t;

extern const uint8_t _cons_font_default_direct[95][13];
extern struct cons _console;

#endif

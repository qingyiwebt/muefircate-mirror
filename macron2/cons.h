/*
 * Portions copyright (c) 2023 TK Chia
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

#ifndef _H_MACRON2_CONS
#define _H_MACRON2_CONS

#include <limits.h>
#include <stdint.h>
#include <wchar.h>

enum cons_typ
{
  CONS_BGR565,
  CONS_BGR555,
  CONS_BGRX8888,
  CONS_RGBX8888
};

/**
 * @internal
 * Possible states for teletype output processing.  NOTE: TTY_UTF8_{1, 2, 3}
 * should be contiguous and in order.
 */
enum cons_tty_state
{
  TTY_NORM,
  TTY_UTF8_1,
  TTY_UTF8_2,
  TTY_UTF8_3,
  TTY_ESC,
  TTY_CSI,
};

enum
{
  UNICODE_BAD = L'\xfffd'
};

typedef union
  {
    uint32_t w;
    struct
      {
	uint8_t b, g, r, x;
      } bgr;
  } cons_bgrx_color_t;
typedef cons_bgrx_color_t cons_std_color_t;

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
  /** Current state of teletype output processing. */
  enum cons_tty_state state;
  /** Intermediate code value of the Unicode character being output. */
  wchar_t u8;
  /** Minimum expected code number for the current unicode character. */
  wchar_t e8;
  /** Current foreground and background colors. */
  cons_std_color_t fg, bg;
  /** Height and width of each character in pixels. */
  uint8_t yc, xc;
  /**
   * Whether we are "off" the end of the screen & should scroll the screen
   * before printing another character.
   */
  unsigned red_zone : 1;
  /** Actual video frame buffer as provided by the video card. */
  char *fb;
  /**
   * Canvas to draw into.
   *
   * During normal operation, the canvas should be separate from the frame
   * buffer, & possibly allocated from main memory; we must arrange to
   * update the frame buffer from the canvas every now & then.  Also, the
   * canvas's pixel format is given by _cons_canvas_type (.), & may be
   * different from the frame buffer's.
   */
  char *canvas;
  /** Pointers to actual implementations of character drawing operations. */
  void (*draw_char) (struct cons *, size_t, size_t, wchar_t);
  void (*erase_line_cells) (struct cons *, size_t, size_t, size_t);
  void (*move_line_cells) (struct cons *, size_t, size_t, size_t, size_t,
					  size_t);
};

extern const uint8_t __cons_font_default_direct[95][13];
extern struct cons __console;

#define __ARRAYLEN(__a) \
	(sizeof (__a) / sizeof ((__a)[0]) \
	 / ! (sizeof (__a) % sizeof ((__a)[0])))

/**
 * @internal
 * Assumed height of each character in pixels.
 */
enum
{
  CONS_ASSUME_CHAR_HEIGHT_PX = __ARRAYLEN (__cons_font_default_direct[0])
};
/**
 * @internal
 * Assumed width of each character in pixels.
 */
enum
{
  CONS_ASSUME_CHAR_WIDTH_PX
    = sizeof (__cons_font_default_direct[0][0]) * CHAR_BIT
};
/**
 * @internal
 * Default foreground color.
 */
#define CONS_DEFAULT_FG \
	((cons_std_color_t) { .bgr.r = 0xaa, .bgr.g = 0xaa, .bgr.b = 0xaa, \
			      .bgr.x = 0xff })
/**
 * @internal
 * Default background color.
 */
#define CONS_DEFAULT_BG \
	((cons_std_color_t) { .bgr.r = 0x00, .bgr.g = 0x00, .bgr.b = 0x00, \
			      .bgr.x = 0xff })

static inline void
__init1b (uint8_t *__accum)
{
  *__accum = 0x80;
}

static inline bool
__get1b (uint8_t *__accum, const uint8_t **__stream)
{
  bool __c;
#if defined __amd64__ && defined __GNUC__ && defined __OPTIMIZE__
  __asm ("shlb %1; "
	 "jnz 0f; "
	 "movb (%2), %1; "
	 "rclb %1; "
	 "incq %2; "
	 "0:"
	 : "=@ccc" (__c), "+&r" (*__accum), "+r" (*__stream));
#else
  uint8_t __a = *__accum;
  if (__a == 0x80)
    {
      __a = **__stream;
      ++*__stream;
      __c = ((__a & 0x80) != 0);
      __a = __a << 1 | 1;
    }
  else
    {
      __c = ((__a & 0x80) != 0);
      __a <<= 1;
    }
  *__accum = __a;
#endif
  return __c;
}

extern void __cons_write (struct cons *, const void *, size_t);
extern void __cons_klog_16_draw_char (struct cons *, size_t, size_t, wchar_t);
extern void __cons_klog_16_erase_line_cells (struct cons *, size_t, size_t,
							    size_t);
extern void __cons_klog_16_move_line_cells (struct cons *, size_t, size_t,
					    size_t, size_t, size_t);
extern void __cons_klog_32_draw_char (struct cons *, size_t, size_t, wchar_t);
extern void __cons_klog_32_erase_line_cells (struct cons *, size_t, size_t,
							    size_t);
extern void __cons_klog_32_move_line_cells (struct cons *, size_t, size_t,
					    size_t, size_t, size_t);

#endif

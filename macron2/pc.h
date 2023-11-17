/*
 * Copyright (c) 2023 TK Chia
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef _H_MACRON2_PC
#define _H_MACRON2_PC

#define CR0_WP		(1 << 16)
#define CR4_VA57	(1 << 12)

#define BANE		0xffff800000000000

#ifndef __ASSEMBLER__
# include <stddef.h>
# include <stdint.h>
static inline void *
__early_map_memory (uintptr_t __where, size_t __length)
{
  return (void *) (BANE + __where);
}
#endif  /* ! __ASSEMBLER__ */

#endif

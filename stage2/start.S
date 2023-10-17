/*
 * Copyright (c) 2023 TK Chia
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "pc.h"

	.text

	.globl	_start
_start:
	/*
	 * Upon entry we have
	 *   * %rdi = list of reserved memory blocks
	 *   * %rsi = no. of reserved memory blocks
	 *   * %rdx = UEFI memory map
	 *   * %rcx = size of UEFI memory map
	 *   * %r8 = size of each descriptor in UEFI memory map.
	 * Also %cr3 points to page tables that implement identity mapping.
	 *
	 * Copy the pre-existing identity mapping from [0, 0x7fff'ffff'ffff]
	 * to [0xffff'8000'0000'0000, 0xffff'ffff'ffff'ffff]; then transfer
	 * control there.  The page tables might be marked read-only by the
	 * UEFI firmware, so we may have to temporarily unset %cr0.WP to
	 * modify them.
	 *
	 * (If 5-level paging is active we might also clobber the mapping at
	 * [0xffff'0000'0000'0000, 0xffff'7fff'ffff'ffff].  But that is OK.)
	 */
	cli
	mov	%cr0, %rax		/* unset %cr0.WP */
	and	$~CR0_WP, %rax
	mov	%rax, %cr0
	mov	%cr3, %rbp		/* get %cr3 */
	mov	%rbp, %rbx
	mov	%cr4, %rax		/* test for VA57 */
	and	$-0x1000, %bx
	test	$CR4_VA57 >> 8, %ah
	jz	.pml4
	mov	(%rbx), %rax		/* if VA57, copy one PML4T pointer */
	mov	%rax, [0x1000-8](%rbx)
	and	$-0x1000, %ax
	xchg	%rax, %rbx
	jmp	.pml4
.pml4:
	mov	(%rbx), %rax		/* copy PML4E entries from low to */
	mov	%rax, 0x800(%rbx)	/* high virtual memory */
	add	$8, %rbx
	test	$(0x800 - 1), %bx
	jnz	.pml4
	mov	%rbp, %cr3		/* reload %cr3 to flush TLBs */
	mov	%cr0, %rax		/* set %cr0.WP (again) */
	or	$CR0_WP, %rax
	mov	%rax, %cr0
	lea	.cont(%rip), %rax	/* jump to high virtual memory */
	movabs	$BANE, %rcx
	add	%rcx, %rax
	jmp	*%rax
.cont:
	jmp	.

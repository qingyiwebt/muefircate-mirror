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
	 * First push the input parameters onto the stack so we can free up
	 * some registers.  Adjust some of the input parameters to point to
	 * high virtual memory [0xffff'8000'0000'0000, 0xffff'ffff'ffff'ffff].
	 * Maintain a pointer to the pushed parameters.
	 */
	push	%r8
	push	%rcx
	mov	$BANE, %rcx
	add	%rcx, %rdx
	add	%rcx, %rdi
	push	%rdx
	push	%rsi
	push	%rdi
	sub	%rcx, %rdi
.reserve:
	add	%rcx, (%rdi)		/* remember to adjust the pointers */
	add	$0x18, %rdi		/* in the reserved list */
	dec	%rsi
	jnz	.reserve
	lea	(%rsp, %rcx), %r12	/* point %r12 to the stage1 stuff */
	and	$-0x10, %sp		/* realign %rsp */
	/*
	 * Copy the pre-existing identity mappings from [0, 0x7fff'ffff'ffff]
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
	jz	.pml4.1
	mov	(%rbx), %rax		/* if VA57, copy one PML4T pointer */
	mov	%rax, [0x1000-8](%rbx)
	and	$-0x1000, %ax
	xchg	%rax, %rbx
.pml4.1:
	mov	(%rbx), %rax		/* copy PML4E entries from low to */
	mov	%rax, 0x800(%rbx)	/* high virtual memory */
	add	$8, %rbx
	test	$(0x800 - 1), %bx
	jnz	.pml4.1
	mov	%rbp, %cr3		/* reload %cr3 to flush TLBs */
	lea	.cont(%rip), %rax	/* jump to high virtual memory */
	add	%rcx, %rax
	jmp	*%rax
.cont:
	/*
	 * We are now running under an "inverted" identity page mapping.
	 * Adjust the stack pointer, & remove the original identity mappings.
	 */
	add	%rcx, %rsp
	add	%rcx, %rbx
	xor	%eax, %eax
.pml4.2:
	sub	$8, %rbx
	mov	%rax, (%rbx)
	test	$(0x800 - 1), %bx
	jnz	.pml4.2
	mov	%rbp, %cr3		/* flush TLBs again */
	/*
	 * Run early initialization routines to set up the final virtual
	 * memory mapping.
	 */
	mov	%r12, %rdi
	call	__early_init_cons
	jmp	.

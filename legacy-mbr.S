/*
 * Copyright (c) 2021--2023 TK Chia
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Dummy legacy master boot record code which directs the user to restart the
 * system in UEFI mode.
 */

	.code16

	.globl	_start
_start:
	ljmp	$0, $.cont		/* set up segment registers */
.cont:
	xor	%ax, %ax
	cli
	mov	%ax, %ss
	mov	$_start, %sp
	sti
	mov	%ax, %ds
	mov	$msg, %si		/* display the error message */
	mov	$(msg.end - msg), %di
.loop:
	lodsb
	mov	$0x0e, %ah
	mov	$0x0007, %bx
	int	$0x10
	dec	%di
	jnz	.loop
	xor	%ah, %ah		/* wait for a keystroke */
	int	$0x16
	push	%sp			/* test if we are on a 286 or above */
	pop	%ax
	cmp	%sp, %ax
	cli
	jnz	.no_286
	lidtw	bad_idtr		/* if we are, use a triple fault to */
	int3				/* reset the PC */
.no_286:
	ljmpw	$0xf000, $0xfff0	/* otherwise, jump to the reset code */

	.balign	2

bad_idtr:
	.short	0

msg:	.byte	13, 10
	.ascii	"System started in legacy BIOS mode\r\n"
	.ascii	"Please restart in UEFI mode\r\n"
	.byte	7
msg.end:

	.org	_start + 0x200 - 2

	.byte	0x55, 0xaa

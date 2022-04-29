; Copyright (c) 2021 TK Chia
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions are
; met:
;
;   * Redistributions of source code must retain the above copyright
;     notice, this list of conditions and the following disclaimer.
;   * Redistributions in binary form must reproduce the above copyright
;     notice, this list of conditions and the following disclaimer in the
;     documentation and/or other materials provided with the distribution.
;   * Neither the name of the developer(s) nor the names of its
;     contributors may be used to endorse or promote products derived from
;     this software without specific prior written permission.
;
; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
; IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
; TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
; PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
; HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
; SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
; TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
; PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
; LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
; NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
; SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

%include "stage2/stage2.inc"

%define	KIBYTE		1024
%define PARA_SIZE	0x10
%define BMEM_MAX_ADDR	0x100000

	section	.text

	extern	mem_alloc, _stext16, _etext16, _sdata16, _end16
	extern	gdt_desc_cs16, gdt_desc_ds16
	extern	rm16_call.cont1, rm16_call.rm_cs16
	extern	vecs16_part1, NUM_VECS16_PART1
	extern	vecs16_part2, NUM_VECS16_PART2
	extern	tb16

	global	rm16_init
rm16_init:
	push	esi
	push	edi
	mov	eax, _etext16		; allocate base memory for the real-
	mov	edx, PARA_SIZE		; -mode code
	mov	ecx, BMEM_MAX_ADDR
	call	mem_alloc
	or	[gdt_desc_cs16+2], eax	; fix up the GDT entry for SEL_CS16
	mov	esi, text16_load	; copy out the 16-bit code
	lea	edi, [eax+_stext16]
	mov	ecx, (text16_load.end-text16_load)/4
	rep movsd
	mov	ecx, eax		; save real mode code seg. no.; also
	shr	ecx, 4			; patch seg. no. in copied code
	mov	[rm16_cs], cx
	mov	[eax+rm16_call.rm_cs16], cx
	push	ecx			; (1) --- see below
	mov	eax, _end16		; allocate base memory for the real-
	mov	edx, KIBYTE		; -mode data
	mov	ecx, BMEM_MAX_ADDR
	call	mem_alloc
	or	[gdt_desc_ds16+2], eax	; fix up the GDT entry for SEL_DS16
	mov	edx, eax		; initialize the EBDA pointer
	shr	edx, 4
	mov	[bda.ebda], dx
	mov	esi, data16_load	; copy out the 16-bit initialized data
	lea	edi, [eax+_sdata16]
	mov	ecx, (data16_load.end-data16_load)/4
	rep movsd
	lea	esi, [eax+vecs16_part1]	; (2) --- see below
	lea	ecx, [eax+_end16+3]	; blank out the uninitialized data
	sub	ecx, edi
	shr	ecx, 2
	xor	eax, eax
	rep stosd
	xchg	edi, eax		; initialize real-mode intr. vectors
	mov	cl, NUM_VECS16_PART1	; before calling option ROMs
	pop	eax			; (1) --- see above
	shl	eax, 16
.vecs_1:
	lodsw				; (2) --- see above
	stosd
	loop	.vecs_1
	mov	di, 0x70*4
	mov	cl, NUM_VECS16_PART2
.vecs_2:
	lodsw
	stosd
	loop	.vecs_2
	mov	ax, bda.def_kb_buf-bda	; initialize IRQ 1 keyboard buffer
	mov	[bda.kb_buf_start], ax
	mov	[bda.kb_buf_head], ax
	mov	[bda.kb_buf_tail], ax
	mov	al, bda.def_kb_buf_end-bda
	mov	[bda.kb_buf_end], ax
	mov	ax, fs			; reload fs desc. cache (for SEL_DS16)
	mov	fs, ax
	pop	edi
	pop	esi
	ret

	align	16
	global	rm16_call
rm16_call:
	push	ebx
	push	esi
	push	edi
	push	ebp
	pushfd
	mov	ebx, [esp+5*4+4]
	lea	edi, [esp+5*4+4+4]
	cli
	call	SEL_CS16:rm16_call.cont1
	mov	si, SEL_DS32
	mov	ds, si
	mov	es, si
	mov	ss, si
	mov	gs, si
	movzx	esi, word [bda.ebda]	; properly update SEL_DS16 descriptor
	shl	esi, 4			; in case EBDA has moved
	or	esi, 0x92000000
	mov	[gdt_desc_ds16+2], esi
	mov	si, SEL_DS16
	mov	fs, si
	popfd
	pop	ebp
	pop	edi
	pop	esi
	pop	ebx
	ret	8

	align	4
data16_load:
	incbin	"stage2/data16.bin"
	align	4, db 0
.end:
text16_load:
	incbin	"stage2/text16.bin"
	align	4
.end:

	section	.bss

	resb	0x1000
starting_stack:

	common	rm16_cs	2

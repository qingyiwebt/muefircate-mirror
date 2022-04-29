; Copyright (c) 2021--2022 TK Chia
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

;	"Time is an illusion, lunchtime doubly so."  -- Douglas Adams

%include "stage2/stage2.inc"

	bits	16

	section	.text

; IRQ 0 (system timer) handler.
	global	irq0
irq0:
	push	ds
	push	eax
	xor	ax, ax
	mov	ds, ax
	mov	eax, [bda.timer]	; increment timer tick count
	inc	eax
	cmp	eax, 0x1800b0		; if 24 hours (or more) since
	jae	.ovf			; midnight, increment overflow byte
.cont:
	mov	[bda.timer], eax
	int	0x1c			; invoke user (?) timer tick handler
	mov	al, OCW2_EOI		; send EOI to first PIC
	out	PIC1_CMD, al
	pop	eax
	pop	ds
	iret
.ovf:
	inc	byte [bda.timer_ovf]
	xor	eax, eax
	jmp	.cont

; IRQ 8 (real-time clock periodic interrupt) handler.
	global	irq8
irq8:
	push	ax
	mov	ax, (CMOS_RTC_STA_C | CMOS_NMI_DIS) << 8 \
		    | (CMOS_RTC_STA_B | CMOS_NMI_DIS)
	call	cmos_read		; read both RTC status registers B &
	xchg	ah, al			; C; determine which RTC events are
	call	cmos_read		; triggered _&_ requested
					; at this pt.: ah = stat. B, al = C
	and	al, ah			; now al = C & B
	test	al, RTC_C_TICK		; if periodic interrupt event...
	jz	.no_tick
	push	bx			; ...decrement wait countdown by
	push	ds			; 1/1024 s = 976.5625 us
	xor	bx, bx
	mov	ds, bx
	sub	byte [bda.wait_cntdn_low], ((1000000 << 8) >> 10) % 0x100
	sbb	dword [bda.wait_cntdn], 1000000 >> 10
	jc	.waited			; ... if countdown overshoots 0,
.tick_done:				; wait is complete
	pop	ds
	pop	bx
.no_tick:
	test	al, RTC_C_ALRM
	call	cmos_home		; always "home" CMOS & re-enable NMI
	jnz	.alarm			; if no alarm event...
.alarm_done:
	mov	al, OCW2_EOI		; ...send EOIs to both PICs
	out	PIC2_CMD, al
	out	PIC1_CMD, al
	pop	ax			; & we are done
	iret
.waited:				; if wait complete...
	and	ah, ~RTC_B_TICK_ENA	; ...disable periodic interrupts
	xchg	bx, ax
	mov	al, CMOS_RTC_STA_B | CMOS_NMI_DIS
	call	cmos_write
	mov	[bda.wait_active], al	; ...say there is now no active wait
					; (we have al == 0 == BDA_WAIT_NONE)
	xchg	bx, ax			; ...set user's wait-complete flag
	lds	bx, [bda.p_wait_flag]
	mov	byte [bx], BDA_WAIT_FIN
	jmp	.tick_done		; ...continue processing
.alarm:					; if alarm...
	sti				; ...temporary enable interrupts
	push	dx			; ...invoke user's alarm handler
	int	0x4a			; (IBM's 1985 Nov 11 BIOS sources
	pop	dx			; save & restore dx here, but why?)
	cli
	jmp	.alarm_done		; ...proceed to send EOIs

; Handler for int 0x1a.
	global	isr16_0x1a
isr16_0x1a:
	cmp	ah, (.hndl_end-.hndl)/2
	jae	.not_time_fn
	push	si
	movzx	si, ah
	shl	si, 1
	jmp	word [cs:.hndl+si]
; Function 0x00: get system time & midnight flag.
.fn0x00:
	push	ds
	xor	dx, dx
	mov	ds, dx
	mov	cx, [bda.timer+2]
	mov	dx, [bda.timer]
	mov	al, 0
	xchg	al, [bda.timer_ovf]
	pop	ds
.iret:
	pop	si
	iret
; Function 0x01: set system time.
.fn0x01:
	cmp	cx, 0x18
	ja	.iret
	jb	.ok0x01
	cmp	dx, 0x00b0
	jae	.iret
.ok0x01:
	push	ds
	push	byte 0
	pop	ds
	mov	[bda.timer+2], cx
	mov	[bda.timer], dx
	pop	ds
	pop	si
	iret
; Function 0x02: get RTC time.
.fn0x02:
	call	is_rtc_ok
	jnz	.error
	push	ax
	push	bx
	push	si
	xor	si, si
.wait0x02:
	call	read_rtc		; read the RTC until we get a stable
	xchg	cx, ax			; value
	mov	bx, dx
	call	read_rtc
	cmp	ax, cx
	jnz	.wait0x02
	cmp	bx, dx
	jz	.ok0x02
.retry0x02:
	dec	si			; if we tried too many times & still
	jnz	.wait0x02		; get inconsistent readings...
	pop	bx
	pop	ax
.error:
	pop	si
	stc
	jmp	.done
.ok0x02:				; otherwise, return success
	pop	bx
	pop	ax
	clc
.done:
	sti
	retf	2
.not_time_fn:
	; TODO
	stc
	jmp	.done

.hndl:	dw	.fn0x00, .fn0x01, .fn0x02
.hndl_end:

is_rtc_ok:
	push	ax
	mov	al, CMOS_DIAG | CMOS_NMI_DIS
	call	cmos_read
	test	al, DIAG_BAD_BAT | DIAG_BAD_CKSUM | DIAG_BAD_CLK
	call	cmos_home
	pop	ax
	ret

read_rtc:
	push	ax
	push	cx
	xor	cx, cx
.wait_for_no_upd:			; wait for RTC to stop updating
	mov	al, CMOS_RTC_STA_A | CMOS_NMI_DIS
	call	cmos_read
	test	al, RTC_A_UIP
	loopnz	.wait_for_no_upd
	jnz	.error			; if timed out, then bail out
	pop	cx			; otherwise, read off the time
	mov	al, CMOS_RTC_STA_B | CMOS_NMI_DIS
	call	cmos_read
	and	al, RTC_B_DST
	xchg	dx, ax
	mov	al, CMOS_RTC_SEC | CMOS_NMI_DIS
	call	cmos_read
	mov	dh, al
	mov	al, CMOS_RTC_MIN | CMOS_NMI_DIS
	call	cmos_read
	xchg	cx, ax
	mov	al, CMOS_RTC_HR | CMOS_NMI_DIS
	call	cmos_read
	mov	ch, al
	call	cmos_home
	pop	ax
	clc
	ret
.error:
	pop	cx
	pop	ax
	stc
	ret

; Read CMOS RAM byte at index al, & return read byte in al.
;   * If index is or'ed with CMOS_NMI_DIS, this will disable NMI; call
;     cmos_home later to re-enable NMI.
;   * All registers, including eflags, are preserved.
cmos_read:
	out	PORT_CMOS_IDX, al
	out	PORT_DUMMY, al
	in	al, PORT_CMOS_DATA
	out	PORT_DUMMY, al
	ret

; Write ah to CMOS RAM at index al.
;   * If index is or'ed with CMOS_NMI_DIS, this will disable NMI; call
;     cmos_home later to re-enable NMI.
;   * All registers, including eflags, are preserved.
cmos_write:
	push	ax
	out	PORT_CMOS_IDX, al
	out	PORT_DUMMY, al
	mov	al, ah
	out	PORT_CMOS_DATA, al
	out	PORT_DUMMY, al
	pop	ax
	ret

; Reset the CMOS index register to point to the "default" status register D,
; & also (re-)enable NMI.
;   * This clobbers al, but preserves eflags.
cmos_home:
	mov	al, CMOS_RTC_STA_D
	out	PORT_CMOS_IDX, al
	out	PORT_DUMMY, al
	ret

/*
 * Copyright (c) 2021--2023 TK Chia
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the developer(s) nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef H_STAGE2_STAGE2
#define H_STAGE2_STAGE2

#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include "bparm.h"

/* Macros, inline functions, & other definitions (part 1). */

typedef uint32_t farptr16_t;
/*
 * Real-mode segment of the BIOS data area.  BDA_SEG:0 refers to the same
 * address as the `bda' structure.
 */
#define BDA_SEG		0x40
/* Size of real mode transfer buffer. */
#define TB_SZ		0x100
/* Address space specifier for our 16-bit data segment. */
#define DATA16		__seg_fs

/* conio.c functions. */

extern int cputs (const char *);
extern int putch (char);
extern int vcprintf (const char *, va_list)
	   __attribute__ ((format (printf, 1, 0)));
extern int cprintf (const char *, ...)
	   __attribute__ ((format (printf, 1, 2)));
extern int wherex (void);

/* irq.c functions. */

extern void irq_init (bparm_t *);

/* mem.c functions. */

extern void mem_init (bparm_t *);
extern void *mem_alloc (size_t, size_t, uintptr_t);
extern void *mem_va_map (uint64_t, size_t, unsigned);
extern void mem_va_unmap (volatile void *, size_t);

/* rm16.asm functions and data. */

extern uint16_t rm16_cs;
extern void rm16_init (void);
extern int rm16_call (uint32_t eax, uint32_t edx, uint32_t ecx, uint32_t ebx,
		      farptr16_t callee);
extern void copy_to_tb (const void *, size_t);

/* time.c functions. */

extern void time_init (bparm_t *);

/* usb.c functions. */

extern void usb_init (bparm_t *);

/* usb-ehci.c functions. */

extern void usb_ehci_init_bus (bdat_pci_dev_t *);

/* usb-xhci.c functions. */

extern void usb_xhci_init_bus (bdat_pci_dev_t *);

/* 16/vecs16.asm functions. */

extern void isr16_unimpl (uint32_t eax, uint32_t edx, uint8_t int_no)
	    __attribute__ ((noreturn));

/* 16/tb16.c data. */

extern DATA16 char tb16[TB_SZ];

/* Macros, inline functions, & other definitions (part 2). */

#define XM32_MAX_ADDR	0x100000000ULL	/* end of 32-bit extended memory,
					   i.e. the 4 GiB mark */
#define PAGE_SIZE	0x1000UL	/* size of a virtual memory page */
#define LARGE_PAGE_SIZE	0x200000UL	/* size of a larger VM page */
#define PDPT_ALIGN	0x20U		/* alignment of the page-dir.-ptr.
					   table (PDPT) for PAE paging */

/* Flags in the eflags register. */
#define EFL_C		(1U << 0)	/* carry */

/* Flags in PAE page directory & page table entries. */
#define PTE_P		(1UL <<  0)	/* present */
#define PTE_RW		(1UL <<  1)	/* read/write */
#define PTE_US		(1UL <<  2)	/* user/supervisor */
#define PTE_WT		(1UL <<  3)	/* write-through */
#define PTE_CD		(1UL <<  4)	/* cache disable */
#define PDE_PS		(1UL <<  7)	/* (page dir.) large page size */

/* Flags in the cr0 register. */
#define CR0_PG		(1UL << 31)	/* paging */
#define CR0_PE		(1UL <<  0)	/* protection enable */

/* Flags in the cr4 register. */
#define CR4_PAE		(1UL <<  5)	/* physical address extension (PAE) */

/* Legacy 8259 programmable interrupt controller (PIC) I/O port numbers. */
#define PIC1_CMD	0x0020
#define PIC1_DATA	0x0021
#define PIC2_CMD	0x00a0
#define PIC2_DATA	0x00a1

/* Default base vector addresses. */
#define IRQ0		0x08
#define IRQ8		0x70

/* OCW2 bit fields for the PICs. */
#define OCW2_EOI	0x20		/* non-specific EOI */

/* CMOS port numbers. */
#define PORT_CMOS_IDX	0x0070
#define PORT_CMOS_DATA	0x0071

/* CMOS register indices & flags. */
#define CMOS_RTC_SEC	0x00		/* real-time clock (RTC) seconds */
#define CMOS_RTC_SEC_ALRM 0x01		/* RTC second alarm */
#define CMOS_RTC_MIN	0x02		/* RTC minutes */
#define CMOS_RTC_MIN_ALRM 0x03		/* RTC minute alarm */
#define CMOS_RTC_HR	0x04		/* RTC hours */
#define CMOS_RTC_HR_ALRM 0x05		/* RTC hour alarm */
#define CMOS_RTC_STA_A	0x0a		/* status register A */
#define CMOS_RTC_STA_B	0x0b		/* status register B */
#define CMOS_RTC_STA_C	0x0c		/* status register C */
#define CMOS_RTC_STA_D	0x0d		/* status register D */
#define CMOS_DIAG	0x0e		/* diagnostic status */
#define CMOS_NMI_DIS	0x80		/* flag to disable NMIs */

/* Bit fields in RTC status register A. */
#define RTC_A_UIP	0x80		/* update in progress */
#define RTC_A_RATE_MASK	0x7f		/* rate selection */
#define RTC_A_RATE_1024HZ 0x26		/* - 1024 Hz (w/ 32768 Hz time base) */

/* Bit fields in RTC status register B. */
#define RTC_B_DST	0x01		/* daylight saving time */
#define RTC_B_UPDE_ENA	0x10		/* enable update-ended interrupt */
#define RTC_B_ALRM_ENA	0x20		/* enable alarm interrupt */
#define RTC_B_TICK_ENA	0x40		/* enable periodic interrupt */
#define RTC_B_FREEZE	0x80		/* freeze updates */

/* Bit fields in RTC status register C. */
#define RTC_C_UPDE	RTC_B_UPDE_ENA	/* update-ended interrupt occurred */
#define RTC_C_ALRM	RTC_B_ALRM_ENA	/* alarm interrupt occurred */
#define RTC_C_TICK	RTC_B_TICK_ENA	/* periodic interrupt occurred */

/* Other ports. */
#define PORT_DUMMY	0x0080

/* Values for [0x40:0xa0]. */
#define BDA_WAIT_NONE	0x00	/* no active wait */
#define BDA_WAIT_ACTIVE	0x01	/* active wait */
#define BDA_WAIT_FIN	0x80	/* wait (just) completed */

/*
 * Data structure describing a single memory address range.  The front part
 * is in the same format as returned by int 0x15, ax = 0xe820.
 */
typedef struct __attribute__ ((packed)) mem_range
{
  uint64_t start;
  uint64_t len;
  uint32_t e820_type;
  uint32_t e820_ext_attr;
  uint64_t uefi_attr;
} mem_range_t;

/* BIOS data area variables.  See stage2/stage2.inc. */
typedef struct __attribute__ ((packed))
{
  uint16_t com1;
  uint16_t com2;
  uint16_t com3;
  uint16_t com4;
  uint16_t lpt1;
  uint16_t lpt2;
  uint16_t lpt3;
  uint16_t ebda;
  uint16_t eqpt;
  uint8_t wait_cntdn_low;
  uint16_t base_kib;
  uint16_t : 16;
  uint8_t kb_stat0;
  uint8_t kb_stat1;
  uint8_t kb_keypad;
  uint16_t kb_buf_head;
  uint16_t kb_buf_tail;
  uint16_t kb_buf[16];
  uint8_t fd_recalib;
  uint8_t fd_motor;
  uint8_t fd_cntdn;
  uint8_t fd_error;
  uint8_t dsk_status[7];
  uint8_t vid_mode;
  uint16_t vid_cols;
  uint16_t vid_page_sz;
  uint16_t vid_page_start;
  uint16_t vid_xy[8];
  uint16_t vid_curs_shape;
  uint8_t vid_pg;
  uint16_t crtc;
  uint8_t vid_msr;
  uint8_t vid_pal;
  farptr16_t restart;
  uint8_t stray_irq;
  uint32_t timer;
  uint8_t timer_ovf;
  uint8_t ctrlc;
  uint16_t reset_flag;
  uint8_t hd_error;
  uint8_t hd_cnt;
  uint8_t hd_ctl;
  uint8_t hd_port_off;
  uint8_t lpt1_cntdn;
  uint8_t lpt2_cntdn;
  uint8_t lpt3_cntdn;
  uint8_t flags_0x4b;
  uint8_t com1_cntdn;
  uint8_t com2_cntdn;
  uint8_t com3_cntdn;
  uint8_t com4_cntdn;
  uint16_t kb_buf_start;
  uint16_t kb_buf_end;
  uint8_t vid_rows_m1;
  uint16_t vid_chr_ht;
  uint8_t vid_ctl;
  uint8_t vid_sw;
  uint8_t : 8, : 8;
  uint8_t fd_ctl;
  uint8_t hd_ctlr_sta;
  uint8_t hd_ctlr_err;
  uint8_t hd_intr;
  uint8_t fd_ctl_info;
  uint8_t fd0_media;
  uint8_t fd1_media;
  uint8_t fd0_media_op;
  uint8_t fd1_media_op;
  uint8_t fd0_cyl;
  uint8_t fd1_cyl;
  uint8_t kb_stat3;
  uint8_t kb_stat2;
  farptr16_t p_wait_flag;
  uint32_t wait_cntdn;
  uint8_t wait_active;
  uint8_t : 8, : 8, : 8, : 8, : 8, : 8, : 8;
  farptr16_t p_vid_save;
} bda_t;

extern __seg_gs bda_t bda;

/*
 * Structure for passing register values to & from the C implementations of
 * interrupt service routines.
 */
typedef struct __attribute__ ((packed))
{
  union
  {
    uint32_t eax;
    uint16_t ax;
    struct
    {
      uint8_t al, ah;
    };
  };
  union
  {
    uint32_t ecx;
    uint16_t cx;
    struct
    {
      uint8_t cl, ch;
    };
  };
  union
  {
    uint32_t edx;
    uint16_t dx;
    struct
    {
      uint8_t dl, dh;
    };
  };
  union
  {
    uint32_t ebx;
    uint16_t bx;
    struct
    {
      uint8_t bl, bh;
    };
  };
  union
  {
    uint32_t ebp;
    uint16_t bp;
  };
  union
  {
    uint32_t esi;
    uint16_t si;
  };
  union
  {
    uint32_t edi;
    uint16_t di;
  };
  uint16_t ds, es, fs, gs, ip, cs, flags;
} isr16_regs_t;

/* Fashion a far 16-bit pointer from a 16-bit segment & a 16-bit offset. */
static inline farptr16_t
MK_FP16 (uint16_t seg, uint16_t off)
{
  return (farptr16_t) seg << 16 | off;
}

/* Call a function in our own 16-bit segment. */
static inline int
rm16_cs_call (uint32_t eax, uint32_t edx, uint32_t ecx,
	      uint32_t ebx, int (*callee) (/* ... */))
{
  farptr16_t far_callee = MK_FP16 (rm16_cs, (uint16_t) (uintptr_t) callee);
  return rm16_call (eax, edx, ecx, ebx, far_callee);
}

/* Read cr0. */
static inline uint32_t
rd_cr0 (void)
{
  uint32_t v;
  __asm volatile ("movl %%cr0, %0" : "=r" (v));
  return v;
}

/* Write cr0. */
static inline void
wr_cr0 (uint32_t v)
{
  __asm volatile ("movl %0, %%cr0" : : "r" (v) : "memory");
}

/* Read cr3. */
static inline uint32_t
rd_cr3 (void)
{
  uint32_t v;
  __asm volatile ("movl %%cr3, %0" : "=r" (v));
  return v;
}

/* Write cr3. */
static inline void
wr_cr3 (uint32_t v)
{
  __asm volatile ("movl %0, %%cr3" : : "r" (v):"memory");
}

/* Flush page table caches by reading & writing cr3. */
static inline void
flush_cr3 (void)
{
  wr_cr3 (rd_cr3 ());
}

/* Read cr4. */
static inline uint32_t
rd_cr4 (void)
{
  uint32_t v;
  __asm volatile ("movl %%cr4, %0" : "=r" (v));
  return v;
}

/* Write cr4. */
static inline void
wr_cr4 (uint32_t v)
{
  __asm volatile ("movl %0, %%cr4" : : "r" (v) : "memory");
}

#define IO_WAIT \
	__asm volatile ("outb %%al, %0" : : "Nd" ((uint16_t) PORT_DUMMY))

/* Read a byte from an I/O port. */
static inline uint8_t
inp (uint16_t p)
{
  uint8_t v;
  __asm volatile ("inb %1, %0":"=a" (v) : "Nd" (p));
  return v;
}

/* Read a byte from an I/O port, with a small wait. */
static inline uint8_t
inp_w (uint16_t p)
{
  uint8_t v = inp (p);
  IO_WAIT;
  return v;
}

/* Write a byte to an I/O port. */
static inline void
outp (uint16_t p, uint8_t v)
{
  __asm volatile ("outb %1, %0" : : "Nd" (p), "a" (v));
}

/* Write a byte to an I/O port, then add a small wait. */
static inline void
outp_w (uint16_t p, uint8_t v)
{
  outp (p, v);
  IO_WAIT;
}

/* Read a longword from an I/O port. */
static inline uint32_t
inpd (uint16_t p)
{
  uint32_t v;
  __asm volatile ("inl %1, %0":"=a" (v):"Nd" (p));
  return v;
}

/* Read a longword from an I/O port, with a small wait. */
static inline uint32_t
inpd_w (uint16_t p)
{
  uint32_t v = inpd (p);
  IO_WAIT;
  return v;
}

/* Write a longword to an I/O port. */
static inline void
outpd (uint16_t p, uint32_t v)
{
  __asm volatile ("outl %1, %0" : : "Nd" (p), "a" (v));
}

/* Write a longword to an I/O port, then add a small wait. */
static inline void
outpd_w (uint16_t p, uint32_t v)
{
  outpd (p, v);
  IO_WAIT;
}

#undef IO_WAIT

/*
 * Read a CMOS RAM byte at the given index.
 *   * If index is or'ed with CMOS_NMI_DIS, this will disable NMI; call
 *     cmos_home later to re-enable NMI.
 */
static inline uint8_t
cmos_read (uint8_t idx)
{
  outp_w (PORT_CMOS_IDX, idx);
  return inp_w (PORT_CMOS_DATA);
}

/*
 * Write a byte to CMOS RAM.
 *   * If index is or'ed with CMOS_NMI_DIS, this will disable NMI; call
 *     cmos_home later to re-enable NMI.
 */
static inline void
cmos_write (uint8_t idx, uint8_t v)
{
  outp_w (PORT_CMOS_IDX, idx);
  outp_w (PORT_CMOS_DATA, v);
}

/*
 * Reset the CMOS index register to point to the "default" status register
 * D, & also (re-)enable NMI.
 */
static inline void
cmos_home (void)
{
  outp_w (PORT_CMOS_IDX, CMOS_RTC_STA_D);
}

/* Disable interrupts. */
static inline void
cli (void)
{
  __asm volatile ("cli" : : : "memory");
}

/* Enable interrupts. */
static inline void
sti (void)
{
  __asm volatile ("sti" : : : "memory");
}

/*
 * Temporary enable interrupts, wait for an IRQ, then disable interrupts
 * (again).
 */
static inline void
yield_to_irq (void)
{
  __asm volatile ("sti; hlt; cli" : : : "memory");
}

/* Read a byte at a segment:offset address. */
static inline uint8_t
peekb (uint16_t s, uint32_t o)
{
  uint16_t scratch;
  uint8_t v;
  __asm volatile ("movw %%ds, %0; "
		  "movw %2, %%ds; "
		  "movb %a3, %1; "
		  "movw %0, %%ds"
		  : "=&g" (scratch), "=r" (v)
		  : "rm" (s), "p" (o));
  return v;
}

/* Write a byte at a segment:offset address. */
static inline void
pokeb (uint16_t s, uint32_t o, uint8_t v)
{
  uint16_t scratch;
  __asm volatile ("movw %%ds, %0; "
		  "movw %1, %%ds; "
		  "movb %3, %a2; "
		  "movw %0, %%ds":"=&g" (scratch):"rm" (s), "p" (o),
		  "r" (v):"memory");
}

/* Write a shortword at a segment:offset address. */
static inline void
poke (uint16_t s, uint32_t o, uint16_t v)
{
  uint16_t scratch;
  __asm volatile ("movw %%ds, %0; "
		  "movw %1, %%ds; "
		  "movw %3, %a2; "
		  "movw %0, %%ds"
		  : "=&g" (scratch)
		  : "rm" (s), "p" (o), "r" (v):"memory");
}

#endif

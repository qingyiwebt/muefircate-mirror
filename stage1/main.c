/*
 * Copyright (c) 2020--2021 TK Chia
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

#include <stdbool.h>
#include <string.h>
#include "stage1/stage1.h"
#include "elf.h"
#ifdef XV6_COMPAT
#   include "mp.h"
#endif

extern EFI_HANDLE LibImageHandle;
extern EFI_GUID gEfiLoadedImageProtocolGuid, gEfiGlobalVariableGuid;

extern void bmem_init(void);
extern void run_stage2(Elf32_Addr entry, Elf32_Addr trampoline,
		       unsigned base_kib, uint16_t temp_ebda_seg,
		       bparm_t *bparm);

static EFI_GUID gEfiFirmwareVolume2ProtocolGuid =
		    { 0x220e73b6, 0x6bdb, 0x4413,
		      { 0x84, 0x05, 0xb9, 0x74, 0xb1, 0x08, 0x61, 0x9a } };
static BOOLEAN secure_boot_p = FALSE;
static EFI_HANDLE boot_media_handle;
static uint16_t temp_ebda_seg = 0;

static void process_efi_conf_tables(void)
{
	UINTN i, sct_cnt = ST->NumberOfTableEntries;
	Output(u"EFI sys. conf. tables:");
	for (i = 0; i < sct_cnt; ++i) {
		const EFI_CONFIGURATION_TABLE *cft =
		    &ST->ConfigurationTable[i];
		const EFI_GUID *vguid = &cft->VendorGuid;
		if (i % 2 == 0)
			Output(u"\r\n");
		Print(u"  %08x-%04x-%04x-%02x%02x-"
			 "%02x%02x%02x%02x%02x%02x",
		    vguid->Data1, (UINT32)vguid->Data2, (UINT32)vguid->Data3,
		    (UINT32)vguid->Data4[0], (UINT32)vguid->Data4[1],
		    (UINT32)vguid->Data4[2], (UINT32)vguid->Data4[3],
		    (UINT32)vguid->Data4[4], (UINT32)vguid->Data4[5],
		    (UINT32)vguid->Data4[6], (UINT32)vguid->Data4[7]);
	}
	Output(u"\r\n");
}

static void find_boot_media(void)
{
	EFI_STATUS status;
	EFI_LOADED_IMAGE_PROTOCOL *li;
	status = BS->HandleProtocol(LibImageHandle,
	    &gEfiLoadedImageProtocolGuid, (void **)&li);
	if (EFI_ERROR(status))
		error_with_status(u"cannot get EFI_LOADED_IMAGE_PROTOCOL",
		    status);
	boot_media_handle = li->DeviceHandle;
}

static void test_if_secure_boot(void)
{
	UINT8 data = 0;
	UINTN data_sz = sizeof(data);
	EFI_STATUS status = RT->GetVariable(u"SecureBoot",
	    &gEfiGlobalVariableGuid, NULL, &data_sz, &data);
	if (!EFI_ERROR(status) && data)
		secure_boot_p = TRUE;
	Print(u"secure boot: %s\r\n", secure_boot_p ? u"yes" : u"no");
}

static void find_efi_fvs(void)
{
	EFI_HANDLE *handles;
	UINTN num_handles;
	EFI_STATUS status = LibLocateHandle(ByProtocol,
	    &gEfiFirmwareVolume2ProtocolGuid, NULL, &num_handles, &handles);
	if (EFI_ERROR(status) || !num_handles)
		Output(u"no EFI firmware volumes avail.?\r\n");
	Print(u"EFI firmware volumes: %lu\r\n", num_handles);
	FreePool(handles);
}

static bool enable_legacy_vga(EFI_PCI_IO_PROTOCOL *io, UINT32 class_if,
			      UINT64 attrs, UINT64 supports, UINT64 *p_enables)
{
	EFI_STATUS status;
	UINT64 enables;
	*p_enables = 0;
	switch (class_if & 0xffff0000UL) {
	    case 0x03000000:  /* VGA */
	    case 0x03010000:  /* XGA */
		break;
	    default:
		return false;
	}
	if (!io->RomImage)
		error(u"VGA/XGA device lacks option ROM?");
	switch (supports & (EFI_PCI_ATTRIBUTE_VGA_MEMORY |
			    EFI_PCI_ATTRIBUTE_VGA_IO |
			    EFI_PCI_ATTRIBUTE_VGA_IO_16)) {
	    case EFI_PCI_ATTRIBUTE_VGA_MEMORY | EFI_PCI_ATTRIBUTE_VGA_IO:
		enables = EFI_PCI_ATTRIBUTE_VGA_MEMORY |
			  EFI_PCI_ATTRIBUTE_VGA_IO;
		break;
	    case EFI_PCI_ATTRIBUTE_VGA_MEMORY | EFI_PCI_ATTRIBUTE_VGA_IO_16:
	    case EFI_PCI_ATTRIBUTE_VGA_MEMORY | EFI_PCI_ATTRIBUTE_VGA_IO_16
					      | EFI_PCI_ATTRIBUTE_VGA_IO:
		enables = EFI_PCI_ATTRIBUTE_VGA_MEMORY |
			  EFI_PCI_ATTRIBUTE_VGA_IO_16;
		break;
	    default:
		return false;
	}
	/*
	 * If legacy VGA memory & I/O are already enabled, then there is
	 * nothing more to do.
	 */
	if ((attrs | enables) == attrs)
		return true;
	/* Otherwise... */
	status = io->Attributes(io, EfiPciIoAttributeOperationEnable,
	    enables, NULL);
	if (EFI_ERROR(status))
		return false;
	*p_enables = enables;
	return true;
}

static uint16_t addr_to_rm_seg(uintptr_t p)
{
	return (uint16_t)(p / PARA_SIZE);
}

static uint16_t ptr_to_rm_seg(void *p)
{
	return addr_to_rm_seg((uintptr_t)p);
}

static uint16_t put_opt_rom_in_bmem(EFI_PCI_IO_PROTOCOL *io)
{
	void *orom = io->RomImage, *orom_copy;
	UINT64 sz = io->RomSize;
	if (!sz || !orom)
		return 0;
	if (sz < BMEM_MAX_ADDR &&
	    (uintptr_t)orom <= BMEM_MAX_ADDR - sz &&
	    (uintptr_t)orom % (2 * KIBYTE) == 0) {
		Print(u"    option ROM: @0x%lx\r\n", orom);
		return ptr_to_rm_seg(orom);
	}
	orom_copy = bmem_alloc(sz, 2 * KIBYTE);
	memcpy(orom_copy, orom, sz);
	Print(u"    option ROM: @0x%lx (copied from @0x%lx)\r\n",
	    orom_copy, orom);
	return ptr_to_rm_seg(orom_copy);
}

static bool process_one_pci_io(EFI_PCI_IO_PROTOCOL *io, bool try_enable_vga)
{
	UINTN seg, bus, dev, fn;
	UINT64 attrs, supports, enables;
	UINT32 pci_conf[6];  /* buffer for PCI id. etc., & for BARs */
	UINT32 pci_id, class_if;
	bool got_vga = false, got_bar = false;
	unsigned idx;
	bdat_pci_dev_t *bd;
	EFI_STATUS status = io->GetLocation(io, &seg, &bus, &dev, &fn);
	if (EFI_ERROR(status))
		error_with_status(u"cannot get PCI ctrlr. locn.", status);
	status = io->Attributes(io, EfiPciIoAttributeOperationGet, 0, &attrs);
	if (EFI_ERROR(status))
		error_with_status(u"cannot get PCI ctrlr. attrs.", status);
	status = io->Attributes(io, EfiPciIoAttributeOperationSupported,
	    0, &supports);
	if (EFI_ERROR(status))
		error_with_status(u"cannot get PCI ctrlr. attrs.", status);
	status = io->Pci.Read(io, EfiPciIoWidthUint32, 0, 4, pci_conf);
	if (EFI_ERROR(status))
		error_with_status(u"cannot read PCI conf. sp.", status);
	pci_id = pci_conf[0];
	class_if = pci_conf[2];
	Print(u"  %04x:%02x:%02x.%x %04x:%04x %02x %02x %02x 0x%06lx%c "
		 "0x%06lx%c 0x%06lx%c",
	    seg, bus, dev, fn,
	    (UINT32)(pci_id & 0xffffU), (UINT32)(pci_id >> 16),
	    class_if >> 24, (class_if >> 16) & 0xffU,
	    (class_if >> 8) & 0xffU,
	    io->RomSize <= 0xffffffULL ? io->RomSize : 0xffffffULL,
	    io->RomSize <= 0xffffffULL ? u' ' : u'+',
	    supports & 0xffffffULL,
	    supports & ~0xffffffULL ? u'+' : u' ',
	    attrs & 0xffffffULL,
	    attrs & ~0xffffffULL ? u'+' : u' ');
	/* Skip further processing if this is not a general device. */
	if ((pci_conf[3] >> 8 & 0xff) != 0) {
		Output(u"\r\n");
		return false;
	}
	/* Add a boot parameter for this PCI device. */
	bd = bparm_add(BP_PCID, sizeof(bdat_pci_dev_t));
	bd->pci_locn = seg << 16 | bus << 8 | dev << 3 | fn;
	bd->pci_id = pci_id;
	bd->class_if = class_if;
	bd->orom_seg = 0;
	/*
	 * If this is a VGA or XGA display controller, try to enable
	 * the legacy memory & I/O port locations for the controller.
	 * Also remember the device's location.
	 */
	if (try_enable_vga &&
	    enable_legacy_vga(io, class_if, attrs, supports, &enables)) {
		got_vga = true;
		attrs |= enables;
		Print(u" VGA -> 0x%lx%c", attrs & 0xffffffULL,
		    attrs & ~0xffffffULL ? u'+' : u' ');
	} else
		Output(u"\r\n");
	bd->orom_seg = put_opt_rom_in_bmem(io);
	/* Enumerate all BAR values. */
	status = io->Pci.Read(io, EfiPciIoWidthUint32, 4 * sizeof(UINT32),
	    6, pci_conf);
	if (EFI_ERROR(status))
		error_with_status(u"cannot read PCI conf. sp.", status);
	idx = 0U - 1;
	while (++idx < 6) {
		UINT32 bar = pci_conf[idx];
		UINT64 addr;
		if (!bar)
			continue;
		if (!got_bar) {
			got_bar = true;
			Output(u"    BAR:");
		}
		switch (bar & 0x00000007U) {
		    case 0x00000000U:
			/* 32-bit address in memory space */
			Print(u" {@0x%x%s}", bar & 0xfffffff0U,
			    bar & 0x00000008U ? u" pf" : u"");
				break;
		    case 0x00000004U:
			/* 64-bit address in memory space */
			if (idx == 9)
				error(u"bogus 64-bit PCI BAR");
			++idx;
			addr = pci_conf[idx];
			addr <<= 32;
			addr |= bar & 0xfffffff0U;
			Print(u" {@0x%lx%s}", addr,
			    bar & 0x00000008U ? u" pf" : u"");
			break;
		    case 0x00000001U:
		    case 0x00000003U:
		    case 0x00000005U:
		    case 0x00000007U:
			/* address in I/O space */
			Print(u" {\u2191""0x%x}", bar & 0xfffffffcU);
			break;
		    default:
			error(u"unhandled 16-bit PCI BAR");
		}
	}
	if (got_bar)
		Output(u"\r\n");
	return got_vga;
}

static void process_pci(void)
{
	EFI_HANDLE *handles;
	UINTN num_handles, idx;
	bool got_vga = false;
	EFI_STATUS status = LibLocateHandle(ByProtocol,
	    &gEfiPciIoProtocolGuid, NULL, &num_handles, &handles);
	if (EFI_ERROR(status) || !num_handles)
	        error_with_status(u"no PCI devices found", status);
	Print(u"PCI devices: %lu\r\n"
	       "  locn.        PCI id.   class+IF ROM sz.   "
		 "supports  attrs.\r\n", num_handles);
	for (idx = 0; idx < num_handles; ++idx) {
		EFI_HANDLE handle = handles[idx];
		EFI_PCI_IO_PROTOCOL *io;
		status = BS->HandleProtocol(handle,
		    &gEfiPciIoProtocolGuid, (void **)&io);
		if (EFI_ERROR(status))
			error_with_status(u"cannot get EFI_PCI_IO_PROTOCOL",
			    status);
		got_vga |= process_one_pci_io(io, !got_vga);
	}
	FreePool(handles);
	if (!got_vga)
		error(u"no usable VGA/XGA controller?");
}

static Elf32_Addr alloc_trampoline(void)
{
	EFI_PHYSICAL_ADDRESS addr = 0x100000000ULL;
	EFI_STATUS status = BS->AllocatePages(AllocateMaxAddress,
	    EfiLoaderData, 1, &addr);
	if (EFI_ERROR(status))
		error_with_status(u"cannot get mem. for trampoline & stk.",
		    status);
	Print(u"made space for trampoline & stk. @0x%lx\r\n", addr);
	return (Elf32_Addr)addr;
}

#define STAGE2		u"biefist2.sys"
#define STAGE2_ALT	u"kernel.sys"

static void dump_stage2_info(EFI_FILE_PROTOCOL *prog, CONST CHAR16 *name)
{
	EFI_FILE_INFO *info = LibFileInfo(prog);
	if (!info)
		error(u"cannot get info on stage 2");
	Print(u"stage2: %s  size: 0x%lx  attrs.: 0x%lx\r\n",
	    name, info->FileSize, info->Attribute);
	FreePool(info);
}

static void read_stage2(EFI_FILE_PROTOCOL *prog, EFI_FILE_PROTOCOL *vol,
			UINTN size, void *buf)
{
	UINTN read_size = size;
	EFI_STATUS status = prog->Read(prog, &read_size, buf);
	if (EFI_ERROR(status)) {
		prog->Close(prog);
		vol->Close(vol);
		error_with_status(u"cannot read stage 2", status);
	}
	if (read_size != size) {
		prog->Close(prog);
		vol->Close(vol);
		error_with_status(u"short read from stage 2", status);
	}
}

static void seek_stage2(EFI_FILE_PROTOCOL *prog, EFI_FILE_PROTOCOL *vol,
			UINT64 pos)
{
	EFI_STATUS status = prog->SetPosition(prog, pos);
	if (EFI_ERROR(status)) {
		prog->Close(prog);
		vol->Close(vol);
		error_with_status(u"cannot seek into stage 2", status);
	}
}

static void free_stage2_mem(const Elf32_Phdr *phdrs, UINT32 ph_cnt)
{
	const Elf32_Phdr *phdr = phdrs;
	while (ph_cnt-- != 0) {
		EFI_PHYSICAL_ADDRESS paddr = phdr->p_paddr;
		UINTN pages =
		  ((UINT64)phdrs->p_memsz + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
		BS->FreePages(paddr, pages);
		++phdr;
	}
}

static Elf32_Addr load_stage2(void)
{
	enum { MAX_PHDRS = 16 };
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
	CHAR16 *name = STAGE2;
	EFI_FILE_PROTOCOL *vol, *prog;
	EFI_STATUS status;
	Elf32_Ehdr ehdr;
	Elf32_Phdr phdrs[MAX_PHDRS], *phdr;
	UINT32 x1, x2, ph_cnt, ph_idx, entry;
	status = BS->HandleProtocol(boot_media_handle,
	    &gEfiSimpleFileSystemProtocolGuid, (void **)&fs);
	if (EFI_ERROR(status))
		error_with_status(u"cannot get "
		    "EFI_SIMPLE_FILE_SYSTEM_PROTOCOL", status);
	status = fs->OpenVolume(fs, &vol);
	if (EFI_ERROR(status))
		error_with_status(u"cannot get EFI_FILE_PROTOCOL", status);
	status = vol->Open(vol, &prog, name, EFI_FILE_MODE_READ, 0);
	if (EFI_ERROR(status)) {
		name = STAGE2_ALT;
		status = vol->Open(vol, &prog, name, EFI_FILE_MODE_READ, 0);
	}
	if (EFI_ERROR(status)) {
		vol->Close(vol);
		error_with_status(u"cannot open stage 2", status);
	}
	dump_stage2_info(prog, name);
	read_stage2(prog, vol, sizeof ehdr, &ehdr);
	if (ehdr.e_ident[EI_MAG0] != ELFMAG0 ||
	    ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr.e_ident[EI_MAG2] != ELFMAG2 ||
	    ehdr.e_ident[EI_MAG3] != ELFMAG3) {
		Output(u"  not ELF file\r\n");
		goto bad_elf;
	}
	x1 = ehdr.e_ident[EI_VERSION];
	x2 = ehdr.e_version;
	Print(u"  ELF ver.: %u / %u\r\n", x1, x2);
	if (x1 != EV_CURRENT || x2 != EV_CURRENT)
		goto bad_elf;
	x1 = ehdr.e_ehsize;
	x2 = ehdr.e_phentsize;
	ph_cnt = ehdr.e_phnum;
	Print(u"  ehdr sz.: 0x%x  phdr sz.: 0x%x  phdr cnt.: %u\r\n",
	    x1, x2, ph_cnt);
	if (x1 < sizeof(ehdr) || x2 != sizeof(*phdr))
		goto bad_elf;
	if (ph_cnt > MAX_PHDRS) {
		Output(u"  too many phdrs.\r\n");
		goto bad_elf;
	}
	x1 = ehdr.e_machine;
	entry = ehdr.e_entry;
	Print(u"  machine: 0x%x  entry: @0x%x\r\n", x1, entry);
	if (x1 != EM_386) {
		Output(u"  not x86-32 ELF\r\n");
		goto bad_elf;
	}
	seek_stage2(prog, vol, ehdr.e_phoff);
	read_stage2(prog, vol, ph_cnt * sizeof(*phdr), phdrs);
	Output(u"  phdr# file off.  phy.addr.  virt.addr. type       "
		  "file sz.   mem. sz.\r\n");
	for (ph_idx = 0; ph_idx < ph_cnt; ++ph_idx) {
		phdr = &phdrs[ph_idx];
		Elf32_Word type = phdr->p_type;
		Elf32_Off off = phdr->p_offset;
		EFI_PHYSICAL_ADDRESS paddr = phdr->p_paddr;
		Elf32_Word filesz = phdr->p_filesz, memsz = phdr->p_memsz;
		UINTN pages;
		phdr = &phdrs[ph_idx];
		Print(u"  %5u 0x%08x 0x%08lx 0x%08x 0x%08x 0x%08x 0x%08x\r\n",
		    ph_idx, off, paddr, phdr->p_vaddr, type, filesz, memsz);
		if (type != PT_LOAD)
			continue;
		if (filesz > memsz) {
			free_stage2_mem(phdrs, ph_idx);
			Output(u"  seg. file sz. > seg. mem. sz.!\r\n");
			goto bad_elf;
		}
		pages = ((UINT64)memsz + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
		status = BS->AllocatePages(AllocateAddress,
		    EfiRuntimeServicesData, pages, &paddr);
		if (EFI_ERROR(status)) {
			free_stage2_mem(phdrs, ph_idx);
			error_with_status(u"cannot get mem. for ELF seg.",
			    status);
		}
		seek_stage2(prog, vol, off);
		read_stage2(prog, vol, filesz, (void *)paddr);
		memset((char *)paddr + filesz, 0, memsz - filesz);
	}
	prog->Close(prog);
	vol->Close(vol);
	return entry;
bad_elf:
	prog->Close(prog);
	vol->Close(vol);
	error(u"bad stage2");
	return 0;
}

#ifdef XV6_COMPAT
static uint64_t rdmsr(uint32_t idx)
{
	uint32_t hi, lo;
	__asm volatile("rdmsr" : "=d" (hi), "=a" (lo)
			       : "c" (idx));
	return (uint64_t)hi << 32 | lo;
}

static void cpuid(uint32_t leaf,
		  uint32_t *pa, uint32_t *pb, uint32_t *pc, uint32_t *pd)
{
	uint32_t a, b, c, d;
	__asm volatile("cpuid" : "=a" (a), "=b" (b), "=c" (c), "=d" (d)
			       : "0" (leaf));
	if (pa)
		*pa = a;
	if (pb)
		*pb = b;
	if (pc)
		*pc = c;
	if (pd)
		*pd = d;
}

static void update_cksum(uint8_t *buf, size_t n, uint8_t *p_cksum)
{
	uint8_t *p = buf, cksum = 0;
	*p_cksum = 0;  /* the checksum may be part of the summed area */
	while (n-- != 0)
		cksum -= *p++;
	*p_cksum = cksum;
}

static void fake_mp_table(void)
{
	/*
	 * MIT's Xv6 teaching OS --- to be precise, the x86 version at
	 * https://github.com/mit-pdos/xv6-public --- will panic unless the
	 * Intel multiprocessor configuration tables are set up.
	 *
	 * This code just fashions a minimal set of MP tables to keep Xv6
	 * happy.  The tables are set up in the last KiB of available base
	 * memory.
	 *
	 * The intent is that a more practical ELF32 kernel or bootloader
	 * will obtain the actual multiprocessor setup by other means, such
	 * as ACPI tables.
	 */
	static const char oem_id[8] = "BOCHSCPU", prod_id[12] = "0.1         ";
	typedef struct __attribute__((packed)) {
		unsigned char ebda_kib, ebda_reserved[15];
		intel_mp_flt_t flt;
		intel_mp_conf_hdr_t conf;
		intel_mp_cpu_t cpu;
		intel_mp_ioapic_t ioapic;
	} mp_bundle_t;
	typedef union {
		mp_bundle_t s;
		uint8_t b[sizeof(mp_bundle_t)];
	} mp_bundle_union_t;
	uint32_t cpu_sig, cpu_features;
	/* Get the local APIC address & APIC version. */
	uintptr_t lapic_addr = rdmsr(0x1b) & 0x000ffffffffff000ULL;
	volatile uint32_t *lapic = (volatile uint32_t *)lapic_addr;
	/* Get the pointer to the I/O APIC. */
	volatile uint32_t *ioapic = (volatile uint32_t *)IOAPIC_ADDR;
	/*
	 * Allocate base memory for the MP tables, which will go into a
	 * temporary Extended BIOS Data Area (EBDA).  The MP tables are
	 * expected to be used only at boot time, not run time.
	 */
	mp_bundle_union_t *u = bmem_alloc_boottime(sizeof(mp_bundle_t),
						   PARA_SIZE);
	/* Fill in the length of the temporary EBDA. */
	u->s.ebda_kib = 1;
	memset(u->s.ebda_reserved, 0, 15);
	/* Fill up the MP floating pointer structure. */
	Print(u"placing MP tables @0x%x\r\n", (uintptr_t)u);
	u->s.flt.sig = MAGIC32('_', 'M', 'P', '_');
	u->s.flt.mp_conf_addr = (uint32_t)(uintptr_t)&u->s.conf;
	u->s.flt.len = sizeof(u->s.flt);
	u->s.flt.spec_rev = 4;
	u->s.flt.features[0] = u->s.flt.features[2] =
	    u->s.flt.features[3] = u->s.flt.features[4] = 0;
	u->s.flt.features[1] = MP_FEAT1_IMCRP;
	update_cksum(u->b + offsetof(mp_bundle_t, flt), sizeof(u->s.flt),
	    &u->s.flt.cksum);
	/*
	 * Fill up the MP configuration table header (except for the
	 * checksum).
	 */
	u->s.conf.sig = MAGIC32('P', 'C', 'M', 'P');
	u->s.conf.base_tbl_len = sizeof(u->s.conf) + sizeof(u->s.cpu) +
				 sizeof(u->s.ioapic);
	u->s.conf.spec_rev = 4;
	memcpy(u->s.conf.oem_id, oem_id, sizeof oem_id);
	memcpy(u->s.conf.prod_id, prod_id, sizeof prod_id);
	u->s.conf.oem_tbl_ptr = 0;
	u->s.conf.oem_tbl_sz = 0;
	u->s.conf.ent_cnt = 2;
	u->s.conf.lapic_addr = (uint32_t)lapic_addr;
	u->s.conf.ext_tbl_len = 0;
	u->s.conf.ext_tbl_cksum = u->s.conf.reserved = 0;
	/* While at it... */
	Print(u"LAPIC: @0x%lx\r\n", lapic_addr);
	/* Fill up the entry for this processor. */
	u->s.cpu.type = MP_CPU;
	u->s.cpu.lapic_id = (uint8_t)lapic[0x20 / 4];
	u->s.cpu.lapic_ver = (uint8_t)lapic[0x30 / 4];
	u->s.cpu.cpu_flags = MP_CPU_EN | MP_CPU_BP;
	cpuid(1, &cpu_sig, NULL, NULL, &cpu_features);
	u->s.cpu.cpu_sig = cpu_sig;
	u->s.cpu.cpu_features = cpu_features;
	/* Fill up the entry for the (first) I/O APIC. */
	u->s.ioapic.type = MP_IOAPIC;
	ioapic[0] = 0x00;
	u->s.ioapic.ic_id = (uint8_t)(ioapic[0x10 / 4] >> 24);
	ioapic[0] = 0x01;
	u->s.ioapic.ic_ver = (uint8_t)ioapic[0x10 / 4];
	u->s.ioapic.ic_flags = MP_IOAPIC_EN;
	u->s.ioapic.ic_addr = (uint32_t)IOAPIC_ADDR;
	/* Compute the checksum for the MP configuration table. */
	update_cksum(u->b + offsetof(mp_bundle_t, conf),
		     sizeof(u->s.conf) + sizeof(u->s.cpu) +
		     sizeof(u->s.ioapic), &u->s.conf.cksum);
	/* Set the temporary EBDA segment. */
	temp_ebda_seg = (uint16_t)((uintptr_t)u / PARA_SIZE);
}
#endif

static unsigned prepare_to_hand_over(EFI_HANDLE image_handle)
{
	bdat_bmem_t *bd;
	uint32_t boottime_bmem_bot, runtime_bmem_top;
	EFI_MEMORY_DESCRIPTOR *descs;
	UINTN num_entries = 0, map_key, desc_sz;
	UINT32 desc_ver;
	EFI_STATUS status;
	/*
	 * Wrap up base memory handling.  Add a boot parameter to tell the
	 * bootloader about base memory availability at boot time & run time.
	 */
	bd = bparm_add(BP_BMEM, sizeof(bdat_bmem_t));
	bmem_fini(&boottime_bmem_bot, &runtime_bmem_top);
	bd->boottime_bmem_bot_seg = addr_to_rm_seg(boottime_bmem_bot);
	bd->runtime_bmem_top_seg = addr_to_rm_seg(runtime_bmem_top);
	/* Say we are exiting UEFI. */
	Output(u"exit UEFI\r\n");
	WaitForSingleEvent(ST->ConIn->WaitForKey, 30000000ULL);
	/*
	 * In order to exit boot services, we need to get the memory map
	 * again, this time silently. =_=
	 */
	descs = LibMemoryMap(&num_entries, &map_key, &desc_sz, &desc_ver);
	if (!num_entries || !descs)
		error(u"cannot get mem. map again!");
	status = BS->ExitBootServices(image_handle, map_key);
	if (EFI_ERROR(status))
		error_with_status(u"cannot exit UEFI", status);
	return runtime_bmem_top / KIBYTE;
}

EFI_STATUS efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table)
{
	Elf32_Addr trampoline, entry;
	unsigned base_kib;
	InitializeLib(image_handle, system_table);
	Output(u".:. biefircate " VERSION " .:.\r\n");
	bmem_init();
	process_efi_conf_tables();
	find_boot_media();
	test_if_secure_boot();
	find_efi_fvs();
	process_pci();
	trampoline = alloc_trampoline();
	entry = load_stage2();
#ifdef XV6_COMPAT
	fake_mp_table();
#endif
	base_kib = prepare_to_hand_over(image_handle);
	run_stage2(entry, trampoline, base_kib, temp_ebda_seg, bparm_get());
	return 0;
}

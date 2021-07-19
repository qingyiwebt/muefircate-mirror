#define GNU_EFI_USE_MS_ABI

#include <efi.h>
#include <efilib.h>
#include <string.h>
#include "elf.h"
#ifdef XV6_COMPAT
#   include "mp.h"
#endif

extern EFI_HANDLE LibImageHandle;
extern EFI_GUID gEfiLoadedImageProtocolGuid, gEfiGlobalVariableGuid;

extern void run_stage2(Elf32_Addr, Elf32_Addr, unsigned);

static BOOLEAN secure_boot_p = FALSE;
static EFI_HANDLE boot_media_handle;

static void wait_and_exit(EFI_STATUS status)
{
	Output(u"press a key to exit\r\n");
	WaitForSingleEvent(ST->ConIn->WaitForKey, 0);
	Exit(status, 0, NULL);
}

static void error_with_status(IN CONST CHAR16 *msg, EFI_STATUS status)
{
	Print(u"error: %s: %d\r\n", msg, (INT32)status);
	wait_and_exit(status);
}

static void error(IN CONST CHAR16 *msg)
{
	Print(u"error: %s\r\n", msg);
	wait_and_exit(EFI_ABORTED);
}

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

static void find_pci(void)
{
	EFI_HANDLE *handles;
	UINTN num_handles, idx;
	EFI_STATUS status = LibLocateHandle(ByProtocol,
	    &gEfiPciRootBridgeIoProtocolGuid, NULL, &num_handles, &handles);
	if (EFI_ERROR(status))
	        error_with_status(u"no PCI root bridges found", status);
	Print(u"PCI root bridges: %lu\r\n"
	       "  seg.       curr.attrs.        supported attrs.\r\n",
	    num_handles);
	for (idx = 0; idx < num_handles; ++idx) {
		EFI_HANDLE handle = handles[idx];
		EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *rb;
		UINT64 supports, attrs;
		status = BS->HandleProtocol(handle,
		    &gEfiPciRootBridgeIoProtocolGuid, (void **)&rb);
		if (EFI_ERROR(status))
			error_with_status
			    (u"cannot get EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL",
			     status);
		status = rb->GetAttributes(rb, &supports, &attrs);
		if (EFI_ERROR(status))
			error_with_status(u"cannot get attrs. of bridge",
			    status);
		Print(u"  0x%08x 0x%016lx 0x%016lx\r\n",
		    rb->SegmentNumber, attrs, supports);
	}
	FreePool(handles);
}

static unsigned process_memory_map(void)
{
	enum { BASE_MEM_MAX = 0xff000ULL };
	EFI_MEMORY_DESCRIPTOR *desc;
	UINTN num_entries = 0, map_key, desc_sz;
	UINT32 desc_ver;
	unsigned i;
	char avail[BASE_MEM_MAX / EFI_PAGE_SIZE];
	desc = LibMemoryMap(&num_entries, &map_key, &desc_sz, &desc_ver);
	if (!num_entries)
		error(u"cannot get memory map!");
	memset(avail, 0, sizeof avail);
	Output(u"memory map below 16 MiB:\r\n"
		"  start    end       type attrs.\r\n");
	while (num_entries-- != 0) {
		EFI_PHYSICAL_ADDRESS start, end;
		start = desc->PhysicalStart;
		if (start <= 0xffffffUL) {
			end = start + desc->NumberOfPages * EFI_PAGE_SIZE;
			Print(u"  0x%06lx 0x%06lx%c %4u 0x%016lx\r\n", start,
			    end > 0x1000000ULL ? (UINT64)0x1000000ULL - 1
					       : end - 1,
			    end > 0x1000000ULL ? u'+' : u' ',
			    (UINT32)desc->Type, desc->Attribute);
		}
		if (start < BASE_MEM_MAX) {
			switch (desc->Type) {
			    case EfiConventionalMemory:
			    case EfiLoaderCode:
			    case EfiLoaderData:
			    case EfiBootServicesCode:
			    case EfiBootServicesData:
				while (start < end && start < BASE_MEM_MAX) {
					avail[start / EFI_PAGE_SIZE] = 1;
					start += EFI_PAGE_SIZE;
				}
			    default:
				;
			}
		}
		/* :-| */
		desc = (EFI_MEMORY_DESCRIPTOR *)((char *)desc + desc_sz);
	}
	i = 0;
	while (i < sizeof avail && avail[i])
		++i;
	i = (i * EFI_PAGE_SIZE) / 1024U;
	Print(u"base mem. avail.: %u KiB\r\n", (UINT32)i);
	if (i < 128)
		error(u"too little base mem.!");
	return i;
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

static void fake_mp_table(unsigned base_kib)
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
	static const char flt_sig[4] = "_MP_", conf_sig[4] = "PCMP",
			  oem_id[8] = "BOCHSCPU", prod_id[12] = "0.1         ";
	typedef struct __attribute__((packed)) {
		intel_mp_flt_t flt;
		intel_mp_conf_hdr_t conf;
		intel_mp_cpu_t cpu;
		intel_mp_ioapic_t ioapic;
	} mp_bundle_t;
	typedef union {
		mp_bundle_t s;
		uint8_t b[sizeof(mp_bundle_t)];
	} mp_bundle_union_t;
	mp_bundle_union_t *u =
	    (mp_bundle_union_t *)((uintptr_t)(base_kib - 1) << 10);
	uint32_t cpu_sig, cpu_features;
	/* Get the local APIC address & APIC version. */
	uintptr_t lapic_addr = rdmsr(0x1b) & 0x000ffffffffff000ULL;
	volatile uint32_t *lapic = (volatile uint32_t *)lapic_addr;
	/* Get the I/O APIC information. */
	volatile uint32_t *ioapic = (volatile uint32_t *)IOAPIC_ADDR;
	/* Fill up the MP floating pointer structure. */
	memcpy(u->s.flt.sig, flt_sig, sizeof flt_sig);
	u->s.flt.mp_conf_addr = (uint32_t)(uintptr_t)&u->s.conf;
	u->s.flt.len = sizeof(u->s.flt);
	u->s.flt.spec_rev = 4;
	u->s.flt.features[0] = u->s.flt.features[2] =
	    u->s.flt.features[3] = u->s.flt.features[4] = 0;
	u->s.flt.features[1] = MP_FEAT1_IMCRP;
	update_cksum(u->b, sizeof(u->s.flt), &u->s.flt.cksum);
	/*
	 * Fill up the MP configuration table header (except for the
	 * checksum).
	 */
	memcpy(u->s.conf.sig, conf_sig, sizeof conf_sig);
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
	update_cksum(u->b + sizeof(u->s.flt),
		     sizeof(u->s.conf) + sizeof(u->s.cpu) +
		     sizeof(u->s.ioapic), &u->s.conf.cksum);
}
#endif

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

static Elf32_Addr alloc_trampoline(void)
{
	EFI_PHYSICAL_ADDRESS addr = 0x100000000ULL;
	EFI_STATUS status = BS->AllocatePages(AllocateMaxAddress,
	    EfiLoaderData, 1, &addr);
	if (EFI_ERROR(status))
		error_with_status(u"cannot alloc. mem. for trampoline & stk.",
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
	UINT32 x1, x2, ph_cnt, ph_idx;
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
	Print(u"  machine: 0x%x\r\n", x1);
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
			error_with_status(u"cannot alloc. mem. for ELF seg.",
			    status);
		}
		seek_stage2(prog, vol, off);
		read_stage2(prog, vol, filesz, (void *)paddr);
		memset((char *)paddr + filesz, 0, memsz - filesz);
	}
	prog->Close(prog);
	vol->Close(vol);
	Print(u"  entry: @0x%x\r\n", ehdr.e_entry);
	return ehdr.e_entry;
bad_elf:
	prog->Close(prog);
	vol->Close(vol);
	error(u"bad stage2");
	return 0;
}

EFI_STATUS efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table)
{
	unsigned base_kib;
	Elf32_Addr trampoline, entry;
	InitializeLib(image_handle, system_table);
	Output(u".:. biefircate " VERSION " .:.\r\n");
	process_efi_conf_tables();
	find_pci();
	base_kib = process_memory_map();
#ifdef XV6_COMPAT
	fake_mp_table(base_kib);
#endif
	find_boot_media();
	test_if_secure_boot();
	trampoline = alloc_trampoline();
	entry = load_stage2();
	run_stage2(entry, trampoline, base_kib);
	return 0;
}
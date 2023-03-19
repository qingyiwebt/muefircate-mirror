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

extern EFI_HANDLE LibImageHandle;
extern EFI_GUID gEfiLoadedImageProtocolGuid, gEfiGlobalVariableGuid;

static EFI_GUID Acpi20TableGuid
  = {
      0x8868e871, 0xe4f1, 0x11d3,
      { 0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81 }
    };

static BOOLEAN secure_boot_p = FALSE;
static EFI_HANDLE boot_media_handle;
static uint16_t temp_ebda_seg = 0;

static void
init (void)
{
  conf_init ();
  bmem_init ();
  fv_init ();
}

static void
process_efi_conf_tables (void)
{
  UINTN i, sct_cnt = ST->NumberOfTableEntries;
  acpi_xsdp_t *rsdp = NULL;
  info (u"EFI sys. conf. tables:");
  for (i = 0; i < sct_cnt; ++i)
    {
      const EFI_CONFIGURATION_TABLE *cft = &ST->ConfigurationTable[i];
      const EFI_GUID *vguid = &cft->VendorGuid;
      if (i % 2 == 0)
	info (u"\r\n");
      info (u"  ");
      print_guid (vguid);
      if (memcmp (vguid, &Acpi20TableGuid, sizeof (EFI_GUID)) == 0)
	rsdp = cft->VendorTable;
    }
  info (u"\r\n");
  if (!rsdp)
    error (u"no ACPI 2+ RSDP");
  acpi_init (rsdp);
}

static void
find_boot_media (void)
{
  EFI_STATUS status;
  EFI_LOADED_IMAGE_PROTOCOL *li;
  status = BS->HandleProtocol (LibImageHandle,
			       &gEfiLoadedImageProtocolGuid, (void **) &li);
  if (EFI_ERROR (status))
    error_with_status (u"cannot get EFI_LOADED_IMAGE_PROTOCOL", status);
  boot_media_handle = li->DeviceHandle;
}

static void
test_if_secure_boot (void)
{
  UINT8 data = 0;
  UINTN data_sz = sizeof (data);
  EFI_STATUS status = RT->GetVariable (u"SecureBoot",
				       &gEfiGlobalVariableGuid, NULL,
				       &data_sz, &data);
  if (!EFI_ERROR (status) && data)
    secure_boot_p = TRUE;
  infof (u"secure boot: %s\r\n", secure_boot_p ? u"yes" : u"no");
}

static Elf32_Addr
alloc_trampoline (void)
{
  EFI_PHYSICAL_ADDRESS addr = 0x100000000ULL;
  EFI_STATUS status = BS->AllocatePages (AllocateMaxAddress,
					 EfiLoaderData, 1, &addr);
  if (EFI_ERROR (status))
    error_with_status (u"cannot get mem. for trampoline & stk.", status);
  infof (u"made space for trampoline & stk. @0x%lx\r\n", addr);
  return (Elf32_Addr) addr;
}

#define STAGE2		u"EFI\\biefirc\\stage2.sys"
#define STAGE2_ALT	u"biefist2.sys"
#define STAGE2_ALT_ALT	u"kernel.sys"

static void
dump_stage2_info (EFI_FILE_PROTOCOL * prog, CONST CHAR16 * name)
{
  EFI_FILE_INFO *info = LibFileInfo (prog);
  if (!info)
    error (u"cannot get info on stage 2");
  infof (u"stage2: %s  size: 0x%lx  attrs.: 0x%lx\r\n",
	 name, info->FileSize, info->Attribute);
  FreePool (info);
}

static void
read_stage2 (EFI_FILE_PROTOCOL * prog, EFI_FILE_PROTOCOL * vol,
	     UINTN size, void *buf)
{
  UINTN read_size = size;
  EFI_STATUS status;
  if (!size)
    return;
  status = prog->Read (prog, &read_size, buf);
  if (EFI_ERROR (status))
    {
      prog->Close (prog);
      vol->Close (vol);
      error_with_status (u"cannot read stage 2", status);
    }
  if (read_size != size)
    {
      prog->Close (prog);
      vol->Close (vol);
      error_with_status (u"short read from stage 2", status);
    }
}

static void
seek_stage2 (EFI_FILE_PROTOCOL * prog, EFI_FILE_PROTOCOL * vol, UINT64 pos)
{
  EFI_STATUS status = prog->SetPosition (prog, pos);
  if (EFI_ERROR (status))
    {
      prog->Close (prog);
      vol->Close (vol);
      error_with_status (u"cannot seek into stage 2", status);
    }
}

static void
free_stage2_mem (const Elf32_Phdr * phdrs, UINT32 ph_cnt)
{
  const Elf32_Phdr *phdr = phdrs;
  while (ph_cnt-- != 0)
    {
      EFI_PHYSICAL_ADDRESS paddr = phdr->p_paddr;
      UINTN pages =
	((UINT64) phdrs->p_memsz + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
      BS->FreePages (paddr, pages);
      ++phdr;
    }
}

static Elf32_Addr
load_stage2 (void)
{
  enum
  { MAX_PHDRS = 16 };
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
  CHAR16 *name = STAGE2;
  EFI_FILE_PROTOCOL *vol, *prog;
  EFI_STATUS status;
  Elf32_Ehdr ehdr;
  Elf32_Phdr phdrs[MAX_PHDRS], *phdr;
  UINT32 x1, x2, ph_cnt, ph_idx, entry;
  status = BS->HandleProtocol (boot_media_handle,
			       &gEfiSimpleFileSystemProtocolGuid,
			       (void **) &fs);
  if (EFI_ERROR (status))
    error_with_status (u"cannot get "
		       "EFI_SIMPLE_FILE_SYSTEM_PROTOCOL", status);
  status = fs->OpenVolume (fs, &vol);
  if (EFI_ERROR (status))
    error_with_status (u"cannot get EFI_FILE_PROTOCOL", status);
  status = vol->Open (vol, &prog, name, EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR (status))
    {
      name = STAGE2_ALT;
      status = vol->Open (vol, &prog, name, EFI_FILE_MODE_READ, 0);
    }
  if (EFI_ERROR (status))
    {
      name = STAGE2_ALT_ALT;
      status = vol->Open (vol, &prog, name, EFI_FILE_MODE_READ, 0);
    }
  if (EFI_ERROR (status))
    {
      vol->Close (vol);
      error_with_status (u"cannot open stage 2", status);
    }
  dump_stage2_info (prog, name);
  read_stage2 (prog, vol, sizeof ehdr, &ehdr);
  if (ehdr.e_ident[EI_MAG0] != ELFMAG0
      || ehdr.e_ident[EI_MAG1] != ELFMAG1
      || ehdr.e_ident[EI_MAG2] != ELFMAG2
      || ehdr.e_ident[EI_MAG3] != ELFMAG3)
    {
      info (u"  not ELF file\r\n");
      goto bad_elf;
    }
  x1 = ehdr.e_ident[EI_VERSION];
  x2 = ehdr.e_version;
  infof (u"  ELF ver.: %u / %u\r\n", x1, x2);
  if (x1 != EV_CURRENT || x2 != EV_CURRENT)
    goto bad_elf;
  x1 = ehdr.e_ehsize;
  x2 = ehdr.e_phentsize;
  ph_cnt = ehdr.e_phnum;
  infof (u"  ehdr sz.: 0x%x  phdr sz.: 0x%x  phdr cnt.: %u\r\n",
	 x1, x2, ph_cnt);
  if (x1 < sizeof (ehdr) || x2 != sizeof (*phdr))
    goto bad_elf;
  if (ph_cnt > MAX_PHDRS)
    {
      info (u"  too many phdrs.\r\n");
      goto bad_elf;
    }
  x1 = ehdr.e_machine;
  entry = ehdr.e_entry;
  infof (u"  machine: 0x%x  entry: @0x%x\r\n", x1, entry);
  if (x1 != EM_386)
    {
      info (u"  not x86-32 ELF\r\n");
      goto bad_elf;
    }
  seek_stage2 (prog, vol, ehdr.e_phoff);
  read_stage2 (prog, vol, ph_cnt * sizeof (*phdr), phdrs);
  info (u"  phdr# file off.  phy.addr.  virt.addr. type       "
	"file sz.   mem. sz.\r\n");
  for (ph_idx = 0; ph_idx < ph_cnt; ++ph_idx)
    {
      phdr = &phdrs[ph_idx];
      Elf32_Word type = phdr->p_type;
      Elf32_Off off = phdr->p_offset;
      EFI_PHYSICAL_ADDRESS paddr = phdr->p_paddr,
	slack = paddr % EFI_PAGE_SIZE;
      Elf32_Word filesz = phdr->p_filesz, memsz = phdr->p_memsz;
      UINTN pages;
      phdr = &phdrs[ph_idx];
      infof (u"  %5u 0x%08x 0x%08lx 0x%08x 0x%08x 0x%08x 0x%08x\r\n",
	     ph_idx, off, paddr, phdr->p_vaddr, type, filesz, memsz);
      if (type != PT_LOAD)
	continue;
      if (filesz > memsz)
	{
	  free_stage2_mem (phdrs, ph_idx);
	  info (u"  seg. file sz. > seg. mem. sz.!\r\n");
	  goto bad_elf;
	}
      if (slack)
	{
	  off -= slack;
	  paddr -= slack;
	  if (filesz)
	    filesz += slack;
	  memsz += slack;
	}
      pages = ((UINT64) memsz + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
      status = BS->AllocatePages (AllocateAddress,
				  EfiRuntimeServicesData, pages, &paddr);
      if (EFI_ERROR (status))
	{
	  free_stage2_mem (phdrs, ph_idx);
	  error_with_status (u"cannot get mem. for ELF seg.", status);
	}
      if (filesz)
	{
	  seek_stage2 (prog, vol, off);
	  read_stage2 (prog, vol, filesz, (void *) paddr);
	}
      memset ((char *) paddr + filesz, 0, memsz - filesz);
    }
  prog->Close (prog);
  vol->Close (vol);
  return entry;
bad_elf:
  prog->Close (prog);
  vol->Close (vol);
  error (u"bad stage2");
  return 0;
}

static void
get_time (EFI_TIME * when)
{
  EFI_STATUS status = RT->GetTime (when, NULL);
  if (EFI_ERROR (status))
    error_with_status (u"cannot get time", status);
}

static unsigned
prepare_to_hand_over (EFI_HANDLE image_handle)
{
  enum
  { EfiPersistentMemory = EfiPalCode + 1 };
  bdat_bmem_t *bd;
  uint32_t boottime_bmem_bot, runtime_bmem_top;
  EFI_MEMORY_DESCRIPTOR *descs, *desc;
  UINTN num_ents = 0, map_key, desc_sz, ent_iter;
  EFI_STATUS status;
  /* Wrap up firmware volume handling. */
  fv_fini ();
  /* Say we are about to exit UEFI. */
  info (u"exit UEFI\r\n");
  /*
   * Add information about blocks of extended memory (above the 1 MiB
   * mark) to the boot parameters.
   */
  descs = get_mem_map (&num_ents, &map_key, &desc_sz);
  FOR_EACH_MEM_DESC (desc, descs, desc_sz, num_ents, ent_iter)
  {
    EFI_PHYSICAL_ADDRESS start = desc->PhysicalStart, end;
    uint32_t e820_type;
    end = start + desc->NumberOfPages * EFI_PAGE_SIZE;
    if (end <= BMEM_MAX_ADDR && end != 0)
      continue;
    if (start < BMEM_MAX_ADDR)
      start = BMEM_MAX_ADDR;
    switch (desc->Type)
      {
      case EfiLoaderCode:
      case EfiLoaderData:
      case EfiBootServicesCode:
      case EfiBootServicesData:
      case EfiConventionalMemory:
	e820_type = E820_RAM;
	break;
      case EfiACPIReclaimMemory:
	e820_type = E820_ACPI;
	break;
      case EfiACPIMemoryNVS:
	e820_type = E820_NVS;
	break;
      case EfiPersistentMemory:
	e820_type = E820_PMEM;
	break;
      default:
	e820_type = E820_RESERVED;
      }
    bparm_add_mem_range (start, end - start, e820_type, 1U, desc->Attribute);
  }
  /*
   * Wrap up base memory handling.  Add boot parameters to tell the
   * bootloader about base memory availability at boot time & run
   * time.
   *
   * We can only do this after adding the boot parameters for the map
   * of extended memory, because these latter parameters will
   * themselves be allocated in base memory (via bmem.c).
   */
  bd = bparm_add (BP_BMEM, sizeof (bdat_bmem_t));
  bmem_fini (descs, num_ents, desc_sz, &boottime_bmem_bot, &runtime_bmem_top);
  bd->boottime_bmem_bot_seg = addr_to_rm_seg (boottime_bmem_bot);
  bd->runtime_bmem_top_seg = addr_to_rm_seg (runtime_bmem_top);
  /* Wrap up any other stuff. */
  conf_fini ();
  /* Wait for about 3 seconds. */
  sleepx (3, NULL);
  /* Really exit boot services... */
  status = BS->ExitBootServices (image_handle, map_key);
  if (EFI_ERROR (status))
    error_with_status (u"cannot exit UEFI", status);
  return runtime_bmem_top / KIBYTE;
}

EFI_STATUS
efi_main (EFI_HANDLE image_handle, EFI_SYSTEM_TABLE * system_table)
{
  Elf32_Addr trampoline, entry;
  unsigned base_kib;
  InitializeLib (image_handle, system_table);
  info (u".:. biefircate " PACKAGE_VERSION " .:.\r\n");
  init ();
  process_efi_conf_tables ();
  find_boot_media ();
  test_if_secure_boot ();
  process_pci ();
  trampoline = alloc_trampoline ();
  entry = load_stage2 ();
  base_kib = prepare_to_hand_over (image_handle);
  run_stage2 (entry, trampoline, base_kib, temp_ebda_seg, bparm_get ());
  return 0;
}

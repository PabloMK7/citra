// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>
#include <memory>

#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/symbols.h"

#include "core/hle/kernel/kernel.h"
#include "core/loader/elf.h"
#include "core/mem_map.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// ELF Header Constants

// File type
enum ElfType {
    ET_NONE   = 0,
    ET_REL    = 1,
    ET_EXEC   = 2,
    ET_DYN    = 3,
    ET_CORE   = 4,
    ET_LOPROC = 0xFF00,
    ET_HIPROC = 0xFFFF,
};

// Machine/Architecture
enum ElfMachine {
    EM_NONE  = 0,
    EM_M32   = 1,
    EM_SPARC = 2,
    EM_386   = 3,
    EM_68K   = 4,
    EM_88K   = 5,
    EM_860   = 7,
    EM_MIPS  = 8
};

// File version
#define EV_NONE    0
#define EV_CURRENT 1

// Identification index
#define EI_MAG0    0
#define EI_MAG1    1
#define EI_MAG2    2
#define EI_MAG3    3
#define EI_CLASS   4
#define EI_DATA    5
#define EI_VERSION 6
#define EI_PAD     7
#define EI_NIDENT 16

// Sections constants

// Section types
#define SHT_NULL            0
#define SHT_PROGBITS        1
#define SHT_SYMTAB          2
#define SHT_STRTAB          3
#define SHT_RELA            4
#define SHT_HASH            5
#define SHT_DYNAMIC         6
#define SHT_NOTE            7
#define SHT_NOBITS          8
#define SHT_REL             9
#define SHT_SHLIB          10
#define SHT_DYNSYM         11
#define SHT_LOPROC 0x70000000
#define SHT_HIPROC 0x7FFFFFFF
#define SHT_LOUSER 0x80000000
#define SHT_HIUSER 0xFFFFFFFF

// Section flags
enum ElfSectionFlags
{
    SHF_WRITE     = 0x1,
    SHF_ALLOC     = 0x2,
    SHF_EXECINSTR = 0x4,
    SHF_MASKPROC  = 0xF0000000,
};

// Segment types
#define PT_NULL             0
#define PT_LOAD             1
#define PT_DYNAMIC          2
#define PT_INTERP           3
#define PT_NOTE             4
#define PT_SHLIB            5
#define PT_PHDR             6
#define PT_LOPROC  0x70000000
#define PT_HIPROC  0x7FFFFFFF

typedef unsigned int   Elf32_Addr;
typedef unsigned short Elf32_Half;
typedef unsigned int   Elf32_Off;
typedef signed   int   Elf32_Sword;
typedef unsigned int   Elf32_Word;

////////////////////////////////////////////////////////////////////////////////////////////////////
// ELF file header

struct Elf32_Ehdr {
    unsigned char e_ident[EI_NIDENT];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
};

// Section header
struct Elf32_Shdr {
    Elf32_Word sh_name;
    Elf32_Word sh_type;
    Elf32_Word sh_flags;
    Elf32_Addr sh_addr;
    Elf32_Off  sh_offset;
    Elf32_Word sh_size;
    Elf32_Word sh_link;
    Elf32_Word sh_info;
    Elf32_Word sh_addralign;
    Elf32_Word sh_entsize;
};

// Segment header
struct Elf32_Phdr {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
};

// Symbol table entry
struct Elf32_Sym {
    Elf32_Word    st_name;
    Elf32_Addr    st_value;
    Elf32_Word    st_size;
    unsigned char st_info;
    unsigned char st_other;
    Elf32_Half    st_shndx;
};

// Relocation entries
struct Elf32_Rel {
    Elf32_Addr r_offset;
    Elf32_Word r_info;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// ElfReader class

typedef int SectionID;

class ElfReader {
private:
    char *base;
    u32 *base32;

    Elf32_Ehdr *header;
    Elf32_Phdr *segments;
    Elf32_Shdr *sections;

    u32 *sectionAddrs;
    bool relocate;
    u32 entryPoint;

public:
    ElfReader(void *ptr);

    u32 Read32(int off) const { return base32[off >> 2]; }

    // Quick accessors
    ElfType GetType() const { return (ElfType)(header->e_type); }
    ElfMachine GetMachine() const { return (ElfMachine)(header->e_machine); }
    u32 GetEntryPoint() const { return entryPoint; }
    u32 GetFlags() const { return (u32)(header->e_flags); }
    void LoadInto(u32 vaddr);
    bool LoadSymbols();

    int GetNumSegments() const { return (int)(header->e_phnum); }
    int GetNumSections() const { return (int)(header->e_shnum); }
    const u8 *GetPtr(int offset) const { return (u8*)base + offset; }
    const char *GetSectionName(int section) const;
    const u8 *GetSectionDataPtr(int section) const {
        if (section < 0 || section >= header->e_shnum)
            return nullptr;
        if (sections[section].sh_type != SHT_NOBITS)
            return GetPtr(sections[section].sh_offset);
        else
            return nullptr;
    }
    bool IsCodeSection(int section) const {
        return sections[section].sh_type == SHT_PROGBITS;
    }
    const u8 *GetSegmentPtr(int segment) {
        return GetPtr(segments[segment].p_offset);
    }
    u32 GetSectionAddr(SectionID section) const { return sectionAddrs[section]; }
    unsigned int GetSectionSize(SectionID section) const { return sections[section].sh_size; }
    SectionID GetSectionByName(const char *name, int firstSection = 0) const; //-1 for not found

    bool DidRelocate() const {
        return relocate;
    }
};

ElfReader::ElfReader(void *ptr) {
    base = (char*)ptr;
    base32 = (u32*)ptr;
    header = (Elf32_Ehdr*)ptr;

    segments = (Elf32_Phdr*)(base + header->e_phoff);
    sections = (Elf32_Shdr*)(base + header->e_shoff);

    entryPoint = header->e_entry;

    LoadSymbols();
}

const char *ElfReader::GetSectionName(int section) const {
    if (sections[section].sh_type == SHT_NULL)
        return nullptr;

    int name_offset = sections[section].sh_name;
    const char* ptr = (char*)GetSectionDataPtr(header->e_shstrndx);

    if (ptr)
        return ptr + name_offset;

    return nullptr;
}

void ElfReader::LoadInto(u32 vaddr) {
    LOG_DEBUG(Loader, "String section: %i", header->e_shstrndx);

    // Should we relocate?
    relocate = (header->e_type != ET_EXEC);

    if (relocate) {
        LOG_DEBUG(Loader, "Relocatable module");
        entryPoint += vaddr;
    } else {
        LOG_DEBUG(Loader, "Prerelocated executable");
    }
    LOG_DEBUG(Loader, "%i segments:", header->e_phnum);

    // First pass : Get the bits into RAM
    u32 segment_addr[32];
    u32 base_addr = relocate ? vaddr : 0;

    for (unsigned i = 0; i < header->e_phnum; i++) {
        Elf32_Phdr* p = segments + i;
        LOG_DEBUG(Loader, "Type: %i Vaddr: %08x Filesz: %i Memsz: %i ", p->p_type, p->p_vaddr,
                  p->p_filesz, p->p_memsz);

        if (p->p_type == PT_LOAD) {
            segment_addr[i] = base_addr + p->p_vaddr;
            memcpy(Memory::GetPointer(segment_addr[i]), GetSegmentPtr(i), p->p_filesz);
            LOG_DEBUG(Loader, "Loadable Segment Copied to %08x, size %08x", segment_addr[i],
                      p->p_memsz);
        }
    }
    LOG_DEBUG(Loader, "Done loading.");
}

SectionID ElfReader::GetSectionByName(const char *name, int firstSection) const {
    for (int i = firstSection; i < header->e_shnum; i++) {
        const char *secname = GetSectionName(i);

        if (secname != nullptr && strcmp(name, secname) == 0)
            return i;
    }
    return -1;
}

bool ElfReader::LoadSymbols() {
    bool hasSymbols = false;
    SectionID sec = GetSectionByName(".symtab");
    if (sec != -1) {
        int stringSection = sections[sec].sh_link;
        const char *stringBase = (const char *)GetSectionDataPtr(stringSection);

        //We have a symbol table!
        Elf32_Sym* symtab = (Elf32_Sym *)(GetSectionDataPtr(sec));
        unsigned int numSymbols = sections[sec].sh_size / sizeof(Elf32_Sym);
        for (unsigned sym = 0; sym < numSymbols; sym++) {
            int size = symtab[sym].st_size;
            if (size == 0)
                continue;

            int type = symtab[sym].st_info & 0xF;

            const char *name = stringBase + symtab[sym].st_name;

            Symbols::Add(symtab[sym].st_value, name, size, type);

            hasSymbols = true;
        }
    }

    return hasSymbols;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Loader namespace

namespace Loader {

FileType AppLoader_ELF::IdentifyType(FileUtil::IOFile& file) {
    u32 magic;
    file.Seek(0, SEEK_SET);
    if (1 != file.ReadArray<u32>(&magic, 1))
        return FileType::Error;

    if (MakeMagic('\x7f', 'E', 'L', 'F') == magic)
        return FileType::ELF;

    return FileType::Error;
}

ResultStatus AppLoader_ELF::Load() {
    if (is_loaded)
        return ResultStatus::ErrorAlreadyLoaded;

    if (!file->IsOpen())
        return ResultStatus::Error;

    // Reset read pointer in case this file has been read before.
    file->Seek(0, SEEK_SET);

    u32 size = static_cast<u32>(file->GetSize());
    std::unique_ptr<u8[]> buffer(new u8[size]);
    if (file->ReadBytes(&buffer[0], size) != size)
        return ResultStatus::Error;

    Kernel::g_current_process = Kernel::Process::Create(filename, 0);
    Kernel::g_current_process->svc_access_mask.set();
    Kernel::g_current_process->address_mappings = default_address_mappings;

    ElfReader elf_reader(&buffer[0]);
    elf_reader.LoadInto(Memory::PROCESS_IMAGE_VADDR);
    // TODO: Fill application title

    Kernel::g_current_process->Run(elf_reader.GetEntryPoint(), 48, Kernel::DEFAULT_STACK_SIZE);

    is_loaded = true;
    return ResultStatus::Success;
}

} // namespace Loader

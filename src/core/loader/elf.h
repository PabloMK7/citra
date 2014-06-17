// Copyright 2013 Dolphin Emulator Project / Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

// ELF Header Constants

// File type
enum ElfType {
    ET_NONE = 0,
    ET_REL = 1,
    ET_EXEC = 2,
    ET_DYN = 3,
    ET_CORE = 4,
    ET_LOPROC = 0xFF00,
    ET_HIPROC = 0xFFFF,
};

// Machine/Architecture
enum ElfMachine {
    EM_NONE = 0,
    EM_M32 = 1,
    EM_SPARC = 2,
    EM_386 = 3,
    EM_68K = 4,
    EM_88K = 5,
    EM_860 = 7,
    EM_MIPS = 8
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

// Magic number
#define ELFMAG0 0x7F
#define ELFMAG1  'E'
#define ELFMAG2  'L'
#define ELFMAG3  'F'

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
    SHF_WRITE = 0x1,
    SHF_ALLOC = 0x2,
    SHF_EXECINSTR = 0x4,
    SHF_MASKPROC = 0xF0000000,
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

typedef unsigned int  Elf32_Addr;
typedef unsigned short Elf32_Half;
typedef unsigned int  Elf32_Off;
typedef signed   int  Elf32_Sword;
typedef unsigned int  Elf32_Word;

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

typedef int SectionID;

class ElfReader {
private:
    char *base;
    u32 *base32;

    Elf32_Ehdr *header;
    Elf32_Phdr *segments;
    Elf32_Shdr *sections;

    u32 *sectionAddrs;
    bool bRelocate;
    u32 entryPoint;

public:
    ElfReader(void *ptr);
    ~ElfReader() { }

    u32 Read32(int off) const { return base32[off >> 2]; }

    // Quick accessors
    ElfType GetType() const { return (ElfType)(header->e_type); }
    ElfMachine GetMachine() const { return (ElfMachine)(header->e_machine); }
    u32 GetEntryPoint() const { return entryPoint; }
    u32 GetFlags() const { return (u32)(header->e_flags); }
    bool LoadInto(u32 vaddr);
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
    int GetSectionSize(SectionID section) const { return sections[section].sh_size; }
    SectionID GetSectionByName(const char *name, int firstSection = 0) const; //-1 for not found

    bool DidRelocate() {
        return bRelocate;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Loader namespace

namespace Loader {

/**
 * Loads an ELF file
 * @param filename String filename of ELF file
 * @param error_string Pointer to string to put error message if an error has occurred
 * @return True on success, otherwise false
 */
bool Load_ELF(std::string& filename, std::string* error_string);

} // namespace Loader

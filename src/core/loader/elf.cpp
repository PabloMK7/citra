// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>

#include "common/common.h"
#include "common/file_util.h"
#include "common/symbols.h"

#include "core/mem_map.h"
#include "core/loader/elf.h"
#include "core/hle/kernel/kernel.h"

ElfReader::ElfReader(void *ptr) {
    base = (char*)ptr;
    base32 = (u32 *)ptr;
    header = (Elf32_Ehdr*)ptr;

    segments = (Elf32_Phdr *)(base + header->e_phoff);
    sections = (Elf32_Shdr *)(base + header->e_shoff);

    entryPoint = header->e_entry;

    LoadSymbols();
}

const char *ElfReader::GetSectionName(int section) const {
    if (sections[section].sh_type == SHT_NULL)
        return nullptr;

    int nameOffset = sections[section].sh_name;
    char *ptr = (char*)GetSectionDataPtr(header->e_shstrndx);

    if (ptr)
        return ptr + nameOffset;
    else
        return nullptr;
}

bool ElfReader::LoadInto(u32 vaddr) {
    DEBUG_LOG(MASTER_LOG, "String section: %i", header->e_shstrndx);

    // Should we relocate?
    bRelocate = (header->e_type != ET_EXEC);

    if (bRelocate)
    {
        DEBUG_LOG(MASTER_LOG, "Relocatable module");
        entryPoint += vaddr;
    }
    else
    {
        DEBUG_LOG(MASTER_LOG, "Prerelocated executable");
    }

    INFO_LOG(MASTER_LOG, "%i segments:", header->e_phnum);

    // First pass : Get the bits into RAM
    u32 segmentVAddr[32];

    u32 baseAddress = bRelocate ? vaddr : 0;

    for (int i = 0; i < header->e_phnum; i++)
    {
        Elf32_Phdr *p = segments + i;

        INFO_LOG(MASTER_LOG, "Type: %i Vaddr: %08x Filesz: %i Memsz: %i ", p->p_type, p->p_vaddr, p->p_filesz, p->p_memsz);

        if (p->p_type == PT_LOAD)
        {
            segmentVAddr[i] = baseAddress + p->p_vaddr;
            u32 writeAddr = segmentVAddr[i];

            const u8 *src = GetSegmentPtr(i);
            u8 *dst = Memory::GetPointer(writeAddr);
            u32 srcSize = p->p_filesz;
            u32 dstSize = p->p_memsz;
            u32 *s = (u32*)src;
            u32 *d = (u32*)dst;
            for (int j = 0; j < (int)(srcSize + 3) / 4; j++)
            {
                *d++ = /*_byteswap_ulong*/(*s++);
            }
            if (srcSize < dstSize)
            {
                //memset(dst + srcSize, 0, dstSize-srcSize); //zero out bss
            }
            INFO_LOG(MASTER_LOG, "Loadable Segment Copied to %08x, size %08x", writeAddr, p->p_memsz);
        }
    }


    INFO_LOG(MASTER_LOG, "Done loading.");
    return true;
}

SectionID ElfReader::GetSectionByName(const char *name, int firstSection) const
{
    for (int i = firstSection; i < header->e_shnum; i++)
    {
        const char *secname = GetSectionName(i);

        if (secname != nullptr && strcmp(name, secname) == 0)
            return i;
    }
    return -1;
}

bool ElfReader::LoadSymbols()
{
    bool hasSymbols = false;
    SectionID sec = GetSectionByName(".symtab");
    if (sec != -1)
    {
        int stringSection = sections[sec].sh_link;
        const char *stringBase = (const char *)GetSectionDataPtr(stringSection);

        //We have a symbol table!
        Elf32_Sym *symtab = (Elf32_Sym *)(GetSectionDataPtr(sec));
        int numSymbols = sections[sec].sh_size / sizeof(Elf32_Sym);
        for (int sym = 0; sym < numSymbols; sym++)
        {
            int size = symtab[sym].st_size;
            if (size == 0)
                continue;

            // int bind = symtab[sym].st_info >> 4;
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

/**
 * Loads an ELF file
 * @param filename String filename of ELF file
 * @param error_string Pointer to string to put error message if an error has occurred
 * @return True on success, otherwise false
 */
bool Load_ELF(std::string& filename, std::string* error_string) {
    std::string full_path = filename;
    std::string path, file, extension;
    SplitPath(ReplaceAll(full_path, "\\", "/"), &path, &file, &extension);
#if EMU_PLATFORM == PLATFORM_WINDOWS
    path = ReplaceAll(path, "/", "\\");
#endif
    File::IOFile f(filename, "rb");

    if (f.IsOpen()) {
        u32 size = (u32)f.GetSize();
        u8* buffer = new u8[size];
        ElfReader* elf_reader = NULL;

        f.ReadBytes(buffer, size);

        elf_reader = new ElfReader(buffer);
        elf_reader->LoadInto(0x00100000);

        Kernel::LoadExec(elf_reader->GetEntryPoint());

        delete[] buffer;
        delete elf_reader;
    } else {
        *error_string = "Unable to open ELF file!";
        return false;
    }
    f.Close();

    return true;
}

} // namespace Loader

// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <string>

#include "common/memory_util.h"
#include "common/mem_arena.h"
#include "common/string_util.h"

#ifndef _WIN32
#include <fcntl.h>
#ifdef ANDROID
#include <sys/ioctl.h>
#include <linux/ashmem.h>
#endif
#endif

#ifdef ANDROID

// Hopefully this ABI will never change...


#define ASHMEM_DEVICE "/dev/ashmem"

/*
* ashmem_create_region - creates a new ashmem region and returns the file
* descriptor, or <0 on error
*
* `name' is an optional label to give the region (visible in /proc/pid/maps)
* `size' is the size of the region, in page-aligned bytes
*/
int ashmem_create_region(const char *name, size_t size)
{
    int fd, ret;

    fd = open(ASHMEM_DEVICE, O_RDWR);
    if (fd < 0)
        return fd;

    if (name) {
        char buf[ASHMEM_NAME_LEN];

        strncpy(buf, name, sizeof(buf));
        ret = ioctl(fd, ASHMEM_SET_NAME, buf);
        if (ret < 0)
            goto error;
    }

    ret = ioctl(fd, ASHMEM_SET_SIZE, size);
    if (ret < 0)
        goto error;

    return fd;

error:
    LOG_ERROR(Common_Memory, "NASTY ASHMEM ERROR: ret = %08x", ret);
    close(fd);
    return ret;
}

int ashmem_set_prot_region(int fd, int prot)
{
    return ioctl(fd, ASHMEM_SET_PROT_MASK, prot);
}

int ashmem_pin_region(int fd, size_t offset, size_t len)
{
    struct ashmem_pin pin = { offset, len };
    return ioctl(fd, ASHMEM_PIN, &pin);
}

int ashmem_unpin_region(int fd, size_t offset, size_t len)
{
    struct ashmem_pin pin = { offset, len };
    return ioctl(fd, ASHMEM_UNPIN, &pin);
}
#endif  // Android


#if defined(_WIN32)
SYSTEM_INFO sysInfo;
#endif


// Windows mappings need to be on 64K boundaries, due to Alpha legacy.
#ifdef _WIN32
size_t roundup(size_t x) {
    int gran = sysInfo.dwAllocationGranularity ? sysInfo.dwAllocationGranularity : 0x10000;
    return (x + gran - 1) & ~(gran - 1);
}
#else
size_t roundup(size_t x) {
    return x;
}
#endif


void MemArena::GrabLowMemSpace(size_t size)
{
#ifdef _WIN32
    hMemoryMapping = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, (DWORD)(size), nullptr);
    GetSystemInfo(&sysInfo);
#elif defined(ANDROID)
    // Use ashmem so we don't have to allocate a file on disk!
    fd = ashmem_create_region("PPSSPP_RAM", size);
    // Note that it appears that ashmem is pinned by default, so no need to pin.
    if (fd < 0)
    {
        LOG_ERROR(Common_Memory, "Failed to grab ashmem space of size: %08x  errno: %d", (int)size, (int)(errno));
        return;
    }
#else
    // Try to find a non-existing filename for our shared memory.
    // In most cases the first one will be available, but it's nicer to search
    // a bit more.
    for (int i = 0; i < 10000; i++)
    {
        std::string file_name = Common::StringFromFormat("/citramem.%d", i);
        fd = shm_open(file_name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd != -1)
        {
            shm_unlink(file_name.c_str());
            break;
        }
        else if (errno != EEXIST)
        {
            LOG_ERROR(Common_Memory, "shm_open failed: %s", strerror(errno));
            return;
        }
    }
    if (ftruncate(fd, size) < 0)
        LOG_ERROR(Common_Memory, "Failed to allocate low memory space");
#endif
}


void MemArena::ReleaseSpace()
{
#ifdef _WIN32
    CloseHandle(hMemoryMapping);
    hMemoryMapping = 0;
#else
    close(fd);
#endif
}


void *MemArena::CreateView(s64 offset, size_t size, void *base)
{
#ifdef _WIN32
    size = roundup(size);
    void *ptr = MapViewOfFileEx(hMemoryMapping, FILE_MAP_ALL_ACCESS, 0, (DWORD)((u64)offset), size, base);
    return ptr;
#else
    void *retval = mmap(base, size, PROT_READ | PROT_WRITE, MAP_SHARED |
        // Do not sync memory to underlying file. Linux has this by default.
#ifdef __FreeBSD__
        MAP_NOSYNC |
#endif
        ((base == nullptr) ? 0 : MAP_FIXED), fd, offset);

    if (retval == MAP_FAILED)
    {
        LOG_ERROR(Common_Memory, "mmap failed");
        return nullptr;
    }
    return retval;
#endif
}


void MemArena::ReleaseView(void* view, size_t size)
{
#ifdef _WIN32
    UnmapViewOfFile(view);
#else
    munmap(view, size);
#endif
}

u8* MemArena::Find4GBBase()
{
#ifdef _M_X64
#ifdef _WIN32
    // 64 bit
    u8* base = (u8*)VirtualAlloc(0, 0xE1000000, MEM_RESERVE, PAGE_READWRITE);
    VirtualFree(base, 0, MEM_RELEASE);
    return base;
#else
    // Very precarious - mmap cannot return an error when trying to map already used pages.
    // This makes the Windows approach above unusable on Linux, so we will simply pray...
    return reinterpret_cast<u8*>(0x2300000000ULL);
#endif

#else // 32 bit

#ifdef _WIN32
    u8* base = (u8*)VirtualAlloc(0, 0x10000000, MEM_RESERVE, PAGE_READWRITE);
    if (base) {
        VirtualFree(base, 0, MEM_RELEASE);
    }
    return base;
#else
    void* base = mmap(0, 0x10000000, PROT_READ | PROT_WRITE,
        MAP_ANON | MAP_SHARED, -1, 0);
    if (base == MAP_FAILED) {
        PanicAlert("Failed to map 256 MB of memory space: %s", strerror(errno));
        return 0;
    }
    munmap(base, 0x10000000);
    return static_cast<u8*>(base);
#endif
#endif
}


// yeah, this could also be done in like two bitwise ops...
#define SKIP(a_flags, b_flags)
//if (!(a_flags & MV_WII_ONLY) && (b_flags & MV_WII_ONLY))
//    continue;
//if (!(a_flags & MV_FAKE_VMEM) && (b_flags & MV_FAKE_VMEM))
//    continue;

static bool Memory_TryBase(u8 *base, const MemoryView *views, int num_views, u32 flags, MemArena *arena) {
    // OK, we know where to find free space. Now grab it!
    // We just mimic the popular BAT setup.
    size_t position = 0;
    size_t last_position = 0;

    // Zero all the pointers to be sure.
    for (int i = 0; i < num_views; i++)
    {
        if (views[i].out_ptr_low)
            *views[i].out_ptr_low = 0;
        if (views[i].out_ptr)
            *views[i].out_ptr = 0;
    }

    int i;
    for (i = 0; i < num_views; i++)
    {
        const MemoryView &view = views[i];
        if (view.size == 0)
            continue;
        SKIP(flags, view.flags);
        if (view.flags & MV_MIRROR_PREVIOUS) {
            position = last_position;
        }
        else {
            *(view.out_ptr_low) = (u8*)arena->CreateView(position, view.size);
            if (!*view.out_ptr_low)
                goto bail;
        }
#ifdef _M_X64
        *view.out_ptr = (u8*)arena->CreateView(
            position, view.size, base + view.virtual_address);
#else
        if (view.flags & MV_MIRROR_PREVIOUS) {  // TODO: should check if the two & 0x3FFFFFFF are identical.
            // No need to create multiple identical views.
            *view.out_ptr = *views[i - 1].out_ptr;
        }
        else {
            *view.out_ptr = (u8*)arena->CreateView(
                position, view.size, base + (view.virtual_address & 0x3FFFFFFF));
            if (!*view.out_ptr)
                goto bail;
        }
#endif

        last_position = position;
        position += roundup(view.size);
    }

    return true;

bail:
    // Argh! ERROR! Free what we grabbed so far so we can try again.
    for (int j = 0; j <= i; j++)
    {
        if (views[i].size == 0)
            continue;
        SKIP(flags, views[i].flags);
        if (views[j].out_ptr_low && *views[j].out_ptr_low)
        {
            arena->ReleaseView(*views[j].out_ptr_low, views[j].size);
            *views[j].out_ptr_low = nullptr;
        }
        if (*views[j].out_ptr)
        {
#ifdef _M_X64
            arena->ReleaseView(*views[j].out_ptr, views[j].size);
#else
            if (!(views[j].flags & MV_MIRROR_PREVIOUS))
            {
                arena->ReleaseView(*views[j].out_ptr, views[j].size);
            }
#endif
            *views[j].out_ptr = nullptr;
        }
    }
    return false;
}

u8 *MemoryMap_Setup(const MemoryView *views, int num_views, u32 flags, MemArena *arena)
{
    size_t total_mem = 0;
    int base_attempts = 0;

    for (int i = 0; i < num_views; i++)
    {
        if (views[i].size == 0)
            continue;
        SKIP(flags, views[i].flags);
        if ((views[i].flags & MV_MIRROR_PREVIOUS) == 0)
            total_mem += roundup(views[i].size);
    }
    // Grab some pagefile backed memory out of the void ...
    arena->GrabLowMemSpace(total_mem);

    // Now, create views in high memory where there's plenty of space.
#ifdef _M_X64
    u8 *base = MemArena::Find4GBBase();
    // This really shouldn't fail - in 64-bit, there will always be enough
    // address space.
    if (!Memory_TryBase(base, views, num_views, flags, arena))
    {
        PanicAlert("MemoryMap_Setup: Failed finding a memory base.");
        return 0;
    }
#elif defined(_WIN32)
    // Try a whole range of possible bases. Return once we got a valid one.
    u32 max_base_addr = 0x7FFF0000 - 0x10000000;
    u8 *base = nullptr;

    for (u32 base_addr = 0x01000000; base_addr < max_base_addr; base_addr += 0x400000)
    {
        base_attempts++;
        base = (u8 *)base_addr;
        if (Memory_TryBase(base, views, num_views, flags, arena))
        {
            LOG_DEBUG(Common_Memory, "Found valid memory base at %p after %i tries.", base, base_attempts);
            base_attempts = 0;
            break;
        }
    }
#else
    // Linux32 is fine with the x64 method, although limited to 32-bit with no automirrors.
    u8 *base = MemArena::Find4GBBase();
    if (!Memory_TryBase(base, views, num_views, flags, arena))
    {
        LOG_ERROR(Common_Memory, "MemoryMap_Setup: Failed finding a memory base.");
        PanicAlert("MemoryMap_Setup: Failed finding a memory base.");
        return 0;
    }
#endif
    if (base_attempts)
        PanicAlert("No possible memory base pointer found!");
    return base;
}

void MemoryMap_Shutdown(const MemoryView *views, int num_views, u32 flags, MemArena *arena)
{
    for (int i = 0; i < num_views; i++)
    {
        if (views[i].size == 0)
            continue;
        SKIP(flags, views[i].flags);
        if (views[i].out_ptr_low && *views[i].out_ptr_low)
            arena->ReleaseView(*views[i].out_ptr_low, views[i].size);
        if (*views[i].out_ptr && (views[i].out_ptr_low && *views[i].out_ptr != *views[i].out_ptr_low))
            arena->ReleaseView(*views[i].out_ptr, views[i].size);
        *views[i].out_ptr = nullptr;
        if (views[i].out_ptr_low)
            *views[i].out_ptr_low = nullptr;
    }
}

// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/logging/log.h"

#include "core/mem_map.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Memory {

u8* g_exefs_code;  ///< ExeFS:/.code is loaded here
u8* g_heap;        ///< Application heap (main memory)
u8* g_shared_mem;  ///< Shared memory
u8* g_heap_linear; ///< Linear heap
u8* g_vram;        ///< Video memory (VRAM) pointer
u8* g_dsp_mem;     ///< DSP memory
u8* g_tls_mem;     ///< TLS memory

namespace {

struct MemoryArea {
    u8** ptr;
    size_t size;
};

// We don't declare the IO regions in here since its handled by other means.
static MemoryArea memory_areas[] = {
    {&g_exefs_code,  PROCESS_IMAGE_MAX_SIZE},
    {&g_heap,        HEAP_SIZE             },
    {&g_shared_mem,  SHARED_MEMORY_SIZE    },
    {&g_heap_linear, LINEAR_HEAP_SIZE      },
    {&g_vram,        VRAM_SIZE             },
    {&g_dsp_mem,     DSP_RAM_SIZE          },
    {&g_tls_mem,     TLS_AREA_SIZE         },
};

}

void Init() {
    for (MemoryArea& area : memory_areas) {
        *area.ptr = new u8[area.size];
    }
    MemBlock_Init();

    LOG_DEBUG(HW_Memory, "initialized OK, RAM at %p", g_heap);
}

void Shutdown() {
    MemBlock_Shutdown();
    for (MemoryArea& area : memory_areas) {
        delete[] *area.ptr;
        *area.ptr = nullptr;
    }

    LOG_DEBUG(HW_Memory, "shutdown OK");
}

} // namespace

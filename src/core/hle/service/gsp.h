// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/bit_field.h"
#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace GSP_GPU

namespace GSP_GPU {

enum class GXCommandId : u32 {
    REQUEST_DMA            = 0x00,
    SET_COMMAND_LIST_LAST  = 0x01,

    // Fills a given memory range with a particular value
    SET_MEMORY_FILL        = 0x02,

    // Copies an image and optionally performs color-conversion or scaling.
    // This is highly similar to the GameCube's EFB copy feature
    SET_DISPLAY_TRANSFER   = 0x03,

    // Conceptionally similar to SET_DISPLAY_TRANSFER and presumable uses the same hardware path
    SET_TEXTURE_COPY       = 0x04,

    SET_COMMAND_LIST_FIRST = 0x05,
};

enum class GXInterruptId : u8 {
    PSC0    = 0x00,
    PSC1    = 0x01,
    PDC0    = 0x02, // Seems called every vertical screen line
    PDC1    = 0x03, // Seems called every frame
    PPF     = 0x04,
    P3D     = 0x05,
    DMA     = 0x06,
};

struct GXCommand {
    BitField<0, 8, GXCommandId> id;

    union {
        struct {
            u32 source_address;
            u32 dest_address;
            u32 size;
        } dma_request;

        struct {
            u32 address;
            u32 size;
        } set_command_list_last;

        struct {
            u32 start1;
            u32 value1;
            u32 end1;
            u32 start2;
            u32 value2;
            u32 end2;
        } memory_fill;

        struct {
            u32 in_buffer_address;
            u32 out_buffer_address;
            u32 in_buffer_size;
            u32 out_buffer_size;
            u32 flags;
        } image_copy;

        u8 raw_data[0x1C];
    };
};
static_assert(sizeof(GXCommand) == 0x20, "GXCommand struct has incorrect size");

/// Interface to "srv:" service
class Interface : public Service::Interface {
public:

    Interface();

    ~Interface();

    /**
     * Gets the string port name used by CTROS for the service
     * @return Port name of service
     */
    const char *GetPortName() const {
        return "gsp::Gpu";
    }

};

/**
 * Signals that the specified interrupt type has occurred to userland code
 * @param interrupt_id ID of interrupt that is being signalled
 */
void SignalInterrupt(GXInterruptId interrupt_id);

} // namespace

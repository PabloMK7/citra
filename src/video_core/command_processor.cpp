// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "pica.h"
#include "command_processor.h"


namespace Pica {

Regs registers;

namespace CommandProcessor {

static inline void WritePicaReg(u32 id, u32 value) {
    u32 old_value = registers[id];
    registers[id] = value;

    switch(id) {
        // TODO: Perform actions for anything which requires special treatment here...

        default:
            break;
    }
}

static std::ptrdiff_t ExecuteCommandBlock(const u32* first_command_word) {
    const CommandHeader& header = *(const CommandHeader*)(&first_command_word[1]);

    u32* read_pointer = (u32*)first_command_word;

    // TODO: Take parameter mask into consideration!

    WritePicaReg(header.cmd_id, *read_pointer);
    read_pointer += 2;

    for (int i = 1; i < 1+header.extra_data_length; ++i) {
        u32 cmd = header.cmd_id + ((header.group_commands) ? i : 0);
        WritePicaReg(cmd, *read_pointer);
        ++read_pointer;
    }

    // align read pointer to 8 bytes
    if ((first_command_word - read_pointer) % 2)
        ++read_pointer;

    return read_pointer - first_command_word;
}

void ProcessCommandList(const u32* list, u32 size) {
    u32* read_pointer = (u32*)list;

    while (read_pointer < list + size) {
        read_pointer += ExecuteCommandBlock(read_pointer);
    }
}

} // namespace

} // namespace

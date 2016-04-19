// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/apt/bcfnt/bcfnt.h"
#include "core/hle/service/service.h"

namespace Service {
namespace APT {
namespace BCFNT {

void RelocateSharedFont(Kernel::SharedPtr<Kernel::SharedMemory> shared_font, VAddr previous_address, VAddr new_address) {
    static const u32 SharedFontStartOffset = 0x80;
    u8* data = shared_font->GetPointer(SharedFontStartOffset);

    CFNT cfnt;
    memcpy(&cfnt, data, sizeof(cfnt));

    // Advance past the header
    data = shared_font->GetPointer(SharedFontStartOffset + cfnt.header_size);

    for (unsigned block = 0; block < cfnt.num_blocks; ++block) {

        u32 section_size = 0;
        if (memcmp(data, "FINF", 4) == 0) {
            BCFNT::FINF finf;
            memcpy(&finf, data, sizeof(finf));
            section_size = finf.section_size;

            // Relocate the offsets in the FINF section
            finf.cmap_offset += new_address - previous_address;
            finf.cwdh_offset += new_address - previous_address;
            finf.tglp_offset += new_address - previous_address;

            memcpy(data, &finf, sizeof(finf));
        } else if (memcmp(data, "CMAP", 4) == 0) {
            BCFNT::CMAP cmap;
            memcpy(&cmap, data, sizeof(cmap));
            section_size = cmap.section_size;

            // Relocate the offsets in the CMAP section
            cmap.next_cmap_offset += new_address - previous_address;

            memcpy(data, &cmap, sizeof(cmap));
        } else if (memcmp(data, "CWDH", 4) == 0) {
            BCFNT::CWDH cwdh;
            memcpy(&cwdh, data, sizeof(cwdh));
            section_size = cwdh.section_size;

            // Relocate the offsets in the CWDH section
            cwdh.next_cwdh_offset += new_address - previous_address;

            memcpy(data, &cwdh, sizeof(cwdh));
        } else if (memcmp(data, "TGLP", 4) == 0) {
            BCFNT::TGLP tglp;
            memcpy(&tglp, data, sizeof(tglp));
            section_size = tglp.section_size;

            // Relocate the offsets in the TGLP section
            tglp.sheet_data_offset += new_address - previous_address;

            memcpy(data, &tglp, sizeof(tglp));
        }

        data += section_size;
    }
}

} // namespace BCFNT
} // namespace APT
} // namespace Service
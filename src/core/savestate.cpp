// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <boost/serialization/binary_object.hpp>
#include <cryptopp/hex.h>
#include "common/archives.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "common/zstd_compression.h"
#include "core/cheats/cheats.h"
#include "core/core.h"
#include "core/savestate.h"
#include "network/network.h"
#include "video_core/video_core.h"

namespace Core {

#pragma pack(push, 1)
struct CSTHeader {
    std::array<u8, 4> filetype;  /// Unique Identifier to check the file type (always "CST"0x1B)
    u64_le program_id;           /// ID of the ROM being executed. Also called title_id
    std::array<u8, 20> revision; /// Git hash of the revision this savestate was created with
    u64_le time;                 /// The time when this save state was created

    std::array<u8, 216> reserved; /// Make heading 256 bytes so it has consistent size

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::binary_object(this, sizeof(CSTHeader));
    }
};
static_assert(sizeof(CSTHeader) == 256, "CSTHeader should be 256 bytes");
#pragma pack(pop)

constexpr std::array<u8, 4> header_magic_bytes{{'C', 'S', 'T', 0x1B}};

std::string GetSaveStatePath(u64 program_id, u32 slot) {
    return fmt::format("{}{:016X}.{:02d}.cst", FileUtil::GetUserPath(FileUtil::UserPath::StatesDir),
                       program_id, slot);
}

std::vector<SaveStateInfo> ListSaveStates(u64 program_id) {
    std::vector<SaveStateInfo> result;
    for (u32 slot = 1; slot <= SaveStateSlotCount; ++slot) {
        const auto path = GetSaveStatePath(program_id, slot);
        if (!FileUtil::Exists(path)) {
            continue;
        }

        SaveStateInfo info;
        info.slot = slot;

        FileUtil::IOFile file(path, "rb");
        if (!file) {
            LOG_ERROR(Core, "Could not open file {}", path);
            continue;
        }
        CSTHeader header;
        if (file.GetSize() < sizeof(header)) {
            LOG_ERROR(Core, "File too small {}", path);
            continue;
        }
        if (file.ReadBytes(&header, sizeof(header)) != sizeof(header)) {
            LOG_ERROR(Core, "Could not read from file {}", path);
            continue;
        }
        if (header.filetype != header_magic_bytes) {
            LOG_WARNING(Core, "Invalid save state file {}", path);
            continue;
        }
        info.time = header.time;

        if (header.program_id != program_id) {
            LOG_WARNING(Core, "Save state file isn't for the current game {}", path);
            continue;
        }
        std::string revision = fmt::format("{:02x}", fmt::join(header.revision, ""));
        if (revision == Common::g_scm_rev) {
            info.status = SaveStateInfo::ValidationStatus::OK;
        } else {
            LOG_WARNING(Core, "Save state file created from a different revision {}", path);
            info.status = SaveStateInfo::ValidationStatus::RevisionDismatch;
        }
        result.emplace_back(std::move(info));
    }
    return result;
}

void System::SaveState(u32 slot) const {
    std::ostringstream sstream{std::ios_base::binary};
    // Serialize
    oarchive oa{sstream};
    oa&* this;

    const std::string& str{sstream.str()};
    auto buffer = Common::Compression::CompressDataZSTDDefault(
        reinterpret_cast<const u8*>(str.data()), str.size());

    const auto path = GetSaveStatePath(title_id, slot);
    if (!FileUtil::CreateFullPath(path)) {
        throw std::runtime_error("Could not create path " + path);
    }

    FileUtil::IOFile file(path, "wb");
    if (!file) {
        throw std::runtime_error("Could not open file " + path);
    }

    CSTHeader header{};
    header.filetype = header_magic_bytes;
    header.program_id = title_id;
    std::string rev_bytes;
    CryptoPP::StringSource(Common::g_scm_rev, true,
                           new CryptoPP::HexDecoder(new CryptoPP::StringSink(rev_bytes)));
    std::memcpy(header.revision.data(), rev_bytes.data(), sizeof(header.revision));
    header.time = std::chrono::duration_cast<std::chrono::seconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();

    if (file.WriteBytes(&header, sizeof(header)) != sizeof(header) ||
        file.WriteBytes(buffer.data(), buffer.size()) != buffer.size()) {
        throw std::runtime_error("Could not write to file " + path);
    }
}

void System::LoadState(u32 slot) {
    if (Network::GetRoomMember().lock()->IsConnected()) {
        throw std::runtime_error("Unable to load while connected to multiplayer");
    }

    const auto path = GetSaveStatePath(title_id, slot);

    std::vector<u8> decompressed;
    {
        std::vector<u8> buffer(FileUtil::GetSize(path) - sizeof(CSTHeader));

        FileUtil::IOFile file(path, "rb");
        file.Seek(sizeof(CSTHeader), SEEK_SET); // Skip header
        if (file.ReadBytes(buffer.data(), buffer.size()) != buffer.size()) {
            throw std::runtime_error("Could not read from file at " + path);
        }
        decompressed = Common::Compression::DecompressDataZSTD(buffer);
    }
    std::istringstream sstream{
        std::string{reinterpret_cast<char*>(decompressed.data()), decompressed.size()},
        std::ios_base::binary};
    decompressed.clear();

    // Deserialize
    iarchive ia{sstream};
    ia&* this;
}

} // namespace Core

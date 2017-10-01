// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cinttypes>
#include <cryptopp/sha.h>
#include "common/alignment.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/file_sys/title_metadata.h"
#include "core/loader/loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

static u32 GetSignatureSize(u32 signature_type) {
    switch (signature_type) {
    case Rsa4096Sha1:
    case Rsa4096Sha256:
        return 0x200;

    case Rsa2048Sha1:
    case Rsa2048Sha256:
        return 0x100;

    case EllipticSha1:
    case EcdsaSha256:
        return 0x3C;
    }
}

Loader::ResultStatus TitleMetadata::Load() {
    FileUtil::IOFile file(filepath, "rb");
    if (!file.IsOpen())
        return Loader::ResultStatus::Error;

    if (!file.ReadBytes(&signature_type, sizeof(u32_be)))
        return Loader::ResultStatus::Error;

    // Signature lengths are variable, and the body follows the signature
    u32 signature_size = GetSignatureSize(signature_type);

    tmd_signature.resize(signature_size);
    if (!file.ReadBytes(&tmd_signature[0], signature_size))
        return Loader::ResultStatus::Error;

    // The TMD body start position is rounded to the nearest 0x40 after the signature
    size_t body_start = Common::AlignUp(signature_size + sizeof(u32), 0x40);
    file.Seek(body_start, SEEK_SET);

    // Read our TMD body, then load the amount of ContentChunks specified
    if (file.ReadBytes(&tmd_body, sizeof(TitleMetadata::Body)) != sizeof(TitleMetadata::Body))
        return Loader::ResultStatus::Error;

    for (u16 i = 0; i < tmd_body.content_count; i++) {
        ContentChunk chunk;
        if (file.ReadBytes(&chunk, sizeof(ContentChunk)) == sizeof(ContentChunk)) {
            tmd_chunks.push_back(chunk);
        } else {
            LOG_ERROR(Service_FS, "Malformed TMD %s, failed to load content chunk index %u!",
                      filepath.c_str(), i);
            return Loader::ResultStatus::ErrorInvalidFormat;
        }
    }

    return Loader::ResultStatus::Success;
}

Loader::ResultStatus TitleMetadata::Save() {
    FileUtil::IOFile file(filepath, "wb");
    if (!file.IsOpen())
        return Loader::ResultStatus::Error;

    if (!file.WriteBytes(&signature_type, sizeof(u32_be)))
        return Loader::ResultStatus::Error;

    // Signature lengths are variable, and the body follows the signature
    u32 signature_size = GetSignatureSize(signature_type);

    if (!file.WriteBytes(tmd_signature.data(), signature_size))
        return Loader::ResultStatus::Error;

    // The TMD body start position is rounded to the nearest 0x40 after the signature
    size_t body_start = Common::AlignUp(signature_size + sizeof(u32), 0x40);
    file.Seek(body_start, SEEK_SET);

    // Update our TMD body values and hashes
    tmd_body.content_count = static_cast<u16>(tmd_chunks.size());

    // TODO(shinyquagsire23): Do TMDs with more than one contentinfo exist?
    // For now we'll just adjust the first index to hold all content chunks
    // and ensure that no further content info data exists.
    tmd_body.contentinfo = {};
    tmd_body.contentinfo[0].index = 0;
    tmd_body.contentinfo[0].command_count = static_cast<u16>(tmd_chunks.size());

    CryptoPP::SHA256 chunk_hash;
    for (u16 i = 0; i < tmd_body.content_count; i++) {
        chunk_hash.Update(reinterpret_cast<u8*>(&tmd_chunks[i]), sizeof(ContentChunk));
    }
    chunk_hash.Final(tmd_body.contentinfo[0].hash.data());

    CryptoPP::SHA256 contentinfo_hash;
    for (size_t i = 0; i < tmd_body.contentinfo.size(); i++) {
        chunk_hash.Update(reinterpret_cast<u8*>(&tmd_body.contentinfo[i]), sizeof(ContentInfo));
    }
    chunk_hash.Final(tmd_body.contentinfo_hash.data());

    // Write our TMD body, then write each of our ContentChunks
    if (file.WriteBytes(&tmd_body, sizeof(TitleMetadata::Body)) != sizeof(TitleMetadata::Body))
        return Loader::ResultStatus::Error;

    for (u16 i = 0; i < tmd_body.content_count; i++) {
        ContentChunk chunk = tmd_chunks[i];
        if (file.WriteBytes(&chunk, sizeof(ContentChunk)) != sizeof(ContentChunk))
            return Loader::ResultStatus::Error;
    }

    return Loader::ResultStatus::Success;
}

u64 TitleMetadata::GetTitleID() const {
    return tmd_body.title_id;
}

u32 TitleMetadata::GetTitleType() const {
    return tmd_body.title_type;
}

u16 TitleMetadata::GetTitleVersion() const {
    return tmd_body.title_version;
}

u64 TitleMetadata::GetSystemVersion() const {
    return tmd_body.system_version;
}

size_t TitleMetadata::GetContentCount() const {
    return tmd_chunks.size();
}

u32 TitleMetadata::GetBootContentID() const {
    return tmd_chunks[TMDContentIndex::Main].id;
}

u32 TitleMetadata::GetManualContentID() const {
    return tmd_chunks[TMDContentIndex::Manual].id;
}

u32 TitleMetadata::GetDLPContentID() const {
    return tmd_chunks[TMDContentIndex::DLP].id;
}

void TitleMetadata::SetTitleID(u64 title_id) {
    tmd_body.title_id = title_id;
}

void TitleMetadata::SetTitleType(u32 type) {
    tmd_body.title_type = type;
}

void TitleMetadata::SetTitleVersion(u16 version) {
    tmd_body.title_version = version;
}

void TitleMetadata::SetSystemVersion(u64 version) {
    tmd_body.system_version = version;
}

void TitleMetadata::AddContentChunk(const ContentChunk& chunk) {
    tmd_chunks.push_back(chunk);
}

void TitleMetadata::Print() const {
    LOG_DEBUG(Service_FS, "%s - %u chunks", filepath.c_str(),
              static_cast<u32>(tmd_body.content_count));

    // Content info describes ranges of content chunks
    LOG_DEBUG(Service_FS, "Content info:");
    for (size_t i = 0; i < tmd_body.contentinfo.size(); i++) {
        if (tmd_body.contentinfo[i].command_count == 0)
            break;

        LOG_DEBUG(Service_FS, "    Index %04X, Command Count %04X",
                  static_cast<u32>(tmd_body.contentinfo[i].index),
                  static_cast<u32>(tmd_body.contentinfo[i].command_count));
    }

    // For each content info, print their content chunk range
    for (size_t i = 0; i < tmd_body.contentinfo.size(); i++) {
        u16 index = static_cast<u16>(tmd_body.contentinfo[i].index);
        u16 count = static_cast<u16>(tmd_body.contentinfo[i].command_count);

        if (count == 0)
            continue;

        LOG_DEBUG(Service_FS, "Content chunks for content info index %zu:", i);
        for (u16 j = index; j < index + count; j++) {
            // Don't attempt to print content we don't have
            if (j > tmd_body.content_count)
                break;

            const ContentChunk& chunk = tmd_chunks[j];
            LOG_DEBUG(Service_FS, "    ID %08X, Index %04X, Type %04x, Size %016" PRIX64,
                      static_cast<u32>(chunk.id), static_cast<u32>(chunk.index),
                      static_cast<u32>(chunk.type), static_cast<u64>(chunk.size));
        }
    }
}
} // namespace FileSys

// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <fmt/format.h>
#include "common/file_util.h"
#include "common/texture.h"
#include "core.h"
#include "core/custom_tex_cache.h"

namespace Core {
CustomTexCache::CustomTexCache() = default;

CustomTexCache::~CustomTexCache() = default;

bool CustomTexCache::IsTextureDumped(u64 hash) const {
    return dumped_textures.count(hash);
}

void CustomTexCache::SetTextureDumped(const u64 hash) {
    dumped_textures.insert(hash);
}

bool CustomTexCache::IsTextureCached(u64 hash) const {
    return custom_textures.count(hash);
}

const CustomTexInfo& CustomTexCache::LookupTexture(u64 hash) const {
    return custom_textures.at(hash);
}

void CustomTexCache::CacheTexture(u64 hash, const std::vector<u8>& tex, u32 width, u32 height) {
    custom_textures[hash] = {width, height, tex};
}

void CustomTexCache::AddTexturePath(u64 hash, const std::string& path) {
    if (custom_texture_paths.count(hash))
        LOG_ERROR(Core, "Textures {} and {} conflict!", custom_texture_paths[hash].path, path);
    else
        custom_texture_paths[hash] = {path, hash};
}

void CustomTexCache::FindCustomTextures() {
    // Custom textures are currently stored as
    // [TitleID]/tex1_[width]x[height]_[64-bit hash]_[format].png

    const std::string load_path =
        fmt::format("{}textures/{:016X}/", FileUtil::GetUserPath(FileUtil::UserPath::LoadDir),
                    Core::System::GetInstance().Kernel().GetCurrentProcess()->codeset->program_id);

    if (FileUtil::Exists(load_path)) {
        FileUtil::FSTEntry texture_dir;
        std::vector<FileUtil::FSTEntry> textures;
        // 64 nested folders should be plenty for most cases
        FileUtil::ScanDirectoryTree(load_path, texture_dir, 64);
        FileUtil::GetAllFilesFromNestedEntries(texture_dir, textures);

        for (const auto& file : textures) {
            if (file.isDirectory)
                continue;
            if (file.virtualName.substr(0, 5) != "tex1_")
                continue;

            u32 width;
            u32 height;
            u64 hash;
            u32 format; // unused
            // TODO: more modern way of doing this
            if (std::sscanf(file.virtualName.c_str(), "tex1_%ux%u_%llX_%u.png", &width, &height,
                            &hash, &format) == 4) {
                AddTexturePath(hash, file.physicalName);
            }
        }
    }
}

void CustomTexCache::PreloadTextures() {
    for (const auto& path : custom_texture_paths) {
        const auto& image_interface = Core::System::GetInstance().GetImageInterface();
        const auto& path_info = path.second;
        Core::CustomTexInfo tex_info;
        if (image_interface->DecodePNG(tex_info.tex, tex_info.width, tex_info.height,
                                       path_info.path)) {
            // Make sure the texture size is a power of 2
            std::bitset<32> width_bits(tex_info.width);
            std::bitset<32> height_bits(tex_info.height);
            if (width_bits.count() == 1 && height_bits.count() == 1) {
                LOG_DEBUG(Render_OpenGL, "Loaded custom texture from {}", path_info.path);
                Common::FlipRGBA8Texture(tex_info.tex, tex_info.width, tex_info.height);
                CacheTexture(path_info.hash, tex_info.tex, tex_info.width, tex_info.height);
            } else {
                LOG_ERROR(Render_OpenGL, "Texture {} size is not a power of 2", path_info.path);
            }
        } else {
            LOG_ERROR(Render_OpenGL, "Failed to load custom texture {}", path_info.path);
        }
    }
}

bool CustomTexCache::CustomTextureExists(u64 hash) const {
    return custom_texture_paths.count(hash);
}

const CustomTexPathInfo& CustomTexCache::LookupTexturePathInfo(u64 hash) const {
    return custom_texture_paths.at(hash);
}

bool CustomTexCache::IsTexturePathMapEmpty() const {
    return custom_texture_paths.size() == 0;
}
} // namespace Core

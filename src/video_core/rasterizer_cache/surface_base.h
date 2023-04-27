// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <boost/icl/interval_set.hpp>
#include "video_core/rasterizer_cache/surface_params.h"

namespace VideoCore {

using SurfaceRegions = boost::icl::interval_set<PAddr, std::less, SurfaceInterval>;

struct Material;

class SurfaceBase : public SurfaceParams {
public:
    SurfaceBase(const SurfaceParams& params);
    ~SurfaceBase();

    /// Returns true when this surface can be used to fill the fill_interval of dest_surface
    bool CanFill(const SurfaceParams& dest_surface, SurfaceInterval fill_interval) const;

    /// Returns true when surface can validate copy_interval of dest_surface
    bool CanCopy(const SurfaceParams& dest_surface, SurfaceInterval copy_interval) const;

    /// Returns the region of the biggest valid rectange within interval
    SurfaceInterval GetCopyableInterval(const SurfaceParams& params) const;

    /// Returns the clear value used to validate another surface from this fill surface
    ClearValue MakeClearValue(PAddr copy_addr, PixelFormat dst_format);

    /// Returns true if the surface contains a custom material with a normal map.
    bool HasNormalMap() const noexcept;

    u64 ModificationTick() const noexcept {
        return modification_tick;
    }

    bool IsCustom() const noexcept {
        return is_custom && custom_format != CustomPixelFormat::Invalid;
    }

    bool IsRegionValid(SurfaceInterval interval) const {
        return (invalid_regions.find(interval) == invalid_regions.end());
    }

    void MarkValid(SurfaceInterval interval) {
        invalid_regions.erase(interval);
        modification_tick++;
    }

    void MarkInvalid(SurfaceInterval interval) {
        invalid_regions.insert(interval);
        modification_tick++;
    }

    bool IsFullyInvalid() const {
        auto interval = GetInterval();
        return *invalid_regions.equal_range(interval).first == interval;
    }

private:
    /// Returns the fill buffer value starting from copy_addr
    std::array<u8, 4> MakeFillBuffer(PAddr copy_addr);

public:
    bool registered = false;
    bool is_custom = false;
    const Material* material = nullptr;
    SurfaceRegions invalid_regions;
    u32 fill_size = 0;
    std::array<u8, 4> fill_data;
    u64 modification_tick = 1;
};

} // namespace VideoCore

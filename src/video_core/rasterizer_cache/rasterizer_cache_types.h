// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once
#include <memory>
#include <set>
#include <tuple>
#include <boost/icl/interval_map.hpp>
#include <boost/icl/interval_set.hpp>
#include "common/common_types.h"
#include "common/math_util.h"

namespace OpenGL {

class CachedSurface;
using Surface = std::shared_ptr<CachedSurface>;

// Declare rasterizer interval types
using SurfaceInterval = boost::icl::right_open_interval<PAddr>;
using SurfaceSet = std::set<Surface>;
using SurfaceRegions = boost::icl::interval_set<PAddr, std::less, SurfaceInterval>;
using SurfaceMap = boost::icl::interval_map<PAddr, Surface, boost::icl::partial_absorber,
                                            std::less, boost::icl::inplace_plus,
                                            boost::icl::inter_section, SurfaceInterval>;
using SurfaceCache = boost::icl::interval_map<PAddr, SurfaceSet, boost::icl::partial_absorber,
                                              std::less, boost::icl::inplace_plus,
                                              boost::icl::inter_section, SurfaceInterval>;

static_assert(std::is_same<SurfaceRegions::interval_type, SurfaceCache::interval_type>() &&
              std::is_same<SurfaceMap::interval_type, SurfaceCache::interval_type>(),
              "Incorrect interval types");

using SurfaceRect_Tuple = std::tuple<Surface, Common::Rectangle<u32>>;
using SurfaceSurfaceRect_Tuple = std::tuple<Surface, Surface, Common::Rectangle<u32>>;
using PageMap = boost::icl::interval_map<u32, int>;


} // namespace OpenGL

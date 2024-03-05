// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "video_core/pica_types.h"

using Pica::f24;

TEST_CASE("Infinities", "[video_core][pica_float]") {
    REQUIRE(std::isinf(f24::FromFloat32(INFINITY).ToFloat32()));
    REQUIRE(std::isinf(f24::FromFloat32(1.e20f).ToFloat32()));
    REQUIRE(std::isinf(f24::FromFloat32(-1.e20f).ToFloat32()));
}

TEST_CASE("Subnormals", "[video_core][pica_float]") {
    REQUIRE(f24::FromFloat32(1e-20f).ToFloat32() == 0.f);
}

TEST_CASE("NaN", "[video_core][pica_float]") {
    const auto inf = f24::FromFloat32(INFINITY);
    const auto nan = f24::FromFloat32(NAN);

    REQUIRE(std::isnan(nan.ToFloat32()));
    REQUIRE(std::isnan((nan * f24::Zero()).ToFloat32()));
    REQUIRE(std::isnan((inf - inf).ToFloat32()));
    REQUIRE((inf * f24::Zero()).ToFloat32() == 0.f);
}

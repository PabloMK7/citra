// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <catch2/catch.hpp>
#include <glad/glad.h>

// This is not an actual test, but a work-around for issue #2183.
// If tests uses functions in core but doesn't explicitly use functions in glad, the linker of macOS
// will error about undefined references from video_core to glad. So we explicitly use a glad
// function here to shut up the linker.
TEST_CASE("glad fake test", "[dummy]") {
    REQUIRE(&gladLoadGL != nullptr);
}

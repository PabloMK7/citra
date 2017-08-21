// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/math_util.h"
#include "common/vector_math.h"
#include "video_core/regs_texturing.h"
#include "video_core/swrasterizer/texturing.h"

namespace Pica {
namespace Rasterizer {

using TevStageConfig = TexturingRegs::TevStageConfig;

int GetWrappedTexCoord(TexturingRegs::TextureConfig::WrapMode mode, int val, unsigned size) {
    switch (mode) {
    case TexturingRegs::TextureConfig::ClampToEdge2:
        // For negative coordinate, ClampToEdge2 behaves the same as Repeat
        if (val < 0) {
            return static_cast<int>(static_cast<unsigned>(val) % size);
        }
    // [[fallthrough]]
    case TexturingRegs::TextureConfig::ClampToEdge:
        val = std::max(val, 0);
        val = std::min(val, static_cast<int>(size) - 1);
        return val;

    case TexturingRegs::TextureConfig::ClampToBorder:
        return val;

    case TexturingRegs::TextureConfig::ClampToBorder2:
    // For ClampToBorder2, the case of positive coordinate beyond the texture size is already
    // handled outside. Here we only handle the negative coordinate in the same way as Repeat.
    case TexturingRegs::TextureConfig::Repeat2:
    case TexturingRegs::TextureConfig::Repeat3:
    case TexturingRegs::TextureConfig::Repeat:
        return static_cast<int>(static_cast<unsigned>(val) % size);

    case TexturingRegs::TextureConfig::MirroredRepeat: {
        unsigned int coord = (static_cast<unsigned>(val) % (2 * size));
        if (coord >= size)
            coord = 2 * size - 1 - coord;
        return static_cast<int>(coord);
    }

    default:
        LOG_ERROR(HW_GPU, "Unknown texture coordinate wrapping mode %x", (int)mode);
        UNIMPLEMENTED();
        return 0;
    }
};

Math::Vec3<u8> GetColorModifier(TevStageConfig::ColorModifier factor,
                                const Math::Vec4<u8>& values) {
    using ColorModifier = TevStageConfig::ColorModifier;

    switch (factor) {
    case ColorModifier::SourceColor:
        return values.rgb();

    case ColorModifier::OneMinusSourceColor:
        return (Math::Vec3<u8>(255, 255, 255) - values.rgb()).Cast<u8>();

    case ColorModifier::SourceAlpha:
        return values.aaa();

    case ColorModifier::OneMinusSourceAlpha:
        return (Math::Vec3<u8>(255, 255, 255) - values.aaa()).Cast<u8>();

    case ColorModifier::SourceRed:
        return values.rrr();

    case ColorModifier::OneMinusSourceRed:
        return (Math::Vec3<u8>(255, 255, 255) - values.rrr()).Cast<u8>();

    case ColorModifier::SourceGreen:
        return values.ggg();

    case ColorModifier::OneMinusSourceGreen:
        return (Math::Vec3<u8>(255, 255, 255) - values.ggg()).Cast<u8>();

    case ColorModifier::SourceBlue:
        return values.bbb();

    case ColorModifier::OneMinusSourceBlue:
        return (Math::Vec3<u8>(255, 255, 255) - values.bbb()).Cast<u8>();
    }

    UNREACHABLE();
};

u8 GetAlphaModifier(TevStageConfig::AlphaModifier factor, const Math::Vec4<u8>& values) {
    using AlphaModifier = TevStageConfig::AlphaModifier;

    switch (factor) {
    case AlphaModifier::SourceAlpha:
        return values.a();

    case AlphaModifier::OneMinusSourceAlpha:
        return 255 - values.a();

    case AlphaModifier::SourceRed:
        return values.r();

    case AlphaModifier::OneMinusSourceRed:
        return 255 - values.r();

    case AlphaModifier::SourceGreen:
        return values.g();

    case AlphaModifier::OneMinusSourceGreen:
        return 255 - values.g();

    case AlphaModifier::SourceBlue:
        return values.b();

    case AlphaModifier::OneMinusSourceBlue:
        return 255 - values.b();
    }

    UNREACHABLE();
};

Math::Vec3<u8> ColorCombine(TevStageConfig::Operation op, const Math::Vec3<u8> input[3]) {
    using Operation = TevStageConfig::Operation;

    switch (op) {
    case Operation::Replace:
        return input[0];

    case Operation::Modulate:
        return ((input[0] * input[1]) / 255).Cast<u8>();

    case Operation::Add: {
        auto result = input[0] + input[1];
        result.r() = std::min(255, result.r());
        result.g() = std::min(255, result.g());
        result.b() = std::min(255, result.b());
        return result.Cast<u8>();
    }

    case Operation::AddSigned: {
        // TODO(bunnei): Verify that the color conversion from (float) 0.5f to
        // (byte) 128 is correct
        auto result =
            input[0].Cast<int>() + input[1].Cast<int>() - Math::MakeVec<int>(128, 128, 128);
        result.r() = MathUtil::Clamp<int>(result.r(), 0, 255);
        result.g() = MathUtil::Clamp<int>(result.g(), 0, 255);
        result.b() = MathUtil::Clamp<int>(result.b(), 0, 255);
        return result.Cast<u8>();
    }

    case Operation::Lerp:
        return ((input[0] * input[2] +
                 input[1] * (Math::MakeVec<u8>(255, 255, 255) - input[2]).Cast<u8>()) /
                255)
            .Cast<u8>();

    case Operation::Subtract: {
        auto result = input[0].Cast<int>() - input[1].Cast<int>();
        result.r() = std::max(0, result.r());
        result.g() = std::max(0, result.g());
        result.b() = std::max(0, result.b());
        return result.Cast<u8>();
    }

    case Operation::MultiplyThenAdd: {
        auto result = (input[0] * input[1] + 255 * input[2].Cast<int>()) / 255;
        result.r() = std::min(255, result.r());
        result.g() = std::min(255, result.g());
        result.b() = std::min(255, result.b());
        return result.Cast<u8>();
    }

    case Operation::AddThenMultiply: {
        auto result = input[0] + input[1];
        result.r() = std::min(255, result.r());
        result.g() = std::min(255, result.g());
        result.b() = std::min(255, result.b());
        result = (result * input[2].Cast<int>()) / 255;
        return result.Cast<u8>();
    }
    case Operation::Dot3_RGB:
    case Operation::Dot3_RGBA: {
        // Not fully accurate.  Worst case scenario seems to yield a +/-3 error.  Some HW results
        // indicate that the per-component computation can't have a higher precision than 1/256,
        // while dot3_rgb((0x80,g0,b0), (0x7F,g1,b1)) and dot3_rgb((0x80,g0,b0), (0x80,g1,b1)) give
        // different results.
        int result = ((input[0].r() * 2 - 255) * (input[1].r() * 2 - 255) + 128) / 256 +
                     ((input[0].g() * 2 - 255) * (input[1].g() * 2 - 255) + 128) / 256 +
                     ((input[0].b() * 2 - 255) * (input[1].b() * 2 - 255) + 128) / 256;
        result = std::max(0, std::min(255, result));
        return {(u8)result, (u8)result, (u8)result};
    }
    default:
        LOG_ERROR(HW_GPU, "Unknown color combiner operation %d", (int)op);
        UNIMPLEMENTED();
        return {0, 0, 0};
    }
};

u8 AlphaCombine(TevStageConfig::Operation op, const std::array<u8, 3>& input) {
    switch (op) {
        using Operation = TevStageConfig::Operation;
    case Operation::Replace:
        return input[0];

    case Operation::Modulate:
        return input[0] * input[1] / 255;

    case Operation::Add:
        return std::min(255, input[0] + input[1]);

    case Operation::AddSigned: {
        // TODO(bunnei): Verify that the color conversion from (float) 0.5f to (byte) 128 is correct
        auto result = static_cast<int>(input[0]) + static_cast<int>(input[1]) - 128;
        return static_cast<u8>(MathUtil::Clamp<int>(result, 0, 255));
    }

    case Operation::Lerp:
        return (input[0] * input[2] + input[1] * (255 - input[2])) / 255;

    case Operation::Subtract:
        return std::max(0, (int)input[0] - (int)input[1]);

    case Operation::MultiplyThenAdd:
        return std::min(255, (input[0] * input[1] + 255 * input[2]) / 255);

    case Operation::AddThenMultiply:
        return (std::min(255, (input[0] + input[1])) * input[2]) / 255;

    default:
        LOG_ERROR(HW_GPU, "Unknown alpha combiner operation %d", (int)op);
        UNIMPLEMENTED();
        return 0;
    }
};

} // namespace Rasterizer
} // namespace Pica

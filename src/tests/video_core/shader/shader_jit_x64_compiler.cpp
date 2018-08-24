// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <memory>
#include <catch2/catch.hpp>
#include <nihstro/inline_assembly.h>
#include "video_core/shader/shader_jit_x64_compiler.h"

using float24 = Pica::float24;
using JitShader = Pica::Shader::JitShader;

using DestRegister = nihstro::DestRegister;
using OpCode = nihstro::OpCode;
using SourceRegister = nihstro::SourceRegister;

static std::unique_ptr<JitShader> CompileShader(std::initializer_list<nihstro::InlineAsm> code) {
    const auto shbin = nihstro::InlineAsm::CompileToRawBinary(code);

    std::array<u32, Pica::Shader::MAX_PROGRAM_CODE_LENGTH> program_code{};
    std::array<u32, Pica::Shader::MAX_SWIZZLE_DATA_LENGTH> swizzle_data{};

    std::transform(shbin.program.begin(), shbin.program.end(), program_code.begin(),
                   [](const auto& x) { return x.hex; });
    std::transform(shbin.swizzle_table.begin(), shbin.swizzle_table.end(), swizzle_data.begin(),
                   [](const auto& x) { return x.hex; });

    auto shader = std::make_unique<JitShader>();
    shader->Compile(&program_code, &swizzle_data);

    return shader;
}

class ShaderTest {
public:
    explicit ShaderTest(std::initializer_list<nihstro::InlineAsm> code)
        : shader(CompileShader(code)) {}

    float Run(float input) {
        Pica::Shader::ShaderSetup shader_setup;
        Pica::Shader::UnitState shader_unit;

        shader_unit.registers.input[0].x = float24::FromFloat32(input);
        shader->Run(shader_setup, shader_unit, 0);
        return shader_unit.registers.output[0].x.ToFloat32();
    }

public:
    std::unique_ptr<JitShader> shader;
};

TEST_CASE("LG2", "[video_core][shader][shader_jit]") {
    const auto sh_input = SourceRegister::MakeInput(0);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader = ShaderTest({
        // clang-format off
        {OpCode::Id::LG2, sh_output, sh_input},
        {OpCode::Id::END},
        // clang-format on
    });

    REQUIRE(std::isnan(shader.Run(NAN)));
    REQUIRE(std::isnan(shader.Run(-1.f)));
    REQUIRE(std::isinf(shader.Run(0.f)));
    REQUIRE(shader.Run(4.f) == Approx(2.f));
    REQUIRE(shader.Run(64.f) == Approx(6.f));
    REQUIRE(shader.Run(1.e24f) == Approx(79.7262742773f));
}

TEST_CASE("EX2", "[video_core][shader][shader_jit]") {
    const auto sh_input = SourceRegister::MakeInput(0);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader = ShaderTest({
        // clang-format off
        {OpCode::Id::EX2, sh_output, sh_input},
        {OpCode::Id::END},
        // clang-format on
    });

    REQUIRE(std::isnan(shader.Run(NAN)));
    REQUIRE(shader.Run(-800.f) == Approx(0.f));
    REQUIRE(shader.Run(0.f) == Approx(1.f));
    REQUIRE(shader.Run(2.f) == Approx(4.f));
    REQUIRE(shader.Run(6.f) == Approx(64.f));
    REQUIRE(shader.Run(79.7262742773f) == Approx(1.e24f));
    REQUIRE(std::isinf(shader.Run(800.f)));
}

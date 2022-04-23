// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <memory>
#include <catch2/catch.hpp>
#include <nihstro/inline_assembly.h>
#include "video_core/shader/shader_interpreter.h"
#include "video_core/shader/shader_jit_x64_compiler.h"

using float24 = Pica::float24;
using JitShader = Pica::Shader::JitShader;
using ShaderInterpreter = Pica::Shader::InterpreterEngine;

using DestRegister = nihstro::DestRegister;
using OpCode = nihstro::OpCode;
using SourceRegister = nihstro::SourceRegister;
using Type = nihstro::InlineAsm::Type;

static std::unique_ptr<Pica::Shader::ShaderSetup> CompileShaderSetup(
    std::initializer_list<nihstro::InlineAsm> code) {
    const auto shbin = nihstro::InlineAsm::CompileToRawBinary(code);

    auto shader = std::make_unique<Pica::Shader::ShaderSetup>();

    std::transform(shbin.program.begin(), shbin.program.end(), shader->program_code.begin(),
                   [](const auto& x) { return x.hex; });
    std::transform(shbin.swizzle_table.begin(), shbin.swizzle_table.end(),
                   shader->swizzle_data.begin(), [](const auto& x) { return x.hex; });

    return shader;
}

class ShaderTest {
public:
    explicit ShaderTest(std::initializer_list<nihstro::InlineAsm> code)
        : shader_setup(CompileShaderSetup(code)) {
        shader_jit.Compile(&shader_setup->program_code, &shader_setup->swizzle_data);
    }

    float Run(float input) {
        Pica::Shader::UnitState shader_unit;
        RunJit(shader_unit, input);
        return shader_unit.registers.output[0].x.ToFloat32();
    }

    void RunJit(Pica::Shader::UnitState& shader_unit, float input) {
        shader_unit.registers.input[0].x = float24::FromFloat32(input);
        shader_unit.registers.temporary[0].x = float24::FromFloat32(0);
        shader_jit.Run(*shader_setup, shader_unit, 0);
    }

    void RunInterpreter(Pica::Shader::UnitState& shader_unit, float input) {
        shader_unit.registers.input[0].x = float24::FromFloat32(input);
        shader_unit.registers.temporary[0].x = float24::FromFloat32(0);
        shader_interpreter.Run(*shader_setup, shader_unit);
    }

public:
    JitShader shader_jit;
    ShaderInterpreter shader_interpreter;
    std::unique_ptr<Pica::Shader::ShaderSetup> shader_setup;
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

TEST_CASE("Nested Loop", "[video_core][shader][shader_jit]") {
    const auto sh_input = SourceRegister::MakeInput(0);
    const auto sh_temp = SourceRegister::MakeTemporary(0);
    const auto sh_output = DestRegister::MakeOutput(0);

    std::array<Common::Vec4<u8>, 2> loop_parms{Common::Vec4<u8>{4, 0, 1, 0},
                                               Common::Vec4<u8>{4, 0, 1, 0}};

    auto shader_test = ShaderTest({
        // clang-format off
        {OpCode::Id::LOOP, 0},
            {OpCode::Id::LOOP, 1},
                {OpCode::Id::ADD, sh_temp, sh_temp, sh_input},
            {Type::EndLoop},
        {Type::EndLoop},
        {OpCode::Id::MOV, sh_output, sh_temp},
        {OpCode::Id::END},
        // clang-format on
    });

    shader_test.shader_setup->uniforms.i[0] = loop_parms[0];
    shader_test.shader_setup->uniforms.i[1] = loop_parms[0];

    const auto run_test_helper = [&shader_test](float input) {
        Pica::Shader::UnitState shader_unit_jit;
        Pica::Shader::UnitState shader_unit_inter;
        shader_test.RunJit(shader_unit_jit, input);
        shader_test.RunInterpreter(shader_unit_inter, input);

        REQUIRE(shader_unit_jit.registers.output[0].x.ToFloat32() ==
                Approx(shader_unit_inter.registers.output[0].x.ToFloat32()));
        REQUIRE(shader_unit_jit.address_registers[2] == shader_unit_inter.address_registers[2]);
    };
    {
        // Sanity check
        Pica::Shader::UnitState shader_unit_jit;
        shader_test.RunJit(shader_unit_jit, 1.0f);
        REQUIRE(shader_unit_jit.address_registers[2] == 6);
        REQUIRE(shader_unit_jit.registers.output[0].x.ToFloat32() == Approx(25.0f));

        Pica::Shader::UnitState shader_unit_inter;
        shader_test.RunInterpreter(shader_unit_inter, 2.0f);
        REQUIRE(shader_unit_inter.address_registers[2] == 6);
        REQUIRE(shader_unit_inter.registers.output[0].x.ToFloat32() == Approx(50.0f));
    }
    run_test_helper(-5.f);
    run_test_helper(0.f);
    run_test_helper(2.f);
    run_test_helper(6.f);
    run_test_helper(79.7262742773f);
}

TEST_CASE("Nested Loop Randomized", "[video_core][shader][shader_jit]") {
    const auto sh_input = SourceRegister::MakeInput(0);
    const auto sh_temp = SourceRegister::MakeTemporary(0);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader_test = ShaderTest({
        // clang-format off
        {OpCode::Id::LOOP, 0},
            {OpCode::Id::LOOP, 1},
                 {OpCode::Id::LOOP, 2},
                    {OpCode::Id::LOOP, 3},
                        {OpCode::Id::ADD, sh_temp, sh_temp, sh_input},
                    {Type::EndLoop},
                {Type::EndLoop},
            {Type::EndLoop},
        {Type::EndLoop},

        {OpCode::Id::MOV, sh_output, sh_temp},
        {OpCode::Id::END},
        // clang-format on
    });

    const auto generate_loop_parms = [] {
        u8 iterations = 1 + rand();
        u8 initial = 1 + rand();
        u8 increment = 1 + rand();

        Common::Vec4<u8> loop_parm{iterations, initial, increment, 0};
        return Common::Vec4<u8>{iterations, initial, increment, 0};
    };

    const auto run_test_helper = [&shader_test](float input) {
        Pica::Shader::UnitState shader_unit_jit;
        Pica::Shader::UnitState shader_unit_inter;
        shader_test.RunJit(shader_unit_jit, input);
        shader_test.RunInterpreter(shader_unit_inter, input);

        REQUIRE(shader_unit_jit.registers.output[0].x.ToFloat32() ==
                Approx(shader_unit_inter.registers.output[0].x.ToFloat32()));
        REQUIRE(shader_unit_jit.address_registers[2] == shader_unit_inter.address_registers[2]);
    };

    srand(time(0));
    for (int i = 0; i < 10; i++) {
        shader_test.shader_setup->uniforms.i[0] = generate_loop_parms();
        shader_test.shader_setup->uniforms.i[1] = generate_loop_parms();
        shader_test.shader_setup->uniforms.i[2] = generate_loop_parms();
        shader_test.shader_setup->uniforms.i[3] = generate_loop_parms();
        float input = -(RAND_MAX / 2) + rand();
        run_test_helper(input);
    }
}

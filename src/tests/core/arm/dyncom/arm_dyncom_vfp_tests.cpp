// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <catch2/catch.hpp>

#include "core/arm/dyncom/arm_dyncom.h"
#include "core/core_timing.h"
#include "tests/core/arm/arm_test_common.h"

namespace ArmTests {

struct VfpTestCase {
    u32 initial_fpscr;
    u32 a;
    u32 b;
    u32 result;
    u32 final_fpscr;
};

TEST_CASE("ARM_DynCom (vfp): vadd", "[arm_dyncom]") {
    TestEnvironment test_env(false);
    test_env.SetMemory32(0, 0xEE321A03); // vadd.f32 s2, s4, s6
    test_env.SetMemory32(4, 0xEAFFFFFE); // b +#0

    ARM_DynCom dyncom(USER32MODE);

    std::vector<VfpTestCase> test_cases{{
#include "vfp_vadd_f32.inc"
    }};

    for (const auto& test_case : test_cases) {
        dyncom.SetPC(0);
        dyncom.SetVFPSystemReg(VFP_FPSCR, test_case.initial_fpscr);
        dyncom.SetVFPReg(4, test_case.a);
        dyncom.SetVFPReg(6, test_case.b);
        dyncom.Step();
        if (dyncom.GetVFPReg(2) != test_case.result ||
            dyncom.GetVFPSystemReg(VFP_FPSCR) != test_case.final_fpscr) {
            printf("f: %x\n", test_case.initial_fpscr);
            printf("a: %x\n", test_case.a);
            printf("b: %x\n", test_case.b);
            printf("c: %x (%x)\n", dyncom.GetVFPReg(2), test_case.result);
            printf("f: %x (%x)\n", dyncom.GetVFPSystemReg(VFP_FPSCR), test_case.final_fpscr);
            FAIL();
        }
    }
}

} // namespace ArmTests
// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <initializer_list>

#include <common/common_types.h>

#include "math.h"
#include "pica.h"

namespace Pica {

namespace VertexShader {

struct InputVertex {
    Math::Vec4<float24> attr[16];
};

struct OutputVertex {
    OutputVertex() = default;

    // VS output attributes
    Math::Vec4<float24> pos;
    Math::Vec4<float24> dummy; // quaternions (not implemented, yet)
    Math::Vec4<float24> color;
    Math::Vec2<float24> tc0;

    // Padding for optimal alignment
    float24 pad[14];

    // Attributes used to store intermediate results

    // position after perspective divide
    Math::Vec3<float24> screenpos;
    float24 pad2;

    // Linear interpolation
    // factor: 0=this, 1=vtx
    void Lerp(float24 factor, const OutputVertex& vtx) {
        pos = pos * factor + vtx.pos * (float24::FromFloat32(1) - factor);

        // TODO: Should perform perspective correct interpolation here...
        tc0 = tc0 * factor + vtx.tc0 * (float24::FromFloat32(1) - factor);

        screenpos = screenpos * factor + vtx.screenpos * (float24::FromFloat32(1) - factor);

        color = color * factor + vtx.color * (float24::FromFloat32(1) - factor);
    }

    // Linear interpolation
    // factor: 0=v0, 1=v1
    static OutputVertex Lerp(float24 factor, const OutputVertex& v0, const OutputVertex& v1) {
        OutputVertex ret = v0;
        ret.Lerp(factor, v1);
        return ret;
    }
};
static_assert(std::is_pod<OutputVertex>::value, "Structure is not POD");
static_assert(sizeof(OutputVertex) == 32 * sizeof(float), "OutputVertex has invalid size");

union Instruction {
    enum class OpCode : u32 {
        ADD = 0x0,
        DP3 = 0x1,
        DP4 = 0x2,

        MUL = 0x8,

        MAX = 0xC,
        MIN = 0xD,
        RCP = 0xE,
        RSQ = 0xF,

        MOV = 0x13,

        RET = 0x21,
        FLS = 0x22, // Flush
        CALL = 0x24,
    };

    std::string GetOpCodeName() const {
        std::map<OpCode, std::string> map = {
            { OpCode::ADD, "ADD" },
            { OpCode::DP3, "DP3" },
            { OpCode::DP4, "DP4" },
            { OpCode::MUL, "MUL" },
            { OpCode::MAX, "MAX" },
            { OpCode::MIN, "MIN" },
            { OpCode::RCP, "RCP" },
            { OpCode::RSQ, "RSQ" },
            { OpCode::MOV, "MOV" },
            { OpCode::RET, "RET" },
            { OpCode::FLS, "FLS" },
        };
        auto it = map.find(opcode);
        if (it == map.end())
            return "UNK";
        else
            return it->second;
    }

    u32 hex;

    BitField<0x1a, 0x6, OpCode> opcode;

    // General notes:
    //
    // When two input registers are used, one of them uses a 5-bit index while the other
    // one uses a 7-bit index. This is because at most one floating point uniform may be used
    // as an input.


    // Format used e.g. by arithmetic instructions and comparisons
    // "src1" and "src2" specify register indices (i.e. indices referring to groups of 4 floats),
    // while "dest" addresses individual floats.
    union {
        BitField<0x00, 0x5, u32> operand_desc_id;

        template<class BitFieldType>
        struct SourceRegister : BitFieldType {
            enum RegisterType {
                Input,
                Temporary,
                FloatUniform
            };

            RegisterType GetRegisterType() const {
                if (BitFieldType::Value() < 0x10)
                    return Input;
                else if (BitFieldType::Value() < 0x20)
                    return Temporary;
                else
                    return FloatUniform;
            }

            int GetIndex() const {
                if (GetRegisterType() == Input)
                    return BitFieldType::Value();
                else if (GetRegisterType() == Temporary)
                    return BitFieldType::Value() - 0x10;
                else // if (GetRegisterType() == FloatUniform)
                    return BitFieldType::Value() - 0x20;
            }

            std::string GetRegisterName() const {
                std::map<RegisterType, std::string> type = {
                    { Input, "i" },
                    { Temporary, "t" },
                    { FloatUniform, "f" },
                };
                return type[GetRegisterType()] + std::to_string(GetIndex());
            }
        };

        SourceRegister<BitField<0x07, 0x5, u32>> src2;
        SourceRegister<BitField<0x0c, 0x7, u32>> src1;

        struct : BitField<0x15, 0x5, u32>
        {
            enum RegisterType {
                Output,
                Temporary,
                Unknown
            };
            RegisterType GetRegisterType() const {
                if (Value() < 0x8)
                    return Output;
                else if (Value() < 0x10)
                    return Unknown;
                else
                    return Temporary;
            }
            int GetIndex() const {
                if (GetRegisterType() == Output)
                    return Value();
                else if (GetRegisterType() == Temporary)
                    return Value() - 0x10;
                else
                    return Value();
            }
            std::string GetRegisterName() const {
                std::map<RegisterType, std::string> type = {
                    { Output, "o" },
                    { Temporary, "t" },
                    { Unknown, "u" }
                };
                return type[GetRegisterType()] + std::to_string(GetIndex());
            }
        } dest;
    } common;

    // Format used for flow control instructions ("if")
    union {
        BitField<0x00, 0x8, u32> num_instructions;
        BitField<0x0a, 0xc, u32> offset_words;
    } flow_control;
};
static_assert(std::is_standard_layout<Instruction>::value, "Structure is not using standard layout!");

union SwizzlePattern {
    u32 hex;

    enum class Selector : u32 {
        x = 0,
        y = 1,
        z = 2,
        w = 3
    };

    Selector GetSelectorSrc1(int comp) const {
        Selector selectors[] = {
            src1_selector_0, src1_selector_1, src1_selector_2, src1_selector_3
        };
        return selectors[comp];
    }

    Selector GetSelectorSrc2(int comp) const {
        Selector selectors[] = {
            src2_selector_0, src2_selector_1, src2_selector_2, src2_selector_3
        };
        return selectors[comp];
    }

    bool DestComponentEnabled(int i) const {
        return (dest_mask & (0x8 >> i)) != 0;
    }

    std::string SelectorToString(bool src2) const {
        std::map<Selector, std::string> map = {
            { Selector::x, "x" },
            { Selector::y, "y" },
            { Selector::z, "z" },
            { Selector::w, "w" }
        };
        std::string ret;
        for (int i = 0; i < 4; ++i) {
            ret += map.at(src2 ? GetSelectorSrc2(i) : GetSelectorSrc1(i));
        }
        return ret;
    }

    std::string DestMaskToString() const {
        std::string ret;
        for (int i = 0; i < 4; ++i) {
            if (!DestComponentEnabled(i))
                ret += "_";
            else
                ret += "xyzw"[i];
        }
        return ret;
    }

    // Components of "dest" that should be written to: LSB=dest.w, MSB=dest.x
    BitField< 0, 4, u32> dest_mask;

    BitField< 4, 1, u32> negate; // negates src1

    BitField< 5, 2, Selector> src1_selector_3;
    BitField< 7, 2, Selector> src1_selector_2;
    BitField< 9, 2, Selector> src1_selector_1;
    BitField<11, 2, Selector> src1_selector_0;

    BitField<14, 2, Selector> src2_selector_3;
    BitField<16, 2, Selector> src2_selector_2;
    BitField<18, 2, Selector> src2_selector_1;
    BitField<20, 2, Selector> src2_selector_0;

    BitField<31, 1, u32> flag; // not sure what this means, maybe it's the sign?
};

void SubmitShaderMemoryChange(u32 addr, u32 value);
void SubmitSwizzleDataChange(u32 addr, u32 value);

OutputVertex RunShader(const InputVertex& input, int num_attributes);

Math::Vec4<float24>& GetFloatUniform(u32 index);

} // namespace

} // namespace


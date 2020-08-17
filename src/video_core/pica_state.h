// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <boost/serialization/array.hpp>
#include <boost/serialization/split_member.hpp>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/vector_math.h"
#include "core/memory.h"
#include "video_core/geometry_pipeline.h"
#include "video_core/primitive_assembly.h"
#include "video_core/regs.h"
#include "video_core/shader/shader.h"
#include "video_core/video_core.h"

// Boost::serialization doesn't like union types for some reason,
// so we need to mark arrays of union values with a special serialization method
template <typename Value, size_t Size>
struct UnionArray : public std::array<Value, Size> {
private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        static_assert(sizeof(Value) == sizeof(u32));
        ar&* static_cast<u32(*)[Size]>(static_cast<void*>(this->data()));
    }
    friend class boost::serialization::access;
};

namespace Pica {

/// Struct used to describe current Pica state
struct State {
    State();
    void Reset();

    /// Pica registers
    Regs regs;

    Shader::ShaderSetup vs;
    Shader::ShaderSetup gs;

    Shader::AttributeBuffer input_default_attributes;

    struct ProcTex {
        union ValueEntry {
            u32 raw;

            // LUT value, encoded as 12-bit fixed point, with 12 fraction bits
            BitField<0, 12, u32> value; // 0.0.12 fixed point

            // Difference between two entry values. Used for efficient interpolation.
            // 0.0.12 fixed point with two's complement. The range is [-0.5, 0.5).
            // Note: the type of this is different from the one of lighting LUT
            BitField<12, 12, s32> difference;

            float ToFloat() const {
                return static_cast<float>(value) / 4095.f;
            }

            float DiffToFloat() const {
                return static_cast<float>(difference) / 4095.f;
            }
        };

        union ColorEntry {
            u32 raw;
            BitField<0, 8, u32> r;
            BitField<8, 8, u32> g;
            BitField<16, 8, u32> b;
            BitField<24, 8, u32> a;

            Common::Vec4<u8> ToVector() const {
                return {static_cast<u8>(r), static_cast<u8>(g), static_cast<u8>(b),
                        static_cast<u8>(a)};
            }
        };

        union ColorDifferenceEntry {
            u32 raw;
            BitField<0, 8, s32> r; // half of the difference between two ColorEntry
            BitField<8, 8, s32> g;
            BitField<16, 8, s32> b;
            BitField<24, 8, s32> a;

            Common::Vec4<s32> ToVector() const {
                return Common::Vec4<s32>{r, g, b, a} * 2;
            }
        };

        UnionArray<ValueEntry, 128> noise_table;
        UnionArray<ValueEntry, 128> color_map_table;
        UnionArray<ValueEntry, 128> alpha_map_table;
        UnionArray<ColorEntry, 256> color_table;
        UnionArray<ColorDifferenceEntry, 256> color_diff_table;

    private:
        friend class boost::serialization::access;
        template <class Archive>
        void serialize(Archive& ar, const unsigned int file_version) {
            ar& noise_table;
            ar& color_map_table;
            ar& alpha_map_table;
            ar& color_table;
            ar& color_diff_table;
        }
    } proctex;

    struct Lighting {
        union LutEntry {
            // Used for raw access
            u32 raw;

            // LUT value, encoded as 12-bit fixed point, with 12 fraction bits
            BitField<0, 12, u32> value; // 0.0.12 fixed point

            // Used for efficient interpolation.
            BitField<12, 11, u32> difference; // 0.0.11 fixed point
            BitField<23, 1, u32> neg_difference;

            float ToFloat() const {
                return static_cast<float>(value) / 4095.f;
            }

            float DiffToFloat() const {
                float diff = static_cast<float>(difference) / 2047.f;
                return neg_difference ? -diff : diff;
            }

            template <class Archive>
            void serialize(Archive& ar, const unsigned int file_version) {
                ar& raw;
            }
        };

        std::array<UnionArray<LutEntry, 256>, 24> luts;
    } lighting;

    struct {
        union LutEntry {
            // Used for raw access
            u32 raw;

            BitField<0, 13, s32> difference; // 1.1.11 fixed point
            BitField<13, 11, u32> value;     // 0.0.11 fixed point

            float ToFloat() const {
                return static_cast<float>(value) / 2047.0f;
            }

            float DiffToFloat() const {
                return static_cast<float>(difference) / 2047.0f;
            }
        };

        UnionArray<LutEntry, 128> lut;
    } fog;

    /// Current Pica command list
    struct {
        PAddr addr; // This exists only for serialization
        const u32* head_ptr;
        const u32* current_ptr;
        u32 length;
    } cmd_list;

    /// Struct used to describe immediate mode rendering state
    struct ImmediateModeState {
        // Used to buffer partial vertices for immediate-mode rendering.
        Shader::AttributeBuffer input_vertex;
        // Index of the next attribute to be loaded into `input_vertex`.
        u32 current_attribute = 0;
        // Indicates the immediate mode just started and the geometry pipeline needs to reconfigure
        bool reset_geometry_pipeline = true;

    private:
        friend class boost::serialization::access;
        template <class Archive>
        void serialize(Archive& ar, const unsigned int file_version) {
            ar& input_vertex;
            ar& current_attribute;
            ar& reset_geometry_pipeline;
        }

    } immediate;

    // the geometry shader needs to be kept in the global state because some shaders relie on
    // preserved register value across shader invocation.
    // TODO: also bring the three vertex shader units here and implement the shader scheduler.
    Shader::GSUnitState gs_unit;

    GeometryPipeline geometry_pipeline;

    // This is constructed with a dummy triangle topology
    PrimitiveAssembler<Shader::OutputVertex> primitive_assembler;

    int vs_float_regs_counter = 0;
    std::array<u32, 4> vs_uniform_write_buffer{};

    int gs_float_regs_counter = 0;
    std::array<u32, 4> gs_uniform_write_buffer{};

    int default_attr_counter = 0;
    std::array<u32, 3> default_attr_write_buffer{};

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int file_version) {
        ar& regs.reg_array;
        ar& vs;
        ar& gs;
        ar& input_default_attributes;
        ar& proctex;
        ar& lighting.luts;
        ar& fog.lut;
        ar& cmd_list.addr;
        ar& cmd_list.length;
        ar& immediate;
        ar& gs_unit;
        ar& geometry_pipeline;
        ar& primitive_assembler;
        ar& vs_float_regs_counter;
        ar& boost::serialization::make_array(vs_uniform_write_buffer.data(),
                                             vs_uniform_write_buffer.size());
        ar& gs_float_regs_counter;
        ar& boost::serialization::make_array(gs_uniform_write_buffer.data(),
                                             gs_uniform_write_buffer.size());
        ar& default_attr_counter;
        ar& boost::serialization::make_array(default_attr_write_buffer.data(),
                                             default_attr_write_buffer.size());
        boost::serialization::split_member(ar, *this, file_version);
    }

    template <class Archive>
    void save(Archive& ar, const unsigned int file_version) const {
        ar << static_cast<u32>(cmd_list.current_ptr - cmd_list.head_ptr);
    }

    template <class Archive>
    void load(Archive& ar, const unsigned int file_version) {
        u32 offset{};
        ar >> offset;
        cmd_list.head_ptr =
            reinterpret_cast<u32*>(VideoCore::g_memory->GetPhysicalPointer(cmd_list.addr));
        cmd_list.current_ptr = cmd_list.head_ptr + offset;
    }
};

extern State g_state; ///< Current Pica state

} // namespace Pica

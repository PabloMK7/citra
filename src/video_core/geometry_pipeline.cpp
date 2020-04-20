// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/unique_ptr.hpp>
#include "common/archives.h"
#include "video_core/geometry_pipeline.h"
#include "video_core/pica_state.h"
#include "video_core/regs.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

namespace Pica {

/// An attribute buffering interface for different pipeline modes
class GeometryPipelineBackend {
public:
    virtual ~GeometryPipelineBackend() = default;

    /// Checks if there is no incomplete data transfer
    virtual bool IsEmpty() const = 0;

    /// Checks if the pipeline needs a direct input from index buffer
    virtual bool NeedIndexInput() const = 0;

    /// Submits an index from index buffer
    virtual void SubmitIndex(unsigned int val) = 0;

    /**
     * Submits vertex attributes
     * @param input attributes of a vertex output from vertex shader
     * @return if the buffer is full and the geometry shader should be invoked
     */
    virtual bool SubmitVertex(const Shader::AttributeBuffer& input) = 0;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int file_version) {}
    friend class boost::serialization::access;
};

// In the Point mode, vertex attributes are sent to the input registers in the geometry shader unit.
// The size of vertex shader outputs and geometry shader inputs are constants. Geometry shader is
// invoked upon inputs buffer filled up by vertex shader outputs. For example, if we have a geometry
// shader that takes 6 inputs, and the vertex shader outputs 2 attributes, it would take 3 vertices
// for one geometry shader invocation.
// TODO: what happens when the input size is not divisible by the output size?
class GeometryPipeline_Point : public GeometryPipelineBackend {
public:
    GeometryPipeline_Point(const Regs& regs, Shader::GSUnitState& unit) : regs(regs), unit(unit) {
        ASSERT(regs.pipeline.variable_primitive == 0);
        ASSERT(regs.gs.input_to_uniform == 0);
        vs_output_num = regs.pipeline.vs_outmap_total_minus_1_a + 1;
        std::size_t gs_input_num = regs.gs.max_input_attribute_index + 1;
        ASSERT(gs_input_num % vs_output_num == 0);
        buffer_cur = attribute_buffer.attr;
        buffer_end = attribute_buffer.attr + gs_input_num;
    }

    bool IsEmpty() const override {
        return buffer_cur == attribute_buffer.attr;
    }

    bool NeedIndexInput() const override {
        return false;
    }

    void SubmitIndex(unsigned int val) override {
        UNREACHABLE();
    }

    bool SubmitVertex(const Shader::AttributeBuffer& input) override {
        buffer_cur = std::copy(input.attr, input.attr + vs_output_num, buffer_cur);
        if (buffer_cur == buffer_end) {
            buffer_cur = attribute_buffer.attr;
            unit.LoadInput(regs.gs, attribute_buffer);
            return true;
        }
        return false;
    }

private:
    const Regs& regs;
    Shader::GSUnitState& unit;
    Shader::AttributeBuffer attribute_buffer;
    Common::Vec4<float24>* buffer_cur;
    Common::Vec4<float24>* buffer_end;
    unsigned int vs_output_num;

    GeometryPipeline_Point() : regs(g_state.regs), unit(g_state.gs_unit) {}

    template <typename Class, class Archive>
    static void serialize_common(Class* self, Archive& ar, const unsigned int version) {
        ar& boost::serialization::base_object<GeometryPipelineBackend>(*self);
        ar & self->attribute_buffer;
        ar & self->vs_output_num;
    }

    template <class Archive>
    void save(Archive& ar, const unsigned int version) const {
        serialize_common(this, ar, version);
        auto buffer_idx = static_cast<u32>(buffer_cur - attribute_buffer.attr);
        auto buffer_size = static_cast<u32>(buffer_end - attribute_buffer.attr);
        ar << buffer_idx;
        ar << buffer_size;
    }

    template <class Archive>
    void load(Archive& ar, const unsigned int version) {
        serialize_common(this, ar, version);
        u32 buffer_idx, buffer_size;
        ar >> buffer_idx;
        ar >> buffer_size;
        buffer_cur = attribute_buffer.attr + buffer_idx;
        buffer_end = attribute_buffer.attr + buffer_size;
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER()

    friend class boost::serialization::access;
};

// In VariablePrimitive mode, vertex attributes are buffered into the uniform registers in the
// geometry shader unit. The number of vertex is variable, which is specified by the first index
// value in the batch. This mode is usually used for subdivision.
class GeometryPipeline_VariablePrimitive : public GeometryPipelineBackend {
public:
    GeometryPipeline_VariablePrimitive(const Regs& regs, Shader::ShaderSetup& setup)
        : regs(regs), setup(setup) {
        ASSERT(regs.pipeline.variable_primitive == 1);
        ASSERT(regs.gs.input_to_uniform == 1);
        vs_output_num = regs.pipeline.vs_outmap_total_minus_1_a + 1;
    }

    bool IsEmpty() const override {
        return need_index;
    }

    bool NeedIndexInput() const override {
        return need_index;
    }

    void SubmitIndex(unsigned int val) override {
        DEBUG_ASSERT(need_index);

        // The number of vertex input is put to the uniform register
        float24 vertex_num = float24::FromFloat32(static_cast<float>(val));
        setup.uniforms.f[0] = Common::MakeVec(vertex_num, vertex_num, vertex_num, vertex_num);

        // The second uniform register and so on are used for receiving input vertices
        buffer_cur = setup.uniforms.f + 1;

        main_vertex_num = regs.pipeline.variable_vertex_main_num_minus_1 + 1;
        total_vertex_num = val;
        need_index = false;
    }

    bool SubmitVertex(const Shader::AttributeBuffer& input) override {
        DEBUG_ASSERT(!need_index);
        if (main_vertex_num != 0) {
            // For main vertices, receive all attributes
            buffer_cur = std::copy(input.attr, input.attr + vs_output_num, buffer_cur);
            --main_vertex_num;
        } else {
            // For other vertices, only receive the first attribute (usually the position)
            *(buffer_cur++) = input.attr[0];
        }
        --total_vertex_num;

        if (total_vertex_num == 0) {
            need_index = true;
            return true;
        }

        return false;
    }

private:
    bool need_index = true;
    const Regs& regs;
    Shader::ShaderSetup& setup;
    unsigned int main_vertex_num;
    unsigned int total_vertex_num;
    Common::Vec4<float24>* buffer_cur;
    unsigned int vs_output_num;

    GeometryPipeline_VariablePrimitive() : regs(g_state.regs), setup(g_state.gs) {}

    template <typename Class, class Archive>
    static void serialize_common(Class* self, Archive& ar, const unsigned int version) {
        ar& boost::serialization::base_object<GeometryPipelineBackend>(*self);
        ar & self->need_index;
        ar & self->main_vertex_num;
        ar & self->total_vertex_num;
        ar & self->vs_output_num;
    }

    template <class Archive>
    void save(Archive& ar, const unsigned int version) const {
        serialize_common(this, ar, version);
        auto buffer_idx = static_cast<u32>(buffer_cur - setup.uniforms.f);
        ar << buffer_idx;
    }

    template <class Archive>
    void load(Archive& ar, const unsigned int version) {
        serialize_common(this, ar, version);
        u32 buffer_idx;
        ar >> buffer_idx;
        buffer_cur = setup.uniforms.f + buffer_idx;
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER()

    friend class boost::serialization::access;
};

// In FixedPrimitive mode, vertex attributes are buffered into the uniform registers in the geometry
// shader unit. The number of vertex per shader invocation is constant. This is usually used for
// particle system.
class GeometryPipeline_FixedPrimitive : public GeometryPipelineBackend {
public:
    GeometryPipeline_FixedPrimitive(const Regs& regs, Shader::ShaderSetup& setup)
        : regs(regs), setup(setup) {
        ASSERT(regs.pipeline.variable_primitive == 0);
        ASSERT(regs.gs.input_to_uniform == 1);
        vs_output_num = regs.pipeline.vs_outmap_total_minus_1_a + 1;
        ASSERT(vs_output_num == regs.pipeline.gs_config.stride_minus_1 + 1);
        std::size_t vertex_num = regs.pipeline.gs_config.fixed_vertex_num_minus_1 + 1;
        buffer_cur = buffer_begin = setup.uniforms.f + regs.pipeline.gs_config.start_index;
        buffer_end = buffer_begin + vs_output_num * vertex_num;
    }

    bool IsEmpty() const override {
        return buffer_cur == buffer_begin;
    }

    bool NeedIndexInput() const override {
        return false;
    }

    void SubmitIndex(unsigned int val) override {
        UNREACHABLE();
    }

    bool SubmitVertex(const Shader::AttributeBuffer& input) override {
        buffer_cur = std::copy(input.attr, input.attr + vs_output_num, buffer_cur);
        if (buffer_cur == buffer_end) {
            buffer_cur = buffer_begin;
            return true;
        }
        return false;
    }

private:
    const Regs& regs;
    Shader::ShaderSetup& setup;
    Common::Vec4<float24>* buffer_begin;
    Common::Vec4<float24>* buffer_cur;
    Common::Vec4<float24>* buffer_end;
    unsigned int vs_output_num;

    GeometryPipeline_FixedPrimitive() : regs(g_state.regs), setup(g_state.gs) {}

    template <typename Class, class Archive>
    static void serialize_common(Class* self, Archive& ar, const unsigned int version) {
        ar& boost::serialization::base_object<GeometryPipelineBackend>(*self);
        ar & self->vs_output_num;
    }

    template <class Archive>
    void save(Archive& ar, const unsigned int version) const {
        serialize_common(this, ar, version);
        auto buffer_offset = static_cast<u32>(buffer_begin - setup.uniforms.f);
        auto buffer_idx = static_cast<u32>(buffer_cur - setup.uniforms.f);
        auto buffer_size = static_cast<u32>(buffer_end - setup.uniforms.f);
        ar << buffer_offset;
        ar << buffer_idx;
        ar << buffer_size;
    }

    template <class Archive>
    void load(Archive& ar, const unsigned int version) {
        serialize_common(this, ar, version);
        u32 buffer_offset, buffer_idx, buffer_size;
        ar >> buffer_offset;
        ar >> buffer_idx;
        ar >> buffer_size;
        buffer_begin = setup.uniforms.f + buffer_offset;
        buffer_cur = setup.uniforms.f + buffer_idx;
        buffer_end = setup.uniforms.f + buffer_size;
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER()

    friend class boost::serialization::access;
};

GeometryPipeline::GeometryPipeline(State& state) : state(state) {}

GeometryPipeline::~GeometryPipeline() = default;

void GeometryPipeline::SetVertexHandler(Shader::VertexHandler vertex_handler) {
    this->vertex_handler = std::move(vertex_handler);
}

void GeometryPipeline::Setup(Shader::ShaderEngine* shader_engine) {
    if (!backend)
        return;

    this->shader_engine = shader_engine;
    shader_engine->SetupBatch(state.gs, state.regs.gs.main_offset);
}

void GeometryPipeline::Reconfigure() {
    ASSERT(!backend || backend->IsEmpty());

    if (state.regs.pipeline.use_gs == PipelineRegs::UseGS::No) {
        backend = nullptr;
        return;
    }

    ASSERT(state.regs.pipeline.use_gs == PipelineRegs::UseGS::Yes);

    // The following assumes that when geometry shader is in use, the shader unit 3 is configured as
    // a geometry shader unit.
    // TODO: what happens if this is not true?
    ASSERT(state.regs.pipeline.gs_unit_exclusive_configuration == 1);
    ASSERT(state.regs.gs.shader_mode == ShaderRegs::ShaderMode::GS);

    state.gs_unit.ConfigOutput(state.regs.gs);

    ASSERT(state.regs.pipeline.vs_outmap_total_minus_1_a ==
           state.regs.pipeline.vs_outmap_total_minus_1_b);

    switch (state.regs.pipeline.gs_config.mode) {
    case PipelineRegs::GSMode::Point:
        backend = std::make_unique<GeometryPipeline_Point>(state.regs, state.gs_unit);
        break;
    case PipelineRegs::GSMode::VariablePrimitive:
        backend = std::make_unique<GeometryPipeline_VariablePrimitive>(state.regs, state.gs);
        break;
    case PipelineRegs::GSMode::FixedPrimitive:
        backend = std::make_unique<GeometryPipeline_FixedPrimitive>(state.regs, state.gs);
        break;
    default:
        UNREACHABLE();
    }
}

bool GeometryPipeline::NeedIndexInput() const {
    if (!backend)
        return false;
    return backend->NeedIndexInput();
}

void GeometryPipeline::SubmitIndex(unsigned int val) {
    backend->SubmitIndex(val);
}

void GeometryPipeline::SubmitVertex(const Shader::AttributeBuffer& input) {
    if (!backend) {
        // No backend means the geometry shader is disabled, so we send the vertex shader output
        // directly to the primitive assembler.
        vertex_handler(input);
    } else {
        if (backend->SubmitVertex(input)) {
            shader_engine->Run(state.gs, state.gs_unit);

            // The uniform b15 is set to true after every geometry shader invocation. This is useful
            // for the shader to know if this is the first invocation in a batch, if the program set
            // b15 to false first.
            state.gs.uniforms.b[15] = true;
        }
    }
}

template <class Archive>
void GeometryPipeline::serialize(Archive& ar, const unsigned int version) {
    // vertex_handler and shader_engine are always set to the same value
    ar& backend;
}

} // namespace Pica

SERIALIZE_EXPORT_IMPL(Pica::GeometryPipeline_Point)
SERIALIZE_EXPORT_IMPL(Pica::GeometryPipeline_VariablePrimitive)
SERIALIZE_EXPORT_IMPL(Pica::GeometryPipeline_FixedPrimitive)
SERIALIZE_IMPL(Pica::GeometryPipeline)

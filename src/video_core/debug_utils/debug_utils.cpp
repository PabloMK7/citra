// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>

#ifdef HAVE_PNG
#include <png.h>
#include <setjmp.h>
#endif

#include <nihstro/bit_field.h>
#include <nihstro/float24.h>
#include <nihstro/shader_binary.h>

#include "common/assert.h"
#include "common/bit_field.h"
#include "common/color.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/vector_math.h"

#include "video_core/debug_utils/debug_utils.h"
#include "video_core/pica.h"
#include "video_core/pica_state.h"
#include "video_core/pica_types.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_base.h"
#include "video_core/shader/shader.h"
#include "video_core/utils.h"
#include "video_core/video_core.h"

using nihstro::DVLBHeader;
using nihstro::DVLEHeader;
using nihstro::DVLPHeader;

namespace Pica {

void DebugContext::DoOnEvent(Event event, void* data) {
    {
        std::unique_lock<std::mutex> lock(breakpoint_mutex);

        // Commit the rasterizer's caches so framebuffers, render targets, etc. will show on debug widgets
        VideoCore::g_renderer->Rasterizer()->FlushAll();

        // TODO: Should stop the CPU thread here once we multithread emulation.

        active_breakpoint = event;
        at_breakpoint = true;

        // Tell all observers that we hit a breakpoint
        for (auto& breakpoint_observer : breakpoint_observers) {
            breakpoint_observer->OnPicaBreakPointHit(event, data);
        }

        // Wait until another thread tells us to Resume()
        resume_from_breakpoint.wait(lock, [&]{ return !at_breakpoint; });
    }
}

void DebugContext::Resume() {
    {
        std::lock_guard<std::mutex> lock(breakpoint_mutex);

        // Tell all observers that we are about to resume
        for (auto& breakpoint_observer : breakpoint_observers) {
            breakpoint_observer->OnPicaResume();
        }

        // Resume the waiting thread (i.e. OnEvent())
        at_breakpoint = false;
    }

    resume_from_breakpoint.notify_one();
}

std::shared_ptr<DebugContext> g_debug_context; // TODO: Get rid of this global

namespace DebugUtils {

void DumpShader(const std::string& filename, const Regs::ShaderConfig& config, const Shader::ShaderSetup& setup, const Regs::VSOutputAttributes* output_attributes)
{
    struct StuffToWrite {
        const u8* pointer;
        u32 size;
    };
    std::vector<StuffToWrite> writing_queue;
    u32 write_offset = 0;

    auto QueueForWriting = [&writing_queue,&write_offset](const u8* pointer, u32 size) {
        writing_queue.push_back({pointer, size});
        u32 old_write_offset = write_offset;
        write_offset += size;
        return old_write_offset;
    };

    // First off, try to translate Pica state (one enum for output attribute type and component)
    // into shbin format (separate type and component mask).
    union OutputRegisterInfo {
        enum Type : u64 {
            POSITION   = 0,
            QUATERNION = 1,
            COLOR      = 2,
            TEXCOORD0  = 3,
            TEXCOORD1  = 5,
            TEXCOORD2  = 6,

            VIEW       = 8,
        };

        BitField< 0, 64, u64> hex;

        BitField< 0, 16, Type> type;
        BitField<16, 16, u64> id;
        BitField<32,  4, u64> component_mask;
    };

    // This is put into a try-catch block to make sure we notice unknown configurations.
    std::vector<OutputRegisterInfo> output_info_table;
        for (unsigned i = 0; i < 7; ++i) {
            using OutputAttributes = Pica::Regs::VSOutputAttributes;

            // TODO: It's still unclear how the attribute components map to the register!
            //       Once we know that, this code probably will not make much sense anymore.
            std::map<OutputAttributes::Semantic, std::pair<OutputRegisterInfo::Type, u32> > map = {
                { OutputAttributes::POSITION_X, { OutputRegisterInfo::POSITION, 1} },
                { OutputAttributes::POSITION_Y, { OutputRegisterInfo::POSITION, 2} },
                { OutputAttributes::POSITION_Z, { OutputRegisterInfo::POSITION, 4} },
                { OutputAttributes::POSITION_W, { OutputRegisterInfo::POSITION, 8} },
                { OutputAttributes::QUATERNION_X, { OutputRegisterInfo::QUATERNION, 1} },
                { OutputAttributes::QUATERNION_Y, { OutputRegisterInfo::QUATERNION, 2} },
                { OutputAttributes::QUATERNION_Z, { OutputRegisterInfo::QUATERNION, 4} },
                { OutputAttributes::QUATERNION_W, { OutputRegisterInfo::QUATERNION, 8} },
                { OutputAttributes::COLOR_R, { OutputRegisterInfo::COLOR, 1} },
                { OutputAttributes::COLOR_G, { OutputRegisterInfo::COLOR, 2} },
                { OutputAttributes::COLOR_B, { OutputRegisterInfo::COLOR, 4} },
                { OutputAttributes::COLOR_A, { OutputRegisterInfo::COLOR, 8} },
                { OutputAttributes::TEXCOORD0_U, { OutputRegisterInfo::TEXCOORD0, 1} },
                { OutputAttributes::TEXCOORD0_V, { OutputRegisterInfo::TEXCOORD0, 2} },
                { OutputAttributes::TEXCOORD1_U, { OutputRegisterInfo::TEXCOORD1, 1} },
                { OutputAttributes::TEXCOORD1_V, { OutputRegisterInfo::TEXCOORD1, 2} },
                { OutputAttributes::TEXCOORD2_U, { OutputRegisterInfo::TEXCOORD2, 1} },
                { OutputAttributes::TEXCOORD2_V, { OutputRegisterInfo::TEXCOORD2, 2} },
                { OutputAttributes::VIEW_X, { OutputRegisterInfo::VIEW, 1} },
                { OutputAttributes::VIEW_Y, { OutputRegisterInfo::VIEW, 2} },
                { OutputAttributes::VIEW_Z, { OutputRegisterInfo::VIEW, 4} }
            };

            for (const auto& semantic : std::vector<OutputAttributes::Semantic>{
                                                output_attributes[i].map_x,
                                                output_attributes[i].map_y,
                                                output_attributes[i].map_z,
                                                output_attributes[i].map_w     }) {
                if (semantic == OutputAttributes::INVALID)
                    continue;

                try {
                    OutputRegisterInfo::Type type = map.at(semantic).first;
                    u32 component_mask = map.at(semantic).second;

                    auto it = std::find_if(output_info_table.begin(), output_info_table.end(),
                                        [&i, &type](const OutputRegisterInfo& info) {
                                            return info.id == i && info.type == type;
                                        }
                                        );

                    if (it == output_info_table.end()) {
                        output_info_table.emplace_back();
                        output_info_table.back().type.Assign(type);
                        output_info_table.back().component_mask.Assign(component_mask);
                        output_info_table.back().id.Assign(i);
                    } else {
                        it->component_mask.Assign(it->component_mask | component_mask);
                    }
                } catch (const std::out_of_range& ) {
                    DEBUG_ASSERT_MSG(false, "Unknown output attribute mapping");
                    LOG_ERROR(HW_GPU, "Unknown output attribute mapping: %03x, %03x, %03x, %03x",
                              (int)output_attributes[i].map_x.Value(),
                              (int)output_attributes[i].map_y.Value(),
                              (int)output_attributes[i].map_z.Value(),
                              (int)output_attributes[i].map_w.Value());
                }
            }
        }


    struct {
        DVLBHeader header;
        u32 dvle_offset;
    } dvlb{ {DVLBHeader::MAGIC_WORD, 1 } }; // 1 DVLE

    DVLPHeader dvlp{ DVLPHeader::MAGIC_WORD };
    DVLEHeader dvle{ DVLEHeader::MAGIC_WORD };

    QueueForWriting(reinterpret_cast<const u8*>(&dvlb), sizeof(dvlb));
    u32 dvlp_offset = QueueForWriting(reinterpret_cast<const u8*>(&dvlp), sizeof(dvlp));
    dvlb.dvle_offset = QueueForWriting(reinterpret_cast<const u8*>(&dvle), sizeof(dvle));

    // TODO: Reduce the amount of binary code written to relevant portions
    dvlp.binary_offset = write_offset - dvlp_offset;
    dvlp.binary_size_words = static_cast<uint32_t>(setup.program_code.size());
    QueueForWriting(reinterpret_cast<const u8*>(setup.program_code.data()),
                    static_cast<u32>(setup.program_code.size()) * sizeof(u32));

    dvlp.swizzle_info_offset = write_offset - dvlp_offset;
    dvlp.swizzle_info_num_entries = static_cast<uint32_t>(setup.swizzle_data.size());
    u32 dummy = 0;
    for (unsigned int i = 0; i < setup.swizzle_data.size(); ++i) {
        QueueForWriting(reinterpret_cast<const u8*>(&setup.swizzle_data[i]), sizeof(setup.swizzle_data[i]));
        QueueForWriting(reinterpret_cast<const u8*>(&dummy), sizeof(dummy));
    }

    dvle.main_offset_words = config.main_offset;
    dvle.output_register_table_offset = write_offset - dvlb.dvle_offset;
    dvle.output_register_table_size = static_cast<u32>(output_info_table.size());
    QueueForWriting(reinterpret_cast<const u8*>(output_info_table.data()), static_cast<u32>(output_info_table.size() * sizeof(OutputRegisterInfo)));

    // TODO: Create a label table for "main"

    std::vector<nihstro::ConstantInfo> constant_table;
    for (unsigned i = 0; i < setup.uniforms.b.size(); ++i) {
        nihstro::ConstantInfo constant;
        memset(&constant, 0, sizeof(constant));
        constant.type = nihstro::ConstantInfo::Bool;
        constant.regid = i;
        constant.b = setup.uniforms.b[i];
        constant_table.emplace_back(constant);
    }
    for (unsigned i = 0; i < setup.uniforms.i.size(); ++i) {
        nihstro::ConstantInfo constant;
        memset(&constant, 0, sizeof(constant));
        constant.type = nihstro::ConstantInfo::Int;
        constant.regid = i;
        constant.i.x = setup.uniforms.i[i].x;
        constant.i.y = setup.uniforms.i[i].y;
        constant.i.z = setup.uniforms.i[i].z;
        constant.i.w = setup.uniforms.i[i].w;
        constant_table.emplace_back(constant);
    }
    for (unsigned i = 0; i < sizeof(setup.uniforms.f) / sizeof(setup.uniforms.f[0]); ++i) {
        nihstro::ConstantInfo constant;
        memset(&constant, 0, sizeof(constant));
        constant.type = nihstro::ConstantInfo::Float;
        constant.regid = i;
        constant.f.x = nihstro::to_float24(setup.uniforms.f[i].x.ToFloat32());
        constant.f.y = nihstro::to_float24(setup.uniforms.f[i].y.ToFloat32());
        constant.f.z = nihstro::to_float24(setup.uniforms.f[i].z.ToFloat32());
        constant.f.w = nihstro::to_float24(setup.uniforms.f[i].w.ToFloat32());

        // Store constant if it's different from zero..
        if (setup.uniforms.f[i].x.ToFloat32() != 0.0 ||
            setup.uniforms.f[i].y.ToFloat32() != 0.0 ||
            setup.uniforms.f[i].z.ToFloat32() != 0.0 ||
            setup.uniforms.f[i].w.ToFloat32() != 0.0)
            constant_table.emplace_back(constant);
    }
    dvle.constant_table_offset = write_offset - dvlb.dvle_offset;
    dvle.constant_table_size = static_cast<uint32_t>(constant_table.size());
    for (const auto& constant : constant_table) {
        QueueForWriting(reinterpret_cast<const u8*>(&constant), sizeof(constant));
    }

    // Write data to file
    std::ofstream file(filename, std::ios_base::out | std::ios_base::binary);

    for (const auto& chunk : writing_queue) {
        file.write(reinterpret_cast<const char*>(chunk.pointer), chunk.size);
    }
}

static std::unique_ptr<PicaTrace> pica_trace;
static std::mutex pica_trace_mutex;
static int is_pica_tracing = false;

void StartPicaTracing()
{
    if (is_pica_tracing) {
        LOG_WARNING(HW_GPU, "StartPicaTracing called even though tracing already running!");
        return;
    }

    std::lock_guard<std::mutex> lock(pica_trace_mutex);
    pica_trace = std::make_unique<PicaTrace>();

    is_pica_tracing = true;
}

bool IsPicaTracing()
{
    return is_pica_tracing != 0;
}

void OnPicaRegWrite(PicaTrace::Write write)
{
    // Double check for is_pica_tracing to avoid pointless locking overhead
    if (!is_pica_tracing)
        return;

    std::lock_guard<std::mutex> lock(pica_trace_mutex);

    if (!is_pica_tracing)
        return;

    pica_trace->writes.push_back(write);
}

std::unique_ptr<PicaTrace> FinishPicaTracing()
{
    if (!is_pica_tracing) {
        LOG_WARNING(HW_GPU, "FinishPicaTracing called even though tracing isn't running!");
        return {};
    }

    // signalize that no further tracing should be performed
    is_pica_tracing = false;

    // Wait until running tracing is finished
    std::lock_guard<std::mutex> lock(pica_trace_mutex);
    std::unique_ptr<PicaTrace> ret(std::move(pica_trace));

    return ret;
}

const Math::Vec4<u8> LookupTexture(const u8* source, int x, int y, const TextureInfo& info, bool disable_alpha) {
    const unsigned int coarse_x = x & ~7;
    const unsigned int coarse_y = y & ~7;

    if (info.format != Regs::TextureFormat::ETC1 &&
        info.format != Regs::TextureFormat::ETC1A4) {
        // TODO(neobrain): Fix code design to unify vertical block offsets!
        source += coarse_y * info.stride;
    }

    // TODO: Assert that width/height are multiples of block dimensions

    switch (info.format) {
    case Regs::TextureFormat::RGBA8:
    {
        auto res = Color::DecodeRGBA8(source + VideoCore::GetMortonOffset(x, y, 4));
        return { res.r(), res.g(), res.b(), static_cast<u8>(disable_alpha ? 255 : res.a()) };
    }

    case Regs::TextureFormat::RGB8:
    {
        auto res = Color::DecodeRGB8(source + VideoCore::GetMortonOffset(x, y, 3));
        return { res.r(), res.g(), res.b(), 255 };
    }

    case Regs::TextureFormat::RGB5A1:
    {
        auto res = Color::DecodeRGB5A1(source + VideoCore::GetMortonOffset(x, y, 2));
        return { res.r(), res.g(), res.b(), static_cast<u8>(disable_alpha ? 255 : res.a()) };
    }

    case Regs::TextureFormat::RGB565:
    {
        auto res = Color::DecodeRGB565(source + VideoCore::GetMortonOffset(x, y, 2));
        return { res.r(), res.g(), res.b(), 255 };
    }

    case Regs::TextureFormat::RGBA4:
    {
        auto res = Color::DecodeRGBA4(source + VideoCore::GetMortonOffset(x, y, 2));
        return { res.r(), res.g(), res.b(), static_cast<u8>(disable_alpha ? 255 : res.a()) };
    }

    case Regs::TextureFormat::IA8:
    {
        const u8* source_ptr = source + VideoCore::GetMortonOffset(x, y, 2);

        if (disable_alpha) {
            // Show intensity as red, alpha as green
            return { source_ptr[1], source_ptr[0], 0, 255 };
        } else {
            return { source_ptr[1], source_ptr[1], source_ptr[1], source_ptr[0] };
        }
    }

    case Regs::TextureFormat::RG8:
    {
        auto res = Color::DecodeRG8(source + VideoCore::GetMortonOffset(x, y, 2));
        return { res.r(), res.g(), 0, 255 };
    }

    case Regs::TextureFormat::I8:
    {
        const u8* source_ptr = source + VideoCore::GetMortonOffset(x, y, 1);
        return { *source_ptr, *source_ptr, *source_ptr, 255 };
    }

    case Regs::TextureFormat::A8:
    {
        const u8* source_ptr = source + VideoCore::GetMortonOffset(x, y, 1);

        if (disable_alpha) {
            return { *source_ptr, *source_ptr, *source_ptr, 255 };
        } else {
            return { 0, 0, 0, *source_ptr };
        }
    }

    case Regs::TextureFormat::IA4:
    {
        const u8* source_ptr = source + VideoCore::GetMortonOffset(x, y, 1);

        u8 i = Color::Convert4To8(((*source_ptr) & 0xF0) >> 4);
        u8 a = Color::Convert4To8((*source_ptr) & 0xF);

        if (disable_alpha) {
            // Show intensity as red, alpha as green
            return { i, a, 0, 255 };
        } else {
            return { i, i, i, a };
        }
    }

    case Regs::TextureFormat::I4:
    {
        u32 morton_offset = VideoCore::GetMortonOffset(x, y, 1);
        const u8* source_ptr = source + morton_offset / 2;

        u8 i = (morton_offset % 2) ? ((*source_ptr & 0xF0) >> 4) : (*source_ptr & 0xF);
        i = Color::Convert4To8(i);

        return { i, i, i, 255 };
    }

    case Regs::TextureFormat::A4:
    {
        u32 morton_offset = VideoCore::GetMortonOffset(x, y, 1);
        const u8* source_ptr = source + morton_offset / 2;

        u8 a = (morton_offset % 2) ? ((*source_ptr & 0xF0) >> 4) : (*source_ptr & 0xF);
        a = Color::Convert4To8(a);

        if (disable_alpha) {
            return { a, a, a, 255 };
        } else {
            return { 0, 0, 0, a };
        }
    }

    case Regs::TextureFormat::ETC1:
    case Regs::TextureFormat::ETC1A4:
    {
        bool has_alpha = (info.format == Regs::TextureFormat::ETC1A4);

        // ETC1 further subdivides each 8x8 tile into four 4x4 subtiles
        const int subtile_width = 4;
        const int subtile_height = 4;

        int subtile_index = ((x / subtile_width) & 1) + 2 * ((y / subtile_height) & 1);
        unsigned subtile_bytes = has_alpha ? 2 : 1; // TODO: Name...

        const u64* source_ptr = (const u64*)(source
                                             + coarse_x * subtile_bytes * 4
                                             + coarse_y * subtile_bytes * 4 * (info.width / 8)
                                             + subtile_index * subtile_bytes * 8);
        u64 alpha = 0xFFFFFFFFFFFFFFFF;
        if (has_alpha) {
            alpha = *source_ptr;
            source_ptr++;
        }

        union ETC1Tile {
            // Each of these two is a collection of 16 bits (one per lookup value)
            BitField< 0, 16, u64> table_subindexes;
            BitField<16, 16, u64> negation_flags;

            unsigned GetTableSubIndex(unsigned index) const {
                return (table_subindexes >> index) & 1;
            }

            bool GetNegationFlag(unsigned index) const {
                return ((negation_flags >> index) & 1) == 1;
            }

            BitField<32, 1, u64> flip;
            BitField<33, 1, u64> differential_mode;

            BitField<34, 3, u64> table_index_2;
            BitField<37, 3, u64> table_index_1;

            union {
                // delta value + base value
                BitField<40, 3, s64> db;
                BitField<43, 5, u64> b;

                BitField<48, 3, s64> dg;
                BitField<51, 5, u64> g;

                BitField<56, 3, s64> dr;
                BitField<59, 5, u64> r;
            } differential;

            union {
                BitField<40, 4, u64> b2;
                BitField<44, 4, u64> b1;

                BitField<48, 4, u64> g2;
                BitField<52, 4, u64> g1;

                BitField<56, 4, u64> r2;
                BitField<60, 4, u64> r1;
            } separate;

            const Math::Vec3<u8> GetRGB(int x, int y) const {
                int texel = 4 * x + y;

                if (flip)
                    std::swap(x, y);

                // Lookup base value
                Math::Vec3<int> ret;
                if (differential_mode) {
                    ret.r() = static_cast<int>(differential.r);
                    ret.g() = static_cast<int>(differential.g);
                    ret.b() = static_cast<int>(differential.b);
                    if (x >= 2) {
                        ret.r() += static_cast<int>(differential.dr);
                        ret.g() += static_cast<int>(differential.dg);
                        ret.b() += static_cast<int>(differential.db);
                    }
                    ret.r() = Color::Convert5To8(ret.r());
                    ret.g() = Color::Convert5To8(ret.g());
                    ret.b() = Color::Convert5To8(ret.b());
                } else {
                    if (x < 2) {
                        ret.r() = Color::Convert4To8(static_cast<u8>(separate.r1));
                        ret.g() = Color::Convert4To8(static_cast<u8>(separate.g1));
                        ret.b() = Color::Convert4To8(static_cast<u8>(separate.b1));
                    } else {
                        ret.r() = Color::Convert4To8(static_cast<u8>(separate.r2));
                        ret.g() = Color::Convert4To8(static_cast<u8>(separate.g2));
                        ret.b() = Color::Convert4To8(static_cast<u8>(separate.b2));
                    }
                }

                // Add modifier
                unsigned table_index = static_cast<int>((x < 2) ? table_index_1.Value() : table_index_2.Value());

                static const std::array<std::array<u8, 2>, 8> etc1_modifier_table = {{
                    {{  2,  8 }}, {{  5, 17 }}, {{  9,  29 }}, {{ 13,  42 }},
                    {{ 18, 60 }}, {{ 24, 80 }}, {{ 33, 106 }}, {{ 47, 183 }}
                }};

                int modifier = etc1_modifier_table.at(table_index).at(GetTableSubIndex(texel));
                if (GetNegationFlag(texel))
                    modifier *= -1;

                ret.r() = MathUtil::Clamp(ret.r() + modifier, 0, 255);
                ret.g() = MathUtil::Clamp(ret.g() + modifier, 0, 255);
                ret.b() = MathUtil::Clamp(ret.b() + modifier, 0, 255);

                return ret.Cast<u8>();
            }
        } const *etc1_tile = reinterpret_cast<const ETC1Tile*>(source_ptr);

        alpha >>= 4 * ((x & 3) * 4 + (y & 3));
        return Math::MakeVec(etc1_tile->GetRGB(x & 3, y & 3),
                             disable_alpha ? (u8)255 : Color::Convert4To8(alpha & 0xF));
    }

    default:
        LOG_ERROR(HW_GPU, "Unknown texture format: %x", (u32)info.format);
        DEBUG_ASSERT(false);
        return {};
    }
}

TextureInfo TextureInfo::FromPicaRegister(const Regs::TextureConfig& config,
                                          const Regs::TextureFormat& format)
{
    TextureInfo info;
    info.physical_address = config.GetPhysicalAddress();
    info.width = config.width;
    info.height = config.height;
    info.format = format;
    info.stride = Pica::Regs::NibblesPerPixel(info.format) * info.width / 2;
    return info;
}

#ifdef HAVE_PNG
// Adapter functions to libpng to write/flush to File::IOFile instances.
static void WriteIOFile(png_structp png_ptr, png_bytep data, png_size_t length) {
    auto* fp = static_cast<FileUtil::IOFile*>(png_get_io_ptr(png_ptr));
    if (!fp->WriteBytes(data, length))
         png_error(png_ptr, "Failed to write to output PNG file.");
}

static void FlushIOFile(png_structp png_ptr) {
    auto* fp = static_cast<FileUtil::IOFile*>(png_get_io_ptr(png_ptr));
    if (!fp->Flush())
         png_error(png_ptr, "Failed to flush to output PNG file.");
}
#endif

void DumpTexture(const Pica::Regs::TextureConfig& texture_config, u8* data) {
#ifndef HAVE_PNG
    return;
#else
    if (!data)
        return;

    // Write data to file
    static int dump_index = 0;
    std::string filename = std::string("texture_dump") + std::to_string(++dump_index) + std::string(".png");
    u32 row_stride = texture_config.width * 3;

    u8* buf;

    char title[] = "Citra texture dump";
    char title_key[] = "Title";
    png_structp png_ptr = nullptr;
    png_infop info_ptr = nullptr;

    // Open file for writing (binary mode)
    FileUtil::IOFile fp(filename, "wb");

    // Initialize write structure
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (png_ptr == nullptr) {
        LOG_ERROR(Debug_GPU, "Could not allocate write struct");
        goto finalise;

    }

    // Initialize info structure
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == nullptr) {
        LOG_ERROR(Debug_GPU, "Could not allocate info struct");
        goto finalise;
    }

    // Setup Exception handling
    if (setjmp(png_jmpbuf(png_ptr))) {
        LOG_ERROR(Debug_GPU, "Error during png creation");
        goto finalise;
    }

    png_set_write_fn(png_ptr, static_cast<void*>(&fp), WriteIOFile, FlushIOFile);

    // Write header (8 bit color depth)
    png_set_IHDR(png_ptr, info_ptr, texture_config.width, texture_config.height,
        8, PNG_COLOR_TYPE_RGB /*_ALPHA*/, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_text title_text;
    title_text.compression = PNG_TEXT_COMPRESSION_NONE;
    title_text.key = title_key;
    title_text.text = title;
    png_set_text(png_ptr, info_ptr, &title_text, 1);

    png_write_info(png_ptr, info_ptr);

    buf = new u8[row_stride * texture_config.height];
    for (unsigned y = 0; y < texture_config.height; ++y) {
        for (unsigned x = 0; x < texture_config.width; ++x) {
            TextureInfo info;
            info.width = texture_config.width;
            info.height = texture_config.height;
            info.stride = row_stride;
            info.format = g_state.regs.texture0_format;
            Math::Vec4<u8> texture_color = LookupTexture(data, x, y, info);
            buf[3 * x + y * row_stride    ] = texture_color.r();
            buf[3 * x + y * row_stride + 1] = texture_color.g();
            buf[3 * x + y * row_stride + 2] = texture_color.b();
        }
    }

    // Write image data
    for (unsigned y = 0; y < texture_config.height; ++y)
    {
        u8* row_ptr = (u8*)buf + y * row_stride;
        png_write_row(png_ptr, row_ptr);
    }

    delete[] buf;

    // End write
    png_write_end(png_ptr, nullptr);

finalise:
    if (info_ptr != nullptr) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    if (png_ptr != nullptr) png_destroy_write_struct(&png_ptr, (png_infopp)nullptr);
#endif
}

static std::string ReplacePattern(const std::string& input, const std::string& pattern, const std::string& replacement) {
    size_t start = input.find(pattern);
    if (start == std::string::npos)
        return input;

    std::string ret = input;
    ret.replace(start, pattern.length(), replacement);
    return ret;
}

static std::string GetTevStageConfigSourceString(const Pica::Regs::TevStageConfig::Source& source) {
    using Source = Pica::Regs::TevStageConfig::Source;
    static const std::map<Source, std::string> source_map = {
        { Source::PrimaryColor,           "PrimaryColor" },
        { Source::PrimaryFragmentColor,   "PrimaryFragmentColor" },
        { Source::SecondaryFragmentColor, "SecondaryFragmentColor" },
        { Source::Texture0,               "Texture0" },
        { Source::Texture1,               "Texture1" },
        { Source::Texture2,               "Texture2" },
        { Source::Texture3,               "Texture3" },
        { Source::PreviousBuffer,         "PreviousBuffer" },
        { Source::Constant,               "Constant" },
        { Source::Previous,               "Previous" },
    };

    const auto src_it = source_map.find(source);
    if (src_it == source_map.end())
        return "Unknown";

    return src_it->second;
}

static std::string GetTevStageConfigColorSourceString(const Pica::Regs::TevStageConfig::Source& source, const Pica::Regs::TevStageConfig::ColorModifier modifier) {
    using ColorModifier = Pica::Regs::TevStageConfig::ColorModifier;
    static const std::map<ColorModifier, std::string> color_modifier_map = {
        { ColorModifier::SourceColor,         "%source.rgb" },
        { ColorModifier::OneMinusSourceColor, "(1.0 - %source.rgb)" },
        { ColorModifier::SourceAlpha,         "%source.aaa" },
        { ColorModifier::OneMinusSourceAlpha, "(1.0 - %source.aaa)" },
        { ColorModifier::SourceRed,           "%source.rrr" },
        { ColorModifier::OneMinusSourceRed,   "(1.0 - %source.rrr)" },
        { ColorModifier::SourceGreen,         "%source.ggg" },
        { ColorModifier::OneMinusSourceGreen, "(1.0 - %source.ggg)" },
        { ColorModifier::SourceBlue,          "%source.bbb" },
        { ColorModifier::OneMinusSourceBlue,  "(1.0 - %source.bbb)" },
    };

    auto src_str = GetTevStageConfigSourceString(source);
    auto modifier_it = color_modifier_map.find(modifier);
    std::string modifier_str = "%source.????";
    if (modifier_it != color_modifier_map.end())
        modifier_str = modifier_it->second;

    return ReplacePattern(modifier_str, "%source", src_str);
}

static std::string GetTevStageConfigAlphaSourceString(const Pica::Regs::TevStageConfig::Source& source, const Pica::Regs::TevStageConfig::AlphaModifier modifier) {
    using AlphaModifier = Pica::Regs::TevStageConfig::AlphaModifier;
    static const std::map<AlphaModifier, std::string> alpha_modifier_map = {
        { AlphaModifier::SourceAlpha,         "%source.a" },
        { AlphaModifier::OneMinusSourceAlpha, "(1.0 - %source.a)" },
        { AlphaModifier::SourceRed,           "%source.r" },
        { AlphaModifier::OneMinusSourceRed,   "(1.0 - %source.r)" },
        { AlphaModifier::SourceGreen,         "%source.g" },
        { AlphaModifier::OneMinusSourceGreen, "(1.0 - %source.g)" },
        { AlphaModifier::SourceBlue,          "%source.b" },
        { AlphaModifier::OneMinusSourceBlue,  "(1.0 - %source.b)" },
    };

    auto src_str = GetTevStageConfigSourceString(source);
    auto modifier_it = alpha_modifier_map.find(modifier);
    std::string modifier_str = "%source.????";
    if (modifier_it != alpha_modifier_map.end())
        modifier_str = modifier_it->second;

    return ReplacePattern(modifier_str, "%source", src_str);
}

static std::string GetTevStageConfigOperationString(const Pica::Regs::TevStageConfig::Operation& operation) {
    using Operation = Pica::Regs::TevStageConfig::Operation;
    static const std::map<Operation, std::string> combiner_map = {
        { Operation::Replace,         "%source1" },
        { Operation::Modulate,        "(%source1 * %source2)" },
        { Operation::Add,             "(%source1 + %source2)" },
        { Operation::AddSigned,       "(%source1 + %source2) - 0.5" },
        { Operation::Lerp,            "lerp(%source1, %source2, %source3)" },
        { Operation::Subtract,        "(%source1 - %source2)" },
        { Operation::Dot3_RGB,        "dot(%source1, %source2)" },
        { Operation::MultiplyThenAdd, "((%source1 * %source2) + %source3)" },
        { Operation::AddThenMultiply, "((%source1 + %source2) * %source3)" },
    };

    const auto op_it = combiner_map.find(operation);
    if (op_it == combiner_map.end())
        return "Unknown op (%source1, %source2, %source3)";

    return op_it->second;
}

std::string GetTevStageConfigColorCombinerString(const Pica::Regs::TevStageConfig& tev_stage) {
    auto op_str = GetTevStageConfigOperationString(tev_stage.color_op);
    op_str = ReplacePattern(op_str, "%source1", GetTevStageConfigColorSourceString(tev_stage.color_source1, tev_stage.color_modifier1));
    op_str = ReplacePattern(op_str, "%source2", GetTevStageConfigColorSourceString(tev_stage.color_source2, tev_stage.color_modifier2));
    return   ReplacePattern(op_str, "%source3", GetTevStageConfigColorSourceString(tev_stage.color_source3, tev_stage.color_modifier3));
}

std::string GetTevStageConfigAlphaCombinerString(const Pica::Regs::TevStageConfig& tev_stage) {
    auto op_str = GetTevStageConfigOperationString(tev_stage.alpha_op);
    op_str = ReplacePattern(op_str, "%source1", GetTevStageConfigAlphaSourceString(tev_stage.alpha_source1, tev_stage.alpha_modifier1));
    op_str = ReplacePattern(op_str, "%source2", GetTevStageConfigAlphaSourceString(tev_stage.alpha_source2, tev_stage.alpha_modifier2));
    return   ReplacePattern(op_str, "%source3", GetTevStageConfigAlphaSourceString(tev_stage.alpha_source3, tev_stage.alpha_modifier3));
}

void DumpTevStageConfig(const std::array<Pica::Regs::TevStageConfig, 6>& stages) {
    std::string stage_info = "Tev setup:\n";
    for (size_t index = 0; index < stages.size(); ++index) {
        const auto& tev_stage = stages[index];
        stage_info += "Stage " + std::to_string(index) + ": " + GetTevStageConfigColorCombinerString(tev_stage) + "   " + GetTevStageConfigAlphaCombinerString(tev_stage) + "\n";
    }
    LOG_TRACE(HW_GPU, "%s", stage_info.c_str());
}

} // namespace

} // namespace

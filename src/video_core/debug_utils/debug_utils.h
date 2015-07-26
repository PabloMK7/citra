// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <condition_variable>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "common/vector_math.h"

#include "core/tracer/recorder.h"

#include "video_core/pica.h"

namespace Pica {

class DebugContext {
public:
    enum class Event {
        FirstEvent = 0,

        PicaCommandLoaded = FirstEvent,
        PicaCommandProcessed,
        IncomingPrimitiveBatch,
        FinishedPrimitiveBatch,
        VertexLoaded,
        IncomingDisplayTransfer,
        GSPCommandProcessed,
        BufferSwapped,

        NumEvents
    };

    /**
     * Inherit from this class to be notified of events registered to some debug context.
     * Most importantly this is used for our debugger GUI.
     *
     * To implement event handling, override the OnPicaBreakPointHit and OnPicaResume methods.
     * @warning All BreakPointObservers need to be on the same thread to guarantee thread-safe state access
     * @todo Evaluate an alternative interface, in which there is only one managing observer and multiple child observers running (by design) on the same thread.
     */
    class BreakPointObserver {
    public:
        /// Constructs the object such that it observes events of the given DebugContext.
        BreakPointObserver(std::shared_ptr<DebugContext> debug_context) : context_weak(debug_context) {
            std::unique_lock<std::mutex> lock(debug_context->breakpoint_mutex);
            debug_context->breakpoint_observers.push_back(this);
        }

        virtual ~BreakPointObserver() {
            auto context = context_weak.lock();
            if (context) {
                std::unique_lock<std::mutex> lock(context->breakpoint_mutex);
                context->breakpoint_observers.remove(this);

                // If we are the last observer to be destroyed, tell the debugger context that
                // it is free to continue. In particular, this is required for a proper Citra
                // shutdown, when the emulation thread is waiting at a breakpoint.
                if (context->breakpoint_observers.empty())
                    context->Resume();
            }
        }

        /**
         * Action to perform when a breakpoint was reached.
         * @param event Type of event which triggered the breakpoint
         * @param data Optional data pointer (if unused, this is a nullptr)
         * @note This function will perform nothing unless it is overridden in the child class.
         */
        virtual void OnPicaBreakPointHit(Event, void*) {
        }

        /**
         * Action to perform when emulation is resumed from a breakpoint.
         * @note This function will perform nothing unless it is overridden in the child class.
         */
        virtual void OnPicaResume() {
        }

    protected:
        /**
         * Weak context pointer. This need not be valid, so when requesting a shared_ptr via
         * context_weak.lock(), always compare the result against nullptr.
         */
        std::weak_ptr<DebugContext> context_weak;
    };

    /**
     * Simple structure defining a breakpoint state
     */
    struct BreakPoint {
        bool enabled = false;
    };

    /**
     * Static constructor used to create a shared_ptr of a DebugContext.
     */
    static std::shared_ptr<DebugContext> Construct() {
        return std::shared_ptr<DebugContext>(new DebugContext);
    }

    /**
     * Used by the emulation core when a given event has happened. If a breakpoint has been set
     * for this event, OnEvent calls the event handlers of the registered breakpoint observers.
     * The current thread then is halted until Resume() is called from another thread (or until
     * emulation is stopped).
     * @param event Event which has happened
     * @param data Optional data pointer (pass nullptr if unused). Needs to remain valid until Resume() is called.
     */
    void OnEvent(Event event, void* data);

    /**
     * Resume from the current breakpoint.
     * @warning Calling this from the same thread that OnEvent was called in will cause a deadlock. Calling from any other thread is safe.
     */
    void Resume();

    /**
     * Delete all set breakpoints and resume emulation.
     */
    void ClearBreakpoints() {
        breakpoints.clear();
        Resume();
    }

    // TODO: Evaluate if access to these members should be hidden behind a public interface.
    std::map<Event, BreakPoint> breakpoints;
    Event active_breakpoint;
    bool at_breakpoint = false;

    std::shared_ptr<CiTrace::Recorder> recorder = nullptr;

private:
    /**
     * Private default constructor to make sure people always construct this through Construct()
     * instead.
     */
    DebugContext() = default;

    /// Mutex protecting current breakpoint state and the observer list.
    std::mutex breakpoint_mutex;

    /// Used by OnEvent to wait for resumption.
    std::condition_variable resume_from_breakpoint;

    /// List of registered observers
    std::list<BreakPointObserver*> breakpoint_observers;
};

extern std::shared_ptr<DebugContext> g_debug_context; // TODO: Get rid of this global

namespace DebugUtils {

#define PICA_DUMP_GEOMETRY 0
#define PICA_DUMP_SHADERS 0
#define PICA_DUMP_TEXTURES 0
#define PICA_LOG_TEV 0

// Simple utility class for dumping geometry data to an OBJ file
class GeometryDumper {
public:
    struct Vertex {
        std::array<float,3> pos;
    };

    void AddTriangle(Vertex& v0, Vertex& v1, Vertex& v2);

    void Dump();

private:
    struct Face {
        int index[3];
    };

    std::vector<Vertex> vertices;
    std::vector<Face> faces;
};

void DumpShader(const u32* binary_data, u32 binary_size, const u32* swizzle_data, u32 swizzle_size,
                u32 main_offset, const Regs::VSOutputAttributes* output_attributes);


// Utility class to log Pica commands.
struct PicaTrace {
    struct Write : public std::pair<u32,u32> {
        Write(u32 id, u32 value) : std::pair<u32,u32>(id, value) {}

        u32& Id() { return first; }
        const u32& Id() const { return first; }

        u32& Value() { return second; }
        const u32& Value() const { return second; }
    };
    std::vector<Write> writes;
};

void StartPicaTracing();
bool IsPicaTracing();
void OnPicaRegWrite(u32 id, u32 value);
std::unique_ptr<PicaTrace> FinishPicaTracing();

struct TextureInfo {
    PAddr physical_address;
    int width;
    int height;
    int stride;
    Pica::Regs::TextureFormat format;

    static TextureInfo FromPicaRegister(const Pica::Regs::TextureConfig& config,
                                        const Pica::Regs::TextureFormat& format);
};

/**
 * Lookup texel located at the given coordinates and return an RGBA vector of its color.
 * @param source Source pointer to read data from
 * @param s,t Texture coordinates to read from
 * @param info TextureInfo object describing the texture setup
 * @param disable_alpha This is used for debug widgets which use this method to display textures without providing a good way to visualize alpha by themselves. If true, this will return 255 for the alpha component, and either drop the information entirely or store it in an "unused" color channel.
 * @todo Eventually we should get rid of the disable_alpha parameter.
 */
const Math::Vec4<u8> LookupTexture(const u8* source, int s, int t, const TextureInfo& info,
                                   bool disable_alpha = false);

void DumpTexture(const Pica::Regs::TextureConfig& texture_config, u8* data);

void DumpTevStageConfig(const std::array<Pica::Regs::TevStageConfig,6>& stages);

} // namespace

} // namespace

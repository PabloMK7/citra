// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <array>
#include <condition_variable>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include "common/common_types.h"
#include "common/vector_math.h"
#include "video_core/pica.h"

namespace CiTrace {
class Recorder;
}

namespace Pica {

namespace Shader {
struct ShaderSetup;
}

class DebugContext {
public:
    enum class Event {
        FirstEvent = 0,

        PicaCommandLoaded = FirstEvent,
        PicaCommandProcessed,
        IncomingPrimitiveBatch,
        FinishedPrimitiveBatch,
        VertexShaderInvocation,
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
     * @warning All BreakPointObservers need to be on the same thread to guarantee thread-safe state
     * access
     * @todo Evaluate an alternative interface, in which there is only one managing observer and
     * multiple child observers running (by design) on the same thread.
     */
    class BreakPointObserver {
    public:
        /// Constructs the object such that it observes events of the given DebugContext.
        BreakPointObserver(std::shared_ptr<DebugContext> debug_context)
            : context_weak(debug_context) {
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
        virtual void OnPicaBreakPointHit(Event, void*) {}

        /**
         * Action to perform when emulation is resumed from a breakpoint.
         * @note This function will perform nothing unless it is overridden in the child class.
         */
        virtual void OnPicaResume() {}

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
     * @param data Optional data pointer (pass nullptr if unused). Needs to remain valid until
     * Resume() is called.
     */
    void OnEvent(Event event, void* data) {
        // This check is left in the header to allow the compiler to inline it.
        if (!breakpoints[(int)event].enabled)
            return;
        // For the rest of event handling, call a separate function.
        DoOnEvent(event, data);
    }

    void DoOnEvent(Event event, void* data);

    /**
     * Resume from the current breakpoint.
     * @warning Calling this from the same thread that OnEvent was called in will cause a deadlock.
     * Calling from any other thread is safe.
     */
    void Resume();

    /**
     * Delete all set breakpoints and resume emulation.
     */
    void ClearBreakpoints() {
        for (auto& bp : breakpoints) {
            bp.enabled = false;
        }
        Resume();
    }

    // TODO: Evaluate if access to these members should be hidden behind a public interface.
    std::array<BreakPoint, (int)Event::NumEvents> breakpoints;
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

#define PICA_DUMP_TEXTURES 0
#define PICA_LOG_TEV 0

void DumpShader(const std::string& filename, const Regs::ShaderConfig& config,
                const Shader::ShaderSetup& setup,
                const Regs::VSOutputAttributes* output_attributes);

// Utility class to log Pica commands.
struct PicaTrace {
    struct Write {
        u16 cmd_id;
        u16 mask;
        u32 value;
    };
    std::vector<Write> writes;
};

void StartPicaTracing();
bool IsPicaTracing();
void OnPicaRegWrite(PicaTrace::Write write);
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
 * @param disable_alpha This is used for debug widgets which use this method to display textures
 * without providing a good way to visualize alpha by themselves. If true, this will return 255 for
 * the alpha component, and either drop the information entirely or store it in an "unused" color
 * channel.
 * @todo Eventually we should get rid of the disable_alpha parameter.
 */
const Math::Vec4<u8> LookupTexture(const u8* source, int s, int t, const TextureInfo& info,
                                   bool disable_alpha = false);

void DumpTexture(const Pica::Regs::TextureConfig& texture_config, u8* data);

std::string GetTevStageConfigColorCombinerString(const Pica::Regs::TevStageConfig& tev_stage);
std::string GetTevStageConfigAlphaCombinerString(const Pica::Regs::TevStageConfig& tev_stage);

/// Dumps the Tev stage config to log at trace level
void DumpTevStageConfig(const std::array<Pica::Regs::TevStageConfig, 6>& stages);

/**
 * Used in the vertex loader to merge access records. TODO: Investigate if actually useful.
 */
class MemoryAccessTracker {
    /// Combine overlapping and close ranges
    void SimplifyRanges() {
        for (auto it = ranges.begin(); it != ranges.end(); ++it) {
            // NOTE: We add 32 to the range end address to make sure "close" ranges are combined,
            // too
            auto it2 = std::next(it);
            while (it2 != ranges.end() && it->first + it->second + 32 >= it2->first) {
                it->second = std::max(it->second, it2->first + it2->second - it->first);
                it2 = ranges.erase(it2);
            }
        }
    }

public:
    /// Record a particular memory access in the list
    void AddAccess(u32 paddr, u32 size) {
        // Create new range or extend existing one
        ranges[paddr] = std::max(ranges[paddr], size);

        // Simplify ranges...
        SimplifyRanges();
    }

    /// Map of accessed ranges (mapping start address to range size)
    std::map<u32, u32> ranges;
};

} // namespace

} // namespace

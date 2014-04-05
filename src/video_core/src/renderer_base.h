/**
 * Copyright (C) 2014 Citra Emulator
 *
 * @file    renderer_base.h
 * @author  bunnei
 * @date    2014-04-05
 * @brief   Renderer base class for new video core
 *
 * @section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Official project repository can be found at:
 * http://code.google.com/p/gekko-gc-emu/
 */

#pragma once

#include "common.h"
#include "hash.h"

class RendererBase {
public:

    /// Used to reference a framebuffer
    enum kFramebuffer {
        kFramebuffer_VirtualXFB = 0,
        kFramebuffer_EFB,
        kFramebuffer_Texture
    };

    /// Used for referencing the render modes
    enum kRenderMode {
        kRenderMode_None = 0,
        kRenderMode_Multipass = 1,
        kRenderMode_ZComp = 2,
        kRenderMode_UseDstAlpha = 4
    };

    RendererBase() : current_fps_(0), current_frame_(0) {
    }

    ~RendererBase() {
    }

    /** 
     * Blits the EFB to the external framebuffer (XFB)
     * @param src_rect Source rectangle in EFB to copy
     * @param dst_rect Destination rectangle in EFB to copy to
     * @param dest_height Destination height in pixels
     */
    virtual void CopyToXFB(const Rect& src_rect, const Rect& dst_rect) = 0;

    /**
     * Clear the screen
     * @param rect Screen rectangle to clear
     * @param enable_color Enable color clearing
     * @param enable_alpha Enable alpha clearing
     * @param enable_z Enable depth clearing
     * @param color Clear color
     * @param z Clear depth
     */
    virtual void Clear(const Rect& rect, bool enable_color, bool enable_alpha, bool enable_z, 
        u32 color, u32 z) = 0;

    /**
     * Set a specific render mode
     * @param flag Render flags mode to enable
     */
    virtual void SetMode(kRenderMode flags) = 0;

    /// Reset the full renderer API to the NULL state
    virtual void ResetRenderState() = 0;

    /// Restore the full renderer API state - As the game set it
    virtual void RestoreRenderState() = 0;

    /** 
     * Set the emulator window to use for renderer
     * @param window EmuWindow handle to emulator window to use for rendering
     */
    virtual void SetWindow(EmuWindow* window) = 0;

    /// Initialize the renderer
    virtual void Init() = 0;

    /// Shutdown the renderer
    virtual void ShutDown() = 0;

    /// Converts EFB rectangle coordinates to renderer rectangle coordinates
    //static Rect EFBToRendererRect(const Rect& rect) {
	   // return Rect(rect.x0_, kGCEFBHeight - rect.y0_, rect.x1_, kGCEFBHeight - rect.y1_);
    //}

    // Getter/setter functions:
    // ------------------------

    f32 current_fps() const { return current_fps_; }

    int current_frame() const { return current_frame_; }

protected:
    f32 current_fps_;                       ///< Current framerate, should be set by the renderer
    int current_frame_;                     ///< Current frame, should be set by the renderer

private:
    DISALLOW_COPY_AND_ASSIGN(RendererBase);
};

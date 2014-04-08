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

    RendererBase() : m_current_fps(0), m_current_frame(0) {
    }

    ~RendererBase() {
    }

    /// Swap buffers (render frame)
    virtual void SwapBuffers() = 0;

    /** 
     * Set the emulator window to use for renderer
     * @param window EmuWindow handle to emulator window to use for rendering
     */
    virtual void SetWindow(EmuWindow* window) = 0;

    /// Initialize the renderer
    virtual void Init() = 0;

    /// Shutdown the renderer
    virtual void ShutDown() = 0;

    // Getter/setter functions:
    // ------------------------

    f32 GetCurrentframe() const {
        return m_current_fps;
    }

    int current_frame() const {
        return m_current_frame;
    }

protected:
    f32 m_current_fps;              ///< Current framerate, should be set by the renderer
    int m_current_frame;            ///< Current frame, should be set by the renderer

private:
    DISALLOW_COPY_AND_ASSIGN(RendererBase);
};

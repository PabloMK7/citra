// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Originally written by Sven Peter <sven@fail0verflow.com> for anergistic.

#pragma once

#include "common/common_types.h"
#include "core/hle/kernel/thread.h"

namespace GDBStub {

/// Breakpoint Method
enum class BreakpointType {
    None,    ///< None
    Execute, ///< Execution Breakpoint
    Read,    ///< Read Breakpoint
    Write,   ///< Write Breakpoint
    Access   ///< Access (R/W) Breakpoint
};

struct BreakpointAddress {
    VAddr address;
    BreakpointType type;
};

/**
 * Set the port the gdbstub should use to listen for connections.
 *
 * @param port Port to listen for connection
 */
void SetServerPort(u16 port);

/**
 * Starts or stops the server if possible.
 *
 * @param status Set the server to enabled or disabled.
 */
void ToggleServer(bool status);

/// Start the gdbstub server.
void Init();

/// Stop gdbstub server.
void Shutdown();

/// Checks if the gdbstub server is enabled.
bool IsServerEnabled();

/// Returns true if there is an active socket connection.
bool IsConnected();

/**
 * Signal to the gdbstub server that it should halt CPU execution.
 *
 * @param is_memory_break If true, the break resulted from a memory breakpoint.
 */
void Break(bool is_memory_break = false);

/// Determine if there was a memory breakpoint.
bool IsMemoryBreak();

/// Read and handle packet from gdb client.
void HandlePacket();

/**
 * Get the nearest breakpoint of the specified type at the given address.
 *
 * @param addr Address to search from.
 * @param type Type of breakpoint.
 */
BreakpointAddress GetNextBreakpointFromAddress(VAddr addr, GDBStub::BreakpointType type);

/**
 * Check if a breakpoint of the specified type exists at the given address.
 *
 * @param addr Address of breakpoint.
 * @param type Type of breakpoint.
 */
bool CheckBreakpoint(VAddr addr, GDBStub::BreakpointType type);

// If set to true, the CPU will halt at the beginning of the next CPU loop.
bool GetCpuHaltFlag();

// If set to true and the CPU is halted, the CPU will step one instruction.
bool GetCpuStepFlag();

/**
 * When set to true, the CPU will step one instruction when the CPU is halted next.
 *
 * @param is_step
 */
void SetCpuStepFlag(bool is_step);

/**
 * Send trap signal from thread back to the gdbstub server.
 *
 * @param thread Sending thread.
 * @param trap Trap no.
 */
void SendTrap(Kernel::Thread* thread, int trap);
} // namespace GDBStub

// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Originally written by Sven Peter <sven@fail0verflow.com> for anergistic.

#include <algorithm>
#include <climits>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <numeric>

#ifdef _MSC_VER
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <common/x64/abi.h>
#include <io.h>
#include <iphlpapi.h>
#define SHUT_RDWR 2
#else
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#endif

#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/memory.h"
#include "core/arm/arm_interface.h"
#include "core/gdbstub/gdbstub.h"

const int GDB_BUFFER_SIZE = 10000;

const char GDB_STUB_START = '$';
const char GDB_STUB_END = '#';
const char GDB_STUB_ACK = '+';
const char GDB_STUB_NACK = '-';

#ifndef SIGTRAP
const u32 SIGTRAP = 5;
#endif

#ifndef SIGTERM
const u32 SIGTERM = 15;
#endif

#ifndef MSG_WAITALL
const u32 MSG_WAITALL = 8;
#endif

const u32 R0_REGISTER = 0;
const u32 R15_REGISTER = 15;
const u32 CPSR_REGISTER = 25;
const u32 FPSCR_REGISTER = 58;

// For sample XML files see the GDB source /gdb/features
// GDB also wants the l character at the start
// This XML defines what the registers are for this specific ARM device
static const char* target_xml =
R"(l<?xml version="1.0"?>
<!DOCTYPE target SYSTEM "gdb-target.dtd">
<target version="1.0">
  <feature name="org.gnu.gdb.arm.core">
    <reg name="r0" bitsize="32"/>
    <reg name="r1" bitsize="32"/>
    <reg name="r2" bitsize="32"/>
    <reg name="r3" bitsize="32"/>
    <reg name="r4" bitsize="32"/>
    <reg name="r5" bitsize="32"/>
    <reg name="r6" bitsize="32"/>
    <reg name="r7" bitsize="32"/>
    <reg name="r8" bitsize="32"/>
    <reg name="r9" bitsize="32"/>
    <reg name="r10" bitsize="32"/>
    <reg name="r11" bitsize="32"/>
    <reg name="r12" bitsize="32"/>
    <reg name="sp" bitsize="32" type="data_ptr"/>
    <reg name="lr" bitsize="32"/>
    <reg name="pc" bitsize="32" type="code_ptr"/>

    <!-- The CPSR is register 25, rather than register 16, because
         the FPA registers historically were placed between the PC
         and the CPSR in the "g" packet.  -->

    <reg name="cpsr" bitsize="32" regnum="25"/>
  </feature>
  <feature name="org.gnu.gdb.arm.vfp">
    <reg name="d0" bitsize="64" type="float"/>
    <reg name="d1" bitsize="64" type="float"/>
    <reg name="d2" bitsize="64" type="float"/>
    <reg name="d3" bitsize="64" type="float"/>
    <reg name="d4" bitsize="64" type="float"/>
    <reg name="d5" bitsize="64" type="float"/>
    <reg name="d6" bitsize="64" type="float"/>
    <reg name="d7" bitsize="64" type="float"/>
    <reg name="d8" bitsize="64" type="float"/>
    <reg name="d9" bitsize="64" type="float"/>
    <reg name="d10" bitsize="64" type="float"/>
    <reg name="d11" bitsize="64" type="float"/>
    <reg name="d12" bitsize="64" type="float"/>
    <reg name="d13" bitsize="64" type="float"/>
    <reg name="d14" bitsize="64" type="float"/>
    <reg name="d15" bitsize="64" type="float"/>
    <reg name="fpscr" bitsize="32" type="int" group="float"/>
  </feature>
</target>
)";

namespace GDBStub {

static int gdbserver_socket = -1;

static u8 command_buffer[GDB_BUFFER_SIZE];
static u32 command_length;

static u32 latest_signal = 0;
static bool step_break = false;
static bool memory_break = false;

// Binding to a port within the reserved ports range (0-1023) requires root permissions,
// so default to a port outside of that range.
static u16 gdbstub_port = 24689;

static bool halt_loop = true;
static bool step_loop = false;
std::atomic<bool> g_server_enabled(false);

#ifdef _WIN32
WSADATA InitData;
#endif

struct Breakpoint {
    bool active;
    PAddr addr;
    u32 len;
};

static std::map<u32, Breakpoint> breakpoints_execute;
static std::map<u32, Breakpoint> breakpoints_read;
static std::map<u32, Breakpoint> breakpoints_write;

/**
 * Turns hex string character into the equivalent byte.
 *
 * @param hex Input hex character to be turned into byte.
 */
static u8 HexCharToValue(u8 hex) {
    if (hex >= '0' && hex <= '9') {
        return hex - '0';
    } else if (hex >= 'a' && hex <= 'f') {
        return hex - 'a' + 0xA;
    } else if (hex >= 'A' && hex <= 'F') {
        return hex - 'A' + 0xA;
    }

    LOG_ERROR(Debug_GDBStub, "Invalid nibble: %c (%02x)\n", hex, hex);
    return 0;
}

/**
 * Turn nibble of byte into hex string character.
 *
 * @param n Nibble to be turned into hex character.
 */
static u8 NibbleToHex(u8 n) {
    n &= 0xF;
    if (n < 0xA) {
        return '0' + n;
    } else {
        return 'A' + n - 0xA;
    }
}

/**
* Converts input hex string characters into an array of equivalent of u8 bytes.
*
* @param dest Pointer to buffer to store u8 bytes.
* @param src Pointer to array of output hex string characters.
* @param len Length of src array.
*/
static u32 HexToInt(u8* src, u32 len) {
    u32 output = 0;
    while (len-- > 0) {
        output = (output << 4) | HexCharToValue(src[0]);
        src++;
    }
    return output;
}

/**
 * Converts input array of u8 bytes into their equivalent hex string characters.
 *
 * @param dest Pointer to buffer to store output hex string characters.
 * @param src Pointer to array of u8 bytes.
 * @param len Length of src array.
 */
static void MemToGdbHex(u8* dest, u8* src, u32 len) {
    while (len-- > 0) {
        u8 tmp = *src++;
        *dest++ = NibbleToHex(tmp >> 4);
        *dest++ = NibbleToHex(tmp);
    }
}

/**
 * Converts input gdb-formatted hex string characters into an array of equivalent of u8 bytes.
 *
 * @param dest Pointer to buffer to store u8 bytes.
 * @param src Pointer to array of output hex string characters.
 * @param len Length of src array.
 */
static void GdbHexToMem(u8* dest, u8* src, u32 len) {
    while (len-- > 0) {
        *dest++ = (HexCharToValue(src[0]) << 4) | HexCharToValue(src[1]);
        src += 2;
    }
}

/**
 * Convert a u32 into a gdb-formatted hex string.
 *
 * @param dest Pointer to buffer to store output hex string characters.
 */
static void IntToGdbHex(u8* dest, u32 v) {
    for (int i = 0; i < 8; i += 2) {
        dest[i + 1] = NibbleToHex(v >> (4 * i));
        dest[i] = NibbleToHex(v >> (4 * (i + 1)));
    }
}

/**
 * Convert a gdb-formatted hex string into a u32.
 *
 * @param src Pointer to hex string.
 */
static u32 GdbHexToInt(u8* src) {
    u32 output = 0;

    for (int i = 0; i < 8; i += 2) {
        output = (output << 4) | HexCharToValue(src[7 - i - 1]);
        output = (output << 4) | HexCharToValue(src[7 - i]);
    }

    return output;
}

/// Read a byte from the gdb client.
static u8 ReadByte() {
    u8 c;
    size_t received_size = recv(gdbserver_socket, reinterpret_cast<char*>(&c), 1, MSG_WAITALL);
    if (received_size != 1) {
        LOG_ERROR(Debug_GDBStub, "recv failed : %ld", received_size);
        Shutdown();
    }

    return c;
}

/// Calculate the checksum of the current command buffer.
static u8 CalculateChecksum(u8* buffer, u32 length) {
    return static_cast<u8>(std::accumulate(buffer, buffer + length, 0, std::plus<u8>()));
}

/**
 * Get the list of breakpoints for a given breakpoint type.
 *
 * @param type Type of breakpoint list.
 */
static std::map<u32, Breakpoint>& GetBreakpointList(BreakpointType type) {
    switch (type) {
    case BreakpointType::Execute:
        return breakpoints_execute;
    case BreakpointType::Read:
        return breakpoints_read;
    case BreakpointType::Write:
        return breakpoints_write;
    default:
        return breakpoints_read;
    }
}

/**
 * Remove the breakpoint from the given address of the specified type.
 *
 * @param type Type of breakpoint.
 * @param addr Address of breakpoint.
 */
static void RemoveBreakpoint(BreakpointType type, PAddr addr) {
    std::map<u32, Breakpoint>& p = GetBreakpointList(type);

    auto bp = p.find(addr);
    if (bp != p.end()) {
        LOG_DEBUG(Debug_GDBStub, "gdb: removed a breakpoint: %08x bytes at %08x of type %d\n", bp->second.len, bp->second.addr, type);
        p.erase(addr);
    }
}

BreakpointAddress GetNextBreakpointFromAddress(PAddr addr, BreakpointType type) {
    std::map<u32, Breakpoint>& p = GetBreakpointList(type);
    auto next_breakpoint = p.lower_bound(addr);
    BreakpointAddress breakpoint;

    if (next_breakpoint != p.end()) {
        breakpoint.address = next_breakpoint->first;
        breakpoint.type = type;
    } else {
        breakpoint.address = 0;
        breakpoint.type = BreakpointType::None;
    }

    return breakpoint;
}

bool CheckBreakpoint(PAddr addr, BreakpointType type) {
    if (!IsConnected()) {
        return false;
    }

    std::map<u32, Breakpoint>& p = GetBreakpointList(type);

    auto bp = p.find(addr);
    if (bp != p.end()) {
        u32 len = bp->second.len;

        // IDA Pro defaults to 4-byte breakpoints for all non-hardware breakpoints
        // no matter if it's a 4-byte or 2-byte instruction. When you execute a
        // Thumb instruction with a 4-byte breakpoint set, it will set a breakpoint on
        // two instructions instead of the single instruction you placed the breakpoint
        // on. So, as a way to make sure that execution breakpoints are only breaking
        // on the instruction that was specified, set the length of an execution
        // breakpoint to 1. This should be fine since the CPU should never begin executing
        // an instruction anywhere except the beginning of the instruction.
        if (type == BreakpointType::Execute) {
            len = 1;
        }

        if (bp->second.active && (addr >= bp->second.addr && addr < bp->second.addr + len)) {
            LOG_DEBUG(Debug_GDBStub, "Found breakpoint type %d @ %08x, range: %08x - %08x (%d bytes)\n", type, addr, bp->second.addr, bp->second.addr + len, len);
            return true;
        }
    }

    return false;
}

/**
 * Send packet to gdb client.
 *
 * @param packet Packet to be sent to client.
 */
static void SendPacket(const char packet) {
    size_t sent_size = send(gdbserver_socket, &packet, 1, 0);
    if (sent_size != 1) {
        LOG_ERROR(Debug_GDBStub, "send failed");
    }
}

/**
 * Send reply to gdb client.
 *
 * @param reply Reply to be sent to client.
 */
static void SendReply(const char* reply) {
    if (!IsConnected()) {
        return;
    }

    memset(command_buffer, 0, sizeof(command_buffer));

    command_length = static_cast<u32>(strlen(reply));
    if (command_length + 4 > sizeof(command_buffer)) {
        LOG_ERROR(Debug_GDBStub, "command_buffer overflow in SendReply");
        return;
    }

    memcpy(command_buffer + 1, reply, command_length);

    u8 checksum = CalculateChecksum(command_buffer, command_length + 1);
    command_buffer[0] = GDB_STUB_START;
    command_buffer[command_length + 1] = GDB_STUB_END;
    command_buffer[command_length + 2] = NibbleToHex(checksum >> 4);
    command_buffer[command_length + 3] = NibbleToHex(checksum);

    u8* ptr = command_buffer;
    u32 left = command_length + 4;
    while (left > 0) {
        int sent_size = send(gdbserver_socket, reinterpret_cast<char*>(ptr), left, 0);
        if (sent_size < 0) {
            LOG_ERROR(Debug_GDBStub, "gdb: send failed");
            return Shutdown();
        }

        left -= sent_size;
        ptr += sent_size;
    }
}

/// Handle query command from gdb client.
static void HandleQuery() {
    LOG_DEBUG(Debug_GDBStub, "gdb: query '%s'\n", command_buffer + 1);

    const char* query = reinterpret_cast<const char*>(command_buffer + 1);

    if (strcmp(query, "TStatus") == 0 ) {
        SendReply("T0");
    } else if (strncmp(query, "Supported:", strlen("Supported:")) == 0) {
        // PacketSize needs to be large enough for target xml
        SendReply("PacketSize=800;qXfer:features:read+");
    } else if (strncmp(query, "Xfer:features:read:target.xml:", strlen("Xfer:features:read:target.xml:")) == 0) {
        SendReply(target_xml);
    } else {
        SendReply("");
    }
}

/// Handle set thread command from gdb client.
static void HandleSetThread() {
    if (memcmp(command_buffer, "Hg0", 3) == 0 ||
        memcmp(command_buffer, "Hc-1", 4) == 0 ||
        memcmp(command_buffer, "Hc0", 4) == 0 ||
        memcmp(command_buffer, "Hc1", 4) == 0) {
        return SendReply("OK");
    }

    SendReply("E01");
}

/**
 * Send signal packet to client.
 *
 * @param signal Signal to be sent to client.
 */
static void SendSignal(u32 signal) {
    if (gdbserver_socket == -1) {
        return;
    }

    latest_signal = signal;

    std::string buffer = Common::StringFromFormat("T%02x%02x:%08x;%02x:%08x;", latest_signal, 15, htonl(Core::g_app_core->GetPC()), 13, htonl(Core::g_app_core->GetReg(13)));
    LOG_DEBUG(Debug_GDBStub, "Response: %s", buffer.c_str());
    SendReply(buffer.c_str());
}

/// Read command from gdb client.
static void ReadCommand() {
    command_length = 0;
    memset(command_buffer, 0, sizeof(command_buffer));

    u8 c = ReadByte();
    if (c == '+') {
        //ignore ack
        return;
    } else if (c == 0x03) {
        LOG_INFO(Debug_GDBStub, "gdb: found break command\n");
        halt_loop = true;
        SendSignal(SIGTRAP);
        return;
    } else if (c != GDB_STUB_START) {
        LOG_DEBUG(Debug_GDBStub, "gdb: read invalid byte %02x\n", c);
        return;
    }

    while ((c = ReadByte()) != GDB_STUB_END) {
        if (command_length >= sizeof(command_buffer)) {
            LOG_ERROR(Debug_GDBStub, "gdb: command_buffer overflow\n");
            SendPacket(GDB_STUB_NACK);
            return;
        }
        command_buffer[command_length++] = c;
    }

    u8 checksum_received = HexCharToValue(ReadByte()) << 4;
    checksum_received |= HexCharToValue(ReadByte());

    u8 checksum_calculated = CalculateChecksum(command_buffer, command_length);

    if (checksum_received != checksum_calculated) {
        LOG_ERROR(Debug_GDBStub, "gdb: invalid checksum: calculated %02x and read %02x for $%s# (length: %d)\n",
            checksum_calculated, checksum_received, command_buffer, command_length);

        command_length = 0;

        SendPacket(GDB_STUB_NACK);
        return;
    }

    SendPacket(GDB_STUB_ACK);
}

/// Check if there is data to be read from the gdb client.
static bool IsDataAvailable() {
    if (!IsConnected()) {
        return false;
    }

    fd_set fd_socket;

    FD_ZERO(&fd_socket);
    FD_SET(gdbserver_socket, &fd_socket);

    struct timeval t;
    t.tv_sec = 0;
    t.tv_usec = 0;

    if (select(gdbserver_socket + 1, &fd_socket, nullptr, nullptr, &t) < 0) {
        LOG_ERROR(Debug_GDBStub, "select failed");
        return false;
    }

    return FD_ISSET(gdbserver_socket, &fd_socket) != 0;
}

/// Send requested register to gdb client.
static void ReadRegister() {
    static u8 reply[64];
    memset(reply, 0, sizeof(reply));

    u32 id = HexCharToValue(command_buffer[1]);
    if (command_buffer[2] != '\0') {
        id <<= 4;
        id |= HexCharToValue(command_buffer[2]);
    }

    if (id <= R15_REGISTER) {
        IntToGdbHex(reply, Core::g_app_core->GetReg(id));
    } else if (id == CPSR_REGISTER) {
        IntToGdbHex(reply, Core::g_app_core->GetCPSR());
    } else if (id > CPSR_REGISTER && id < FPSCR_REGISTER) {
        IntToGdbHex(reply, Core::g_app_core->GetVFPReg(id - CPSR_REGISTER - 1)); // VFP registers should start at 26, so one after CSPR_REGISTER
    } else if (id == FPSCR_REGISTER) {
        IntToGdbHex(reply, Core::g_app_core->GetVFPSystemReg(VFP_FPSCR)); // Get FPSCR
        IntToGdbHex(reply + 8, 0);
    } else {
        return SendReply("E01");
    }

    SendReply(reinterpret_cast<char*>(reply));
}

/// Send all registers to the gdb client.
static void ReadRegisters() {
    static u8 buffer[GDB_BUFFER_SIZE - 4];
    memset(buffer, 0, sizeof(buffer));

    u8* bufptr = buffer;

    for (int reg = 0; reg <= R15_REGISTER; reg++) {
        IntToGdbHex(bufptr + reg * CHAR_BIT, Core::g_app_core->GetReg(reg));
    }

    bufptr += (16 * CHAR_BIT);

    IntToGdbHex(bufptr, Core::g_app_core->GetCPSR());

    bufptr += CHAR_BIT;

    for (int reg = 0; reg <= 31; reg++) {
        IntToGdbHex(bufptr + reg * CHAR_BIT, Core::g_app_core->GetVFPReg(reg));
    }

    bufptr += (32 * CHAR_BIT);

    IntToGdbHex(bufptr, Core::g_app_core->GetVFPSystemReg(VFP_FPSCR));

    SendReply(reinterpret_cast<char*>(buffer));
}

/// Modify data of register specified by gdb client.
static void WriteRegister() {
    u8* buffer_ptr = command_buffer + 3;

    u32 id = HexCharToValue(command_buffer[1]);
    if (command_buffer[2] != '=') {
        ++buffer_ptr;
        id <<= 4;
        id |= HexCharToValue(command_buffer[2]);
    }

    if (id <= R15_REGISTER) {
        Core::g_app_core->SetReg(id, GdbHexToInt(buffer_ptr));
    } else if (id == CPSR_REGISTER) {
        Core::g_app_core->SetCPSR(GdbHexToInt(buffer_ptr));
    } else if (id > CPSR_REGISTER && id < FPSCR_REGISTER) {
        Core::g_app_core->SetVFPReg(id - CPSR_REGISTER - 1, GdbHexToInt(buffer_ptr));
    } else if (id == FPSCR_REGISTER) {
        Core::g_app_core->SetVFPSystemReg(VFP_FPSCR, GdbHexToInt(buffer_ptr));
    } else {
        return SendReply("E01");
    }

    SendReply("OK");
}

/// Modify all registers with data received from the client.
static void WriteRegisters() {
    u8* buffer_ptr = command_buffer + 1;

    if (command_buffer[0] != 'G')
        return SendReply("E01");

    for (int i = 0, reg = 0; reg <= FPSCR_REGISTER; i++, reg++) {
        if (reg <= R15_REGISTER) {
            Core::g_app_core->SetReg(reg, GdbHexToInt(buffer_ptr + i * CHAR_BIT));
        } else if (reg == CPSR_REGISTER) {
            Core::g_app_core->SetCPSR(GdbHexToInt(buffer_ptr + i * CHAR_BIT));
        } else if (reg == CPSR_REGISTER - 1) {
            // Dummy FPA register, ignore
        } else if (reg < CPSR_REGISTER) {
            // Dummy FPA registers, ignore
            i += 2;
        } else if (reg > CPSR_REGISTER && reg < FPSCR_REGISTER) {
            Core::g_app_core->SetVFPReg(reg - CPSR_REGISTER - 1, GdbHexToInt(buffer_ptr + i * CHAR_BIT));
            i++; // Skip padding
        } else if (reg == FPSCR_REGISTER) {
            Core::g_app_core->SetVFPSystemReg(VFP_FPSCR, GdbHexToInt(buffer_ptr + i * CHAR_BIT));
        }
    }

    SendReply("OK");
}

/// Read location in memory specified by gdb client.
static void ReadMemory() {
    static u8 reply[GDB_BUFFER_SIZE - 4];

    auto start_offset = command_buffer+1;
    auto addr_pos = std::find(start_offset, command_buffer+command_length, ',');
    PAddr addr = HexToInt(start_offset, static_cast<u32>(addr_pos - start_offset));

    start_offset = addr_pos+1;
    u32 len = HexToInt(start_offset, static_cast<u32>((command_buffer + command_length) - start_offset));

    LOG_DEBUG(Debug_GDBStub, "gdb: addr: %08x len: %08x\n", addr, len);

    if (len * 2 > sizeof(reply)) {
        SendReply("E01");
    }

    u8* data = Memory::GetPointer(addr);
    if (!data) {
        return SendReply("E00");
    }

    MemToGdbHex(reply, data, len);
    reply[len * 2] = '\0';
    SendReply(reinterpret_cast<char*>(reply));
}

/// Modify location in memory with data received from the gdb client.
static void WriteMemory() {
    auto start_offset = command_buffer+1;
    auto addr_pos = std::find(start_offset, command_buffer+command_length, ',');
    PAddr addr = HexToInt(start_offset, static_cast<u32>(addr_pos - start_offset));

    start_offset = addr_pos+1;
    auto len_pos = std::find(start_offset, command_buffer+command_length, ':');
    u32 len = HexToInt(start_offset, static_cast<u32>(len_pos - start_offset));

    u8* dst = Memory::GetPointer(addr);
    if (!dst) {
        return SendReply("E00");
    }

    GdbHexToMem(dst, len_pos + 1, len);
    SendReply("OK");
}

void Break(bool is_memory_break) {
    if (!halt_loop) {
        halt_loop = true;
        SendSignal(SIGTRAP);
    }

    memory_break = is_memory_break;
}

/// Tell the CPU that it should perform a single step.
static void Step() {
    step_loop = true;
    halt_loop = true;
    step_break = true;
    SendSignal(SIGTRAP);
}

bool IsMemoryBreak() {
    if (IsConnected()) {
        return false;
    }

    return memory_break;
}

/// Tell the CPU to continue executing.
static void Continue() {
    memory_break = false;
    step_break = false;
    step_loop = false;
    halt_loop = false;
}

/**
 * Commit breakpoint to list of breakpoints.
 *
 * @param type Type of breakpoint.
 * @param addr Address of breakpoint.
 * @param len Length of breakpoint.
 */
static bool CommitBreakpoint(BreakpointType type, PAddr addr, u32 len) {
    std::map<u32, Breakpoint>& p = GetBreakpointList(type);

    Breakpoint breakpoint;
    breakpoint.active = true;
    breakpoint.addr = addr;
    breakpoint.len = len;
    p.insert({ addr, breakpoint });

    LOG_DEBUG(Debug_GDBStub, "gdb: added %d breakpoint: %08x bytes at %08x\n", type, breakpoint.len, breakpoint.addr);

    return true;
}

/// Handle add breakpoint command from gdb client.
static void AddBreakpoint() {
    BreakpointType type;

    u8 type_id = HexCharToValue(command_buffer[1]);
    switch (type_id) {
    case 0:
    case 1:
        type = BreakpointType::Execute;
        break;
    case 2:
        type = BreakpointType::Write;
        break;
    case 3:
        type = BreakpointType::Read;
        break;
    case 4:
        type = BreakpointType::Access;
        break;
    default:
        return SendReply("E01");
    }

    auto start_offset = command_buffer+3;
    auto addr_pos = std::find(start_offset, command_buffer+command_length, ',');
    PAddr addr = HexToInt(start_offset, static_cast<u32>(addr_pos - start_offset));

    start_offset = addr_pos+1;
    u32 len = HexToInt(start_offset, static_cast<u32>((command_buffer + command_length) - start_offset));

    if (type == BreakpointType::Access) {
        // Access is made up of Read and Write types, so add both breakpoints
        type = BreakpointType::Read;

        if (!CommitBreakpoint(type, addr, len)) {
            return SendReply("E02");
        }

        type = BreakpointType::Write;
    }

    if (!CommitBreakpoint(type, addr, len)) {
        return SendReply("E02");
    }

    SendReply("OK");
}

/// Handle remove breakpoint command from gdb client.
static void RemoveBreakpoint() {
    BreakpointType type;

    u8 type_id = HexCharToValue(command_buffer[1]);
    switch (type_id) {
    case 0:
    case 1:
        type = BreakpointType::Execute;
        break;
    case 2:
        type = BreakpointType::Write;
        break;
    case 3:
        type = BreakpointType::Read;
        break;
    case 4:
        type = BreakpointType::Access;
        break;
    default:
        return SendReply("E01");
    }

    auto start_offset = command_buffer+3;
    auto addr_pos = std::find(start_offset, command_buffer+command_length, ',');
    PAddr addr = HexToInt(start_offset, static_cast<u32>(addr_pos - start_offset));

    start_offset = addr_pos+1;
    u32 len = HexToInt(start_offset, static_cast<u32>((command_buffer + command_length) - start_offset));

    if (type == BreakpointType::Access) {
        // Access is made up of Read and Write types, so add both breakpoints
        type = BreakpointType::Read;
        RemoveBreakpoint(type, addr);

        type = BreakpointType::Write;
    }

    RemoveBreakpoint(type, addr);
    SendReply("OK");
}

void HandlePacket() {
    if (!IsConnected()) {
        return;
    }

    if (!IsDataAvailable()) {
        return;
    }

    ReadCommand();
    if (command_length == 0) {
        return;
    }

    LOG_DEBUG(Debug_GDBStub, "Packet: %s", command_buffer);

    switch (command_buffer[0]) {
    case 'q':
        HandleQuery();
        break;
    case 'H':
        HandleSetThread();
        break;
    case '?':
        SendSignal(latest_signal);
        break;
    case 'k':
        Shutdown();
        LOG_INFO(Debug_GDBStub, "killed by gdb");
        return;
    case 'g':
        ReadRegisters();
        break;
    case 'G':
        WriteRegisters();
        break;
    case 'p':
        ReadRegister();
        break;
    case 'P':
        WriteRegister();
        break;
    case 'm':
        ReadMemory();
        break;
    case 'M':
        WriteMemory();
        break;
    case 's':
        Step();
        return;
    case 'C':
    case 'c':
        Continue();
        return;
    case 'z':
        RemoveBreakpoint();
        break;
    case 'Z':
        AddBreakpoint();
        break;
    default:
        SendReply("");
        break;
    }
}

void SetServerPort(u16 port) {
    gdbstub_port = port;
}

void ToggleServer(bool status) {
    if (status) {
        g_server_enabled = status;

        // Start server
        if (!IsConnected() && Core::g_sys_core != nullptr) {
            Init();
        }
    }
    else {
        // Stop server
        if (IsConnected()) {
            Shutdown();
        }

        g_server_enabled = status;
    }
}

static void Init(u16 port) {
    if (!g_server_enabled) {
        // Set the halt loop to false in case the user enabled the gdbstub mid-execution.
        // This way the CPU can still execute normally.
        halt_loop = false;
        step_loop = false;
        return;
    }

    // Setup initial gdbstub status
    halt_loop = true;
    step_loop = false;

    breakpoints_execute.clear();
    breakpoints_read.clear();
    breakpoints_write.clear();

    // Start gdb server
    LOG_INFO(Debug_GDBStub, "Starting GDB server on port %d...", port);

    sockaddr_in saddr_server = {};
    saddr_server.sin_family = AF_INET;
    saddr_server.sin_port = htons(port);
    saddr_server.sin_addr.s_addr = INADDR_ANY;

#ifdef _WIN32
    WSAStartup(MAKEWORD(2, 2), &InitData);
#endif

    int tmpsock = socket(PF_INET, SOCK_STREAM, 0);
    if (tmpsock == -1) {
        LOG_ERROR(Debug_GDBStub, "Failed to create gdb socket");
    }

    // Set socket to SO_REUSEADDR so it can always bind on the same port
    int reuse_enabled = 1;
    if (setsockopt(tmpsock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse_enabled, sizeof(reuse_enabled)) < 0) {
        LOG_ERROR(Debug_GDBStub, "Failed to set gdb socket option");
    }

    const sockaddr* server_addr = reinterpret_cast<const sockaddr*>(&saddr_server);
    socklen_t server_addrlen = sizeof(saddr_server);
    if (bind(tmpsock, server_addr, server_addrlen) < 0) {
        LOG_ERROR(Debug_GDBStub, "Failed to bind gdb socket");
    }

    if (listen(tmpsock, 1) < 0) {
        LOG_ERROR(Debug_GDBStub, "Failed to listen to gdb socket");
    }

    // Wait for gdb to connect
    LOG_INFO(Debug_GDBStub, "Waiting for gdb to connect...\n");
    sockaddr_in saddr_client;
    sockaddr* client_addr = reinterpret_cast<sockaddr*>(&saddr_client);
    socklen_t client_addrlen = sizeof(saddr_client);
    gdbserver_socket = accept(tmpsock, client_addr, &client_addrlen);
    if (gdbserver_socket < 0) {
        // In the case that we couldn't start the server for whatever reason, just start CPU execution like normal.
        halt_loop = false;
        step_loop = false;

        LOG_ERROR(Debug_GDBStub, "Failed to accept gdb client");
    }
    else {
        LOG_INFO(Debug_GDBStub, "Client connected.\n");
        saddr_client.sin_addr.s_addr = ntohl(saddr_client.sin_addr.s_addr);
    }

    // Clean up temporary socket if it's still alive at this point.
    if (tmpsock != -1) {
        shutdown(tmpsock, SHUT_RDWR);
    }
}

void Init() {
    Init(gdbstub_port);
}

void Shutdown() {
    if (!g_server_enabled) {
        return;
    }

    LOG_INFO(Debug_GDBStub, "Stopping GDB ...");
    if (gdbserver_socket != -1) {
        shutdown(gdbserver_socket, SHUT_RDWR);
        gdbserver_socket = -1;
    }

#ifdef _WIN32
    WSACleanup();
#endif

    LOG_INFO(Debug_GDBStub, "GDB stopped.");
}

bool IsConnected() {
    return g_server_enabled && gdbserver_socket != -1;
}

bool GetCpuHaltFlag() {
    return halt_loop;
}

bool GetCpuStepFlag() {
    return step_loop;
}

void SetCpuStepFlag(bool is_step) {
    step_loop = is_step;
}

};

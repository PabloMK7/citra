// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <functional>
#include <vector>

#include "common/log.h"

#include "core/hle/service/gsp.h"
#include "pica.h"

class GraphicsDebugger
{
public:
    // A few utility structs used to expose data
    // A vector of commands represented by their raw byte sequence
    struct PicaCommand : public std::vector<u32>
    {
        const Pica::CommandHeader& GetHeader() const
        {
            const u32& val = at(1);
            return *(Pica::CommandHeader*)&val;
        }
    };

    typedef std::vector<PicaCommand> PicaCommandList;

    // Base class for all objects which need to be notified about GPU events
    class DebuggerObserver
    {
    public:
        DebuggerObserver() : observed(nullptr) { }

        virtual ~DebuggerObserver()
        {
            if (observed)
                observed->UnregisterObserver(this);
        }

        /**
        * Called when a GX command has been processed and is ready for being
        * read via GraphicsDebugger::ReadGXCommandHistory.
        * @param total_command_count Total number of commands in the GX history
        * @note All methods in this class are called from the GSP thread
        */
        virtual void GXCommandProcessed(int total_command_count)
        {
            const GSP_GPU::Command& cmd = observed->ReadGXCommandHistory(total_command_count-1);
            ERROR_LOG(GSP, "Received command: id=%x", (int)cmd.id.Value());
        }

        /**
        * @param lst command list which triggered this call
        * @param is_new true if the command list was called for the first time
        * @todo figure out how to make sure called functions don't keep references around beyond their life time
        */
        virtual void OnCommandListCalled(const PicaCommandList& lst, bool is_new)
        {
            ERROR_LOG(GSP, "Command list called: %d", (int)is_new);
        }

    protected:
        const GraphicsDebugger* GetDebugger() const
        {
            return observed;
        }

    private:
        GraphicsDebugger* observed;
        bool in_destruction;

        friend class GraphicsDebugger;
    };

    void GXCommandProcessed(u8* command_data)
    {
        if (observers.empty())
            return;

        gx_command_history.push_back(GSP_GPU::Command());
        GSP_GPU::Command& cmd = gx_command_history[gx_command_history.size()-1];

        memcpy(&cmd, command_data, sizeof(GSP_GPU::Command));

        ForEachObserver([this](DebuggerObserver* observer) {
                          observer->GXCommandProcessed(this->gx_command_history.size());
                        } );
    }

    void CommandListCalled(u32 address, u32* command_list, u32 size_in_words)
    {
        if (observers.empty())
            return;

        PicaCommandList cmdlist;
        for (u32* parse_pointer = command_list; parse_pointer < command_list + size_in_words;)
        {
            const Pica::CommandHeader header = static_cast<Pica::CommandHeader>(parse_pointer[1]);

            cmdlist.push_back(PicaCommand());
            auto& cmd = cmdlist.back();

            size_t size = 2 + header.extra_data_length;
            size = (size + 1) / 2 * 2; // align to 8 bytes
            cmd.reserve(size);
            std::copy(parse_pointer, parse_pointer + size, std::back_inserter(cmd));

            parse_pointer += size;
        }

        auto obj = std::pair<u32,PicaCommandList>(address, cmdlist);
        auto it = std::find(command_lists.begin(), command_lists.end(), obj);
        bool is_new = (it == command_lists.end());
        if (is_new)
            command_lists.push_back(obj);

        ForEachObserver([&](DebuggerObserver* observer) {
                            observer->OnCommandListCalled(obj.second, is_new);
                        } );
    }

    const GSP_GPU::Command& ReadGXCommandHistory(int index) const
    {
        // TODO: Is this thread-safe?
        return gx_command_history[index];
    }

    const std::vector<std::pair<u32,PicaCommandList>>& GetCommandLists() const
    {
        return command_lists;
    }

    void RegisterObserver(DebuggerObserver* observer)
    {
        // TODO: Check for duplicates
        observers.push_back(observer);
        observer->observed = this;
    }

    void UnregisterObserver(DebuggerObserver* observer)
    {
        std::remove(observers.begin(), observers.end(), observer);
        observer->observed = nullptr;
    }

private:
    void ForEachObserver(std::function<void (DebuggerObserver*)> func)
    {
        std::for_each(observers.begin(),observers.end(), func);
    }

    std::vector<DebuggerObserver*> observers;

    std::vector<GSP_GPU::Command> gx_command_history;

    // vector of pairs of command lists and their storage address
    std::vector<std::pair<u32,PicaCommandList>> command_lists;
};

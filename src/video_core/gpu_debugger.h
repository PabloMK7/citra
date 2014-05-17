// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <functional>
#include <vector>

#include "common/log.h"

#include "core/hle/service/gsp.h"


class GraphicsDebugger
{
public:
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
            const GSP_GPU::GXCommand& cmd = observed->ReadGXCommandHistory(total_command_count-1);
            ERROR_LOG(GSP, "Received command: id=%x", cmd.id);
        }

    protected:
        GraphicsDebugger* GetDebugger()
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
        gx_command_history.push_back(GSP_GPU::GXCommand());
        GSP_GPU::GXCommand& cmd = gx_command_history[gx_command_history.size()-1];

        const int cmd_length = sizeof(GSP_GPU::GXCommand);
        memcpy(cmd.data, command_data, cmd_length);

        ForEachObserver([this](DebuggerObserver* observer) {
                          observer->GXCommandProcessed(this->gx_command_history.size());
                        } );
    }

    const GSP_GPU::GXCommand& ReadGXCommandHistory(int index) const
    {
        // TODO: Is this thread-safe?
        return gx_command_history[index];
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

    std::vector<GSP_GPU::GXCommand> gx_command_history;
};

// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/hle/result.h"
#include "core/hle/service/apt/applet_manager.h"

namespace HLE {
namespace Applets {

class Applet {
public:
    virtual ~Applet() = default;

    /**
     * Creates an instance of the Applet subclass identified by the parameter.
     * and stores it in a global map.
     * @param id Id of the applet to create.
     * @returns ResultCode Whether the operation was successful or not.
     */
    static ResultCode Create(Service::APT::AppletId id,
                             std::weak_ptr<Service::APT::AppletManager> manager);

    /**
     * Retrieves the Applet instance identified by the specified id.
     * @param id Id of the Applet to retrieve.
     * @returns Requested Applet or nullptr if not found.
     */
    static std::shared_ptr<Applet> Get(Service::APT::AppletId id);

    /**
     * Handles a parameter from the application.
     * @param parameter Parameter data to handle.
     * @returns ResultCode Whether the operation was successful or not.
     */
    virtual ResultCode ReceiveParameter(const Service::APT::MessageParameter& parameter) = 0;

    /**
     * Handles the Applet start event, triggered from the application.
     * @param parameter Parameter data to handle.
     * @returns ResultCode Whether the operation was successful or not.
     */
    ResultCode Start(const Service::APT::AppletStartupParameter& parameter);

    /**
     * Whether the applet is currently executing instead of the host application or not.
     */
    bool IsRunning() const;

    /**
     * Handles an update tick for the Applet, lets it update the screen, send commands, etc.
     */
    virtual void Update() = 0;

protected:
    Applet(Service::APT::AppletId id, std::weak_ptr<Service::APT::AppletManager> manager)
        : id(id), manager(std::move(manager)) {}

    /**
     * Handles the Applet start event, triggered from the application.
     * @param parameter Parameter data to handle.
     * @returns ResultCode Whether the operation was successful or not.
     */
    virtual ResultCode StartImpl(const Service::APT::AppletStartupParameter& parameter) = 0;

    Service::APT::AppletId id;                    ///< Id of this Applet
    std::shared_ptr<std::vector<u8>> heap_memory; ///< Heap memory for this Applet

    /// Whether this applet is currently running instead of the host application or not.
    bool is_running = false;

    void SendParameter(const Service::APT::MessageParameter& parameter);

private:
    std::weak_ptr<Service::APT::AppletManager> manager;
};

/// Returns whether a library applet is currently running
bool IsLibraryAppletRunning();

/// Initializes the HLE applets
void Init();

/// Shuts down the HLE applets
void Shutdown();
}
} // namespace

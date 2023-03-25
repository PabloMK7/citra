// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/hle/result.h"
#include "core/hle/service/apt/applet_manager.h"

namespace HLE::Applets {

class Applet {
public:
    virtual ~Applet() = default;

    /**
     * Creates an instance of the Applet subclass identified by the parameter.
     * and stores it in a global map.
     * @param id Id of the applet to create.
     * @param parent Id of the applet's parent.
     * @param preload Whether the applet is being preloaded.
     * @returns ResultCode Whether the operation was successful or not.
     */
    static ResultCode Create(Service::APT::AppletId id, Service::APT::AppletId parent, bool preload,
                             const std::shared_ptr<Service::APT::AppletManager>& manager);

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
    ResultCode ReceiveParameter(const Service::APT::MessageParameter& parameter);

    /**
     * Whether the applet is currently running.
     */
    [[nodiscard]] bool IsRunning() const;

    /**
     * Whether the applet is currently active instead of the host application or not.
     */
    [[nodiscard]] bool IsActive() const;

    /**
     * Handles an update tick for the Applet, lets it update the screen, send commands, etc.
     */
    virtual void Update() = 0;

protected:
    Applet(Service::APT::AppletId id, Service::APT::AppletId parent, bool preload,
           std::weak_ptr<Service::APT::AppletManager> manager)
        : id(id), parent(parent), preload(preload), manager(std::move(manager)) {}

    /**
     * Handles a parameter from the application.
     * @param parameter Parameter data to handle.
     * @returns ResultCode Whether the operation was successful or not.
     */
    virtual ResultCode ReceiveParameterImpl(const Service::APT::MessageParameter& parameter) = 0;

    /**
     * Handles the Applet start event, triggered from the application.
     * @param parameter Parameter data to handle.
     * @returns ResultCode Whether the operation was successful or not.
     */
    virtual ResultCode Start(const Service::APT::MessageParameter& parameter) = 0;

    /**
     * Sends the LibAppletClosing signal to the application,
     * along with the relevant data buffers.
     */
    virtual ResultCode Finalize() = 0;

    Service::APT::AppletId id;                    ///< Id of this Applet
    Service::APT::AppletId parent;                ///< Id of this Applet's parent
    bool preload;                                 ///< Whether the Applet is being preloaded.
    std::shared_ptr<std::vector<u8>> heap_memory; ///< Heap memory for this Applet

    /// Whether this applet is running.
    bool is_running = true;

    /// Whether this applet is currently active instead of the host application or not.
    bool is_active = false;

    void SendParameter(const Service::APT::MessageParameter& parameter);
    void CloseApplet(std::shared_ptr<Kernel::Object> object, const std::vector<u8>& buffer);

private:
    std::weak_ptr<Service::APT::AppletManager> manager;
};

/// Initializes the HLE applets
void Init();

/// Shuts down the HLE applets
void Shutdown();
} // namespace HLE::Applets

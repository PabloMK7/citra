// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include "common/logging/log.h"
#include "common/param_package.h"

namespace Input {

/// An abstract class template for an input device (a button, an analog input, etc.).
template <typename StatusType>
class InputDevice {
public:
    virtual ~InputDevice() = default;
    virtual StatusType GetStatus() const {
        return {};
    }
};

/// An abstract class template for a factory that can create input devices.
template <typename InputDeviceType>
class Factory {
public:
    virtual ~Factory() = default;
    virtual std::unique_ptr<InputDeviceType> Create(const Common::ParamPackage&) = 0;
};

namespace Impl {

template <typename InputDeviceType>
using FactoryListType = std::unordered_map<std::string, std::shared_ptr<Factory<InputDeviceType>>>;

template <typename InputDeviceType>
struct FactoryList {
    static FactoryListType<InputDeviceType> list;
};

template <typename InputDeviceType>
FactoryListType<InputDeviceType> FactoryList<InputDeviceType>::list;

} // namespace Impl

/**
 * Registers an input device factory.
 * @tparam InputDeviceType the type of input devices the factory can create
 * @param name the name of the factory. Will be used to match the "engine" parameter when creating
 *     a device
 * @param factory the factory object to register
 */
template <typename InputDeviceType>
void RegisterFactory(const std::string& name, std::shared_ptr<Factory<InputDeviceType>> factory) {
    auto pair = std::make_pair(name, std::move(factory));
    if (!Impl::FactoryList<InputDeviceType>::list.insert(std::move(pair)).second) {
        LOG_ERROR(Input, "Factory %s already registered", name.c_str());
    }
}

/**
 * Unregisters an input device factory.
 * @tparam InputDeviceType the type of input devices the factory can create
 * @param name the name of the factory to unregister
 */
template <typename InputDeviceType>
void UnregisterFactory(const std::string& name) {
    if (Impl::FactoryList<InputDeviceType>::list.erase(name) == 0) {
        LOG_ERROR(Input, "Factory %s not registered", name.c_str());
    }
}

/**
 * Create an input device from given paramters.
 * @tparam InputDeviceType the type of input devices to create
 * @param params a serialized ParamPackage string contains all parameters for creating the device
 */
template <typename InputDeviceType>
std::unique_ptr<InputDeviceType> CreateDevice(const std::string& params) {
    const Common::ParamPackage package(params);
    const std::string engine = package.Get("engine", "null");
    const auto& factory_list = Impl::FactoryList<InputDeviceType>::list;
    const auto pair = factory_list.find(engine);
    if (pair == factory_list.end()) {
        if (engine != "null") {
            LOG_ERROR(Input, "Unknown engine name: %s", engine.c_str());
        }
        return std::make_unique<InputDeviceType>();
    }
    return pair->second->Create(package);
}

/**
 * A button device is an input device that returns bool as status.
 * true for pressed; false for released.
 */
using ButtonDevice = InputDevice<bool>;

} // namespace Input

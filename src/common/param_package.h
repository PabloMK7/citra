// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <initializer_list>
#include <string>
#include <unordered_map>

namespace Common {

/// A string-based key-value container supporting serializing to and deserializing from a string
class ParamPackage {
public:
    using DataType = std::unordered_map<std::string, std::string>;

    ParamPackage() = default;
    explicit ParamPackage(const std::string& serialized);
    ParamPackage(std::initializer_list<DataType::value_type> list);
    ParamPackage(const ParamPackage& other) = default;
    ParamPackage(ParamPackage&& other) = default;

    ParamPackage& operator=(const ParamPackage& other) = default;
    ParamPackage& operator=(ParamPackage&& other) = default;

    std::string Serialize() const;
    std::string Get(const std::string& key, const std::string& default_value) const;
    int Get(const std::string& key, int default_value) const;
    float Get(const std::string& key, float default_value) const;
    void Set(const std::string& key, std::string value);
    void Set(const std::string& key, int value);
    void Set(const std::string& key, float value);
    bool Has(const std::string& key) const;
    void Erase(const std::string& key);
    void Clear();

private:
    DataType data;
};

} // namespace Common

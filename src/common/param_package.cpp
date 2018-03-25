// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <vector>
#include "common/logging/log.h"
#include "common/param_package.h"
#include "common/string_util.h"

namespace Common {

constexpr char KEY_VALUE_SEPARATOR = ':';
constexpr char PARAM_SEPARATOR = ',';
constexpr char ESCAPE_CHARACTER = '$';
const std::string KEY_VALUE_SEPARATOR_ESCAPE{ESCAPE_CHARACTER, '0'};
const std::string PARAM_SEPARATOR_ESCAPE{ESCAPE_CHARACTER, '1'};
const std::string ESCAPE_CHARACTER_ESCAPE{ESCAPE_CHARACTER, '2'};

ParamPackage::ParamPackage(const std::string& serialized) {
    std::vector<std::string> pairs;
    Common::SplitString(serialized, PARAM_SEPARATOR, pairs);

    for (const std::string& pair : pairs) {
        std::vector<std::string> key_value;
        Common::SplitString(pair, KEY_VALUE_SEPARATOR, key_value);
        if (key_value.size() != 2) {
            NGLOG_ERROR(Common, "invalid key pair {}", pair);
            continue;
        }

        for (std::string& part : key_value) {
            part = Common::ReplaceAll(part, KEY_VALUE_SEPARATOR_ESCAPE, {KEY_VALUE_SEPARATOR});
            part = Common::ReplaceAll(part, PARAM_SEPARATOR_ESCAPE, {PARAM_SEPARATOR});
            part = Common::ReplaceAll(part, ESCAPE_CHARACTER_ESCAPE, {ESCAPE_CHARACTER});
        }

        Set(key_value[0], key_value[1]);
    }
}

ParamPackage::ParamPackage(std::initializer_list<DataType::value_type> list) : data(list) {}

std::string ParamPackage::Serialize() const {
    if (data.empty())
        return "";

    std::string result;

    for (const auto& pair : data) {
        std::array<std::string, 2> key_value{{pair.first, pair.second}};
        for (std::string& part : key_value) {
            part = Common::ReplaceAll(part, {ESCAPE_CHARACTER}, ESCAPE_CHARACTER_ESCAPE);
            part = Common::ReplaceAll(part, {PARAM_SEPARATOR}, PARAM_SEPARATOR_ESCAPE);
            part = Common::ReplaceAll(part, {KEY_VALUE_SEPARATOR}, KEY_VALUE_SEPARATOR_ESCAPE);
        }
        result += key_value[0] + KEY_VALUE_SEPARATOR + key_value[1] + PARAM_SEPARATOR;
    }

    result.pop_back(); // discard the trailing PARAM_SEPARATOR
    return result;
}

std::string ParamPackage::Get(const std::string& key, const std::string& default_value) const {
    auto pair = data.find(key);
    if (pair == data.end()) {
        NGLOG_DEBUG(Common, "key {} not found", key);
        return default_value;
    }

    return pair->second;
}

int ParamPackage::Get(const std::string& key, int default_value) const {
    auto pair = data.find(key);
    if (pair == data.end()) {
        NGLOG_DEBUG(Common, "key {} not found", key);
        return default_value;
    }

    try {
        return std::stoi(pair->second);
    } catch (const std::logic_error&) {
        NGLOG_ERROR(Common, "failed to convert {} to int", pair->second);
        return default_value;
    }
}

float ParamPackage::Get(const std::string& key, float default_value) const {
    auto pair = data.find(key);
    if (pair == data.end()) {
        NGLOG_DEBUG(Common, "key {} not found", key);
        return default_value;
    }

    try {
        return std::stof(pair->second);
    } catch (const std::logic_error&) {
        NGLOG_ERROR(Common, "failed to convert {} to float", pair->second);
        return default_value;
    }
}

void ParamPackage::Set(const std::string& key, const std::string& value) {
    data[key] = value;
}

void ParamPackage::Set(const std::string& key, int value) {
    data[key] = std::to_string(value);
}

void ParamPackage::Set(const std::string& key, float value) {
    data[key] = std::to_string(value);
}

bool ParamPackage::Has(const std::string& key) const {
    return data.find(key) != data.end();
}

} // namespace Common

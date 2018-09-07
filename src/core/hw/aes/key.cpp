// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <exception>
#include <sstream>
#include <boost/optional.hpp>
#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/hw/aes/arithmetic128.h"
#include "core/hw/aes/key.h"

namespace HW {
namespace AES {

namespace {

boost::optional<AESKey> generator_constant;

struct KeySlot {
    boost::optional<AESKey> x;
    boost::optional<AESKey> y;
    boost::optional<AESKey> normal;

    void SetKeyX(const AESKey& key) {
        x = key;
        if (y && generator_constant) {
            GenerateNormalKey();
        }
    }

    void SetKeyY(const AESKey& key) {
        y = key;
        if (x && generator_constant) {
            GenerateNormalKey();
        }
    }

    void SetNormalKey(const AESKey& key) {
        normal = key;
    }

    void GenerateNormalKey() {
        normal = Lrot128(Add128(Xor128(Lrot128(*x, 2), *y), *generator_constant), 87);
    }

    void Clear() {
        x.reset();
        y.reset();
        normal.reset();
    }
};

std::array<KeySlot, KeySlotID::MaxKeySlotID> key_slots;

AESKey HexToKey(const std::string& hex) {
    if (hex.size() < 32) {
        throw std::invalid_argument("hex string is too short");
    }

    AESKey key;
    for (std::size_t i = 0; i < key.size(); ++i) {
        key[i] = static_cast<u8>(std::stoi(hex.substr(i * 2, 2), 0, 16));
    }

    return key;
}

void LoadPresetKeys() {
    const std::string filepath = FileUtil::GetUserPath(D_SYSDATA_IDX) + AES_KEYS;
    FileUtil::CreateFullPath(filepath); // Create path if not already created
    std::ifstream file;
    OpenFStream(file, filepath, std::ios_base::in);
    if (!file) {
        return;
    }

    while (!file.eof()) {
        std::string line;
        std::getline(file, line);
        std::vector<std::string> parts;
        Common::SplitString(line, '=', parts);
        if (parts.size() != 2) {
            LOG_ERROR(HW_AES, "Failed to parse {}", line);
            continue;
        }

        const std::string& name = parts[0];
        AESKey key;
        try {
            key = HexToKey(parts[1]);
        } catch (const std::logic_error& e) {
            LOG_ERROR(HW_AES, "Invalid key {}: {}", parts[1], e.what());
            continue;
        }

        if (name == "generator") {
            generator_constant = key;
            continue;
        }

        std::size_t slot_id;
        char key_type;
        if (std::sscanf(name.c_str(), "slot0x%zXKey%c", &slot_id, &key_type) != 2) {
            LOG_ERROR(HW_AES, "Invalid key name {}", name);
            continue;
        }

        if (slot_id >= MaxKeySlotID) {
            LOG_ERROR(HW_AES, "Out of range slot ID {:#X}", slot_id);
            continue;
        }

        switch (key_type) {
        case 'X':
            key_slots.at(slot_id).SetKeyX(key);
            break;
        case 'Y':
            key_slots.at(slot_id).SetKeyY(key);
            break;
        case 'N':
            key_slots.at(slot_id).SetNormalKey(key);
            break;
        default:
            LOG_ERROR(HW_AES, "Invalid key type {}", key_type);
            break;
        }
    }
}

} // namespace

void InitKeys() {
    static bool initialized = false;
    if (initialized)
        return;
    LoadPresetKeys();
    initialized = true;
}

void SetGeneratorConstant(const AESKey& key) {
    generator_constant = key;
}

void SetKeyX(std::size_t slot_id, const AESKey& key) {
    key_slots.at(slot_id).SetKeyX(key);
}

void SetKeyY(std::size_t slot_id, const AESKey& key) {
    key_slots.at(slot_id).SetKeyY(key);
}

void SetNormalKey(std::size_t slot_id, const AESKey& key) {
    key_slots.at(slot_id).SetNormalKey(key);
}

bool IsNormalKeyAvailable(std::size_t slot_id) {
    return key_slots.at(slot_id).normal.is_initialized();
}

AESKey GetNormalKey(std::size_t slot_id) {
    return key_slots.at(slot_id).normal.value_or(AESKey{});
}

} // namespace AES
} // namespace HW

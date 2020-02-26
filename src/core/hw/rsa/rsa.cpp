// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <sstream>
#include <cryptopp/integer.h>
#include <cryptopp/nbtheory.h>
#include <cryptopp/sha.h>
#include <fmt/format.h>
#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/hw/rsa/rsa.h"

namespace HW::RSA {

namespace {
std::vector<u8> HexToBytes(const std::string& hex) {
    std::vector<u8> bytes;

    for (unsigned int i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        u8 byte = (u8)strtol(byteString.c_str(), NULL, 16);
        bytes.push_back(byte);
    }
    return bytes;
};
} // namespace

constexpr std::size_t SlotSize = 4;
std::array<RsaSlot, SlotSize> rsa_slots;

std::vector<u8> RsaSlot::GetSignature(const std::vector<u8>& message) {
    CryptoPP::Integer sig =
        CryptoPP::ModularExponentiation(CryptoPP::Integer(message.data(), message.size()),
                                        CryptoPP::Integer(exponent.data(), exponent.size()),
                                        CryptoPP::Integer(modulus.data(), modulus.size()));
    std::stringstream ss;
    ss << std::hex << sig;
    return HexToBytes(ss.str());
}

void InitSlots() {
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;

    const std::string filepath = FileUtil::GetUserPath(FileUtil::UserPath::SysDataDir) + BOOTROM9;
    auto file = FileUtil::IOFile(filepath, "rb");
    if (!file) {
        return;
    }

    const std::size_t length = file.GetSize();
    if (length != 65536) {
        LOG_ERROR(HW_AES, "Bootrom9 size is wrong: {}", length);
        return;
    }

    constexpr std::size_t RSA_MODULUS_POS = 0xB3E0;
    file.Seek(RSA_MODULUS_POS, SEEK_SET);
    std::vector<u8> modulus;
    modulus.resize(256);
    file.ReadArray(modulus.data(), modulus.size());

    constexpr std::size_t RSA_EXPONENT_POS = 0xB4E0;
    file.Seek(RSA_EXPONENT_POS, SEEK_SET);
    std::vector<u8> exponent;
    exponent.resize(256);
    file.ReadArray(exponent.data(), exponent.size());

    rsa_slots[0] = RsaSlot(exponent, modulus);
    // TODO(B3N30): Initalize the other slots. But since they aren't used at all, we can skip them
    // for now
}

RsaSlot GetSlot(std::size_t slot_id) {
    if (slot_id >= rsa_slots.size())
        return RsaSlot{};
    return rsa_slots[slot_id];
}

std::vector<u8> CreateASN1Message(const std::vector<u8>& data) {
    static constexpr auto asn1_header =
        "0001FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
        "FFFFFFFFFFFFFFFFFFFFFFFF003031300D060960864801650304020105000420";
    std::vector<u8> message = HexToBytes(asn1_header);
    CryptoPP::SHA256 sha;
    std::array<u8, CryptoPP::SHA256::DIGESTSIZE> hash;
    sha.CalculateDigest(hash.data(), data.data(), data.size());
    std::copy(hash.begin(), hash.end(), std::back_inserter(message));
    return message;
}

} // namespace HW::RSA
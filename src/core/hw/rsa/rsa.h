// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "common/common_types.h"

namespace HW::RSA {

class RsaSlot {
public:
    RsaSlot() : init(false) {}
    RsaSlot(const std::vector<u8>& exponent, const std::vector<u8>& modulus)
        : init(true), exponent(exponent), modulus(modulus) {}
    std::vector<u8> GetSignature(const std::vector<u8>& message);

    operator bool() const {
        // TODO(B3N30): Maybe check if exponent and modulus are vailid
        return init;
    }

private:
    bool init;
    std::vector<u8> exponent;
    std::vector<u8> modulus;
};

void InitSlots();

RsaSlot GetSlot(std::size_t slot_id);

std::vector<u8> CreateASN1Message(const std::vector<u8>& data);

} // namespace HW::RSA
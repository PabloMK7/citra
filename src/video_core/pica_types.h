// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Pica {

struct float24 {
    static float24 FromFloat32(float val) {
        float24 ret;
        ret.value = val;
        return ret;
    }

    // 16 bit mantissa, 7 bit exponent, 1 bit sign
    // TODO: No idea if this works as intended
    static float24 FromRawFloat24(u32 hex) {
        float24 ret;
        if ((hex & 0xFFFFFF) == 0) {
            ret.value = 0;
        } else {
            u32 mantissa = hex & 0xFFFF;
            u32 exponent = (hex >> 16) & 0x7F;
            u32 sign = hex >> 23;
            ret.value = std::pow(2.0f, (float)exponent-63.0f) * (1.0f + mantissa * std::pow(2.0f, -16.f));
            if (sign)
                ret.value = -ret.value;
        }
        return ret;
    }

    static float24 Zero() {
        return FromFloat32(0.f);
    }

    // Not recommended for anything but logging
    float ToFloat32() const {
        return value;
    }

    float24 operator * (const float24& flt) const {
        if ((this->value == 0.f && !std::isnan(flt.value)) ||
            (flt.value == 0.f && !std::isnan(this->value)))
            // PICA gives 0 instead of NaN when multiplying by inf
            return Zero();
        return float24::FromFloat32(ToFloat32() * flt.ToFloat32());
    }

    float24 operator / (const float24& flt) const {
        return float24::FromFloat32(ToFloat32() / flt.ToFloat32());
    }

    float24 operator + (const float24& flt) const {
        return float24::FromFloat32(ToFloat32() + flt.ToFloat32());
    }

    float24 operator - (const float24& flt) const {
        return float24::FromFloat32(ToFloat32() - flt.ToFloat32());
    }

    float24& operator *= (const float24& flt) {
        if ((this->value == 0.f && !std::isnan(flt.value)) ||
            (flt.value == 0.f && !std::isnan(this->value)))
            // PICA gives 0 instead of NaN when multiplying by inf
            *this = Zero();
        else value *= flt.ToFloat32();
        return *this;
    }

    float24& operator /= (const float24& flt) {
        value /= flt.ToFloat32();
        return *this;
    }

    float24& operator += (const float24& flt) {
        value += flt.ToFloat32();
        return *this;
    }

    float24& operator -= (const float24& flt) {
        value -= flt.ToFloat32();
        return *this;
    }

    float24 operator - () const {
        return float24::FromFloat32(-ToFloat32());
    }

    bool operator < (const float24& flt) const {
        return ToFloat32() < flt.ToFloat32();
    }

    bool operator > (const float24& flt) const {
        return ToFloat32() > flt.ToFloat32();
    }

    bool operator >= (const float24& flt) const {
        return ToFloat32() >= flt.ToFloat32();
    }

    bool operator <= (const float24& flt) const {
        return ToFloat32() <= flt.ToFloat32();
    }

    bool operator == (const float24& flt) const {
        return ToFloat32() == flt.ToFloat32();
    }

    bool operator != (const float24& flt) const {
        return ToFloat32() != flt.ToFloat32();
    }

private:
    // Stored as a regular float, merely for convenience
    // TODO: Perform proper arithmetic on this!
    float value;
};

static_assert(sizeof(float24) == sizeof(float), "Shader JIT assumes float24 is implemented as a 32-bit float");

struct float16 {
    // 10 bit mantissa, 5 bit exponent, 1 bit sign
    // TODO: No idea if this works as intended
    static float16 FromRawFloat16(u32 hex) {
        float16 ret;
        if ((hex & 0xFFFF) == 0) {
            ret.value = 0;
        } else {
            u32 mantissa = hex & 0x3FF;
            u32 exponent = (hex >> 10) & 0x1F;
            u32 sign = (hex >> 15) & 1;
            ret.value = std::pow(2.0f, (float)exponent - 15.0f) * (1.0f + mantissa * std::pow(2.0f, -10.f));
            if (sign)
                ret.value = -ret.value;
        }
        return ret;
    }

    float ToFloat32() const {
        return value;
    }

private:
    // Stored as a regular float, merely for convenience
    // TODO: Perform proper arithmetic on this!
    float value;
};

struct float20 {
    // 12 bit mantissa, 7 bit exponent, 1 bit sign
    // TODO: No idea if this works as intended
    static float20 FromRawFloat20(u32 hex) {
        float20 ret;
        if ((hex & 0xFFFFF) == 0) {
            ret.value = 0;
        } else {
            u32 mantissa = hex & 0xFFF;
            u32 exponent = (hex >> 12) & 0x7F;
            u32 sign = (hex >> 19) & 1;
            ret.value = std::pow(2.0f, (float)exponent - 63.0f) * (1.0f + mantissa * std::pow(2.0f, -12.f));
            if (sign)
                ret.value = -ret.value;
        }
        return ret;
    }

    float ToFloat32() const {
        return value;
    }

private:
    // Stored as a regular float, merely for convenience
    // TODO: Perform proper arithmetic on this!
    float value;
};

} // namespace Pica

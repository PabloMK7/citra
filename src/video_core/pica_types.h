// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cmath>
#include <cstring>
#include <boost/serialization/access.hpp>
#include "common/common_types.h"

namespace Pica {

/**
 * Template class for converting arbitrary Pica float types to IEEE 754 32-bit single-precision
 * floating point.
 *
 * When decoding, format is as follows:
 *  - The first `M` bits are the mantissa
 *  - The next `E` bits are the exponent
 *  - The last bit is the sign bit
 *
 * @todo Verify on HW if this conversion is sufficiently accurate.
 */
template <unsigned M, unsigned E>
struct Float {
public:
    static Float<M, E> FromFloat32(float val) {
        Float<M, E> ret;
        ret.value = val;
        return ret;
    }

    static Float<M, E> FromRaw(u32 hex) {
        Float<M, E> res;

        const int width = M + E + 1;
        const int bias = 128 - (1 << (E - 1));
        int exponent = (hex >> M) & ((1 << E) - 1);
        const unsigned mantissa = hex & ((1 << M) - 1);
        const unsigned sign = (hex >> (E + M)) << 31;

        if (hex & ((1 << (width - 1)) - 1)) {
            if (exponent == (1 << E) - 1)
                exponent = 255;
            else
                exponent += bias;
            hex = sign | (mantissa << (23 - M)) | (exponent << 23);
        } else {
            hex = sign;
        }

        std::memcpy(&res.value, &hex, sizeof(float));

        return res;
    }

    static Float<M, E> Zero() {
        return FromFloat32(0.f);
    }

    // Not recommended for anything but logging
    float ToFloat32() const {
        return value;
    }

    Float<M, E> operator*(const Float<M, E>& flt) const {
        float result = value * flt.ToFloat32();
        // PICA gives 0 instead of NaN when multiplying by inf
        if (std::isnan(result))
            if (!std::isnan(value) && !std::isnan(flt.ToFloat32()))
                result = 0.f;
        return Float<M, E>::FromFloat32(result);
    }

    Float<M, E> operator/(const Float<M, E>& flt) const {
        return Float<M, E>::FromFloat32(ToFloat32() / flt.ToFloat32());
    }

    Float<M, E> operator+(const Float<M, E>& flt) const {
        return Float<M, E>::FromFloat32(ToFloat32() + flt.ToFloat32());
    }

    Float<M, E> operator-(const Float<M, E>& flt) const {
        return Float<M, E>::FromFloat32(ToFloat32() - flt.ToFloat32());
    }

    Float<M, E>& operator*=(const Float<M, E>& flt) {
        value = operator*(flt).value;
        return *this;
    }

    Float<M, E>& operator/=(const Float<M, E>& flt) {
        value /= flt.ToFloat32();
        return *this;
    }

    Float<M, E>& operator+=(const Float<M, E>& flt) {
        value += flt.ToFloat32();
        return *this;
    }

    Float<M, E>& operator-=(const Float<M, E>& flt) {
        value -= flt.ToFloat32();
        return *this;
    }

    Float<M, E> operator-() const {
        return Float<M, E>::FromFloat32(-ToFloat32());
    }

    bool operator<(const Float<M, E>& flt) const {
        return ToFloat32() < flt.ToFloat32();
    }

    bool operator>(const Float<M, E>& flt) const {
        return ToFloat32() > flt.ToFloat32();
    }

    bool operator>=(const Float<M, E>& flt) const {
        return ToFloat32() >= flt.ToFloat32();
    }

    bool operator<=(const Float<M, E>& flt) const {
        return ToFloat32() <= flt.ToFloat32();
    }

    bool operator==(const Float<M, E>& flt) const {
        return ToFloat32() == flt.ToFloat32();
    }

    bool operator!=(const Float<M, E>& flt) const {
        return ToFloat32() != flt.ToFloat32();
    }

private:
    static const unsigned MASK = (1 << (M + E + 1)) - 1;
    static const unsigned MANTISSA_MASK = (1 << M) - 1;
    static const unsigned EXPONENT_MASK = (1 << E) - 1;

    // Stored as a regular float, merely for convenience
    // TODO: Perform proper arithmetic on this!
    float value;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int file_version) {
        ar& value;
    }
};

using float24 = Float<16, 7>;
using float20 = Float<12, 7>;
using float16 = Float<10, 5>;

} // namespace Pica

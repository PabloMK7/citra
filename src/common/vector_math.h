// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Copyright 2014 Tony Wasserka
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the owner nor the names of its contributors may
//       be used to endorse or promote products derived from this software
//       without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <cmath>
#include <type_traits>

namespace Math {

template <typename T>
class Vec2;
template <typename T>
class Vec3;
template <typename T>
class Vec4;

template <typename T>
static inline Vec2<T> MakeVec(const T& x, const T& y);
template <typename T>
static inline Vec3<T> MakeVec(const T& x, const T& y, const T& z);
template <typename T>
static inline Vec4<T> MakeVec(const T& x, const T& y, const T& z, const T& w);

template <typename T>
class Vec2 {
public:
    T x;
    T y;

    T* AsArray() {
        return &x;
    }

    Vec2() = default;
    Vec2(const T a[2]) : x(a[0]), y(a[1]) {}
    Vec2(const T& _x, const T& _y) : x(_x), y(_y) {}

    template <typename T2>
    Vec2<T2> Cast() const {
        return Vec2<T2>((T2)x, (T2)y);
    }

    static Vec2 AssignToAll(const T& f) {
        return Vec2<T>(f, f);
    }

    void Write(T a[2]) {
        a[0] = x;
        a[1] = y;
    }

    Vec2<decltype(T{} + T{})> operator+(const Vec2& other) const {
        return MakeVec(x + other.x, y + other.y);
    }
    void operator+=(const Vec2& other) {
        x += other.x;
        y += other.y;
    }
    Vec2<decltype(T{} - T{})> operator-(const Vec2& other) const {
        return MakeVec(x - other.x, y - other.y);
    }
    void operator-=(const Vec2& other) {
        x -= other.x;
        y -= other.y;
    }
    template <typename Q = T, class = typename std::enable_if<std::is_signed<Q>::value>::type>
    Vec2<decltype(-T{})> operator-() const {
        return MakeVec(-x, -y);
    }
    Vec2<decltype(T{} * T{})> operator*(const Vec2& other) const {
        return MakeVec(x * other.x, y * other.y);
    }
    template <typename V>
    Vec2<decltype(T{} * V{})> operator*(const V& f) const {
        return MakeVec(x * f, y * f);
    }
    template <typename V>
    void operator*=(const V& f) {
        x *= f;
        y *= f;
    }
    template <typename V>
    Vec2<decltype(T{} / V{})> operator/(const V& f) const {
        return MakeVec(x / f, y / f);
    }
    template <typename V>
    void operator/=(const V& f) {
        *this = *this / f;
    }

    T Length2() const {
        return x * x + y * y;
    }

    // Only implemented for T=float
    float Length() const;
    void SetLength(const float l);
    Vec2 WithLength(const float l) const;
    float Distance2To(Vec2& other);
    Vec2 Normalized() const;
    float Normalize(); // returns the previous length, which is often useful

    T& operator[](int i) // allow vector[1] = 3   (vector.y=3)
    {
        return *((&x) + i);
    }
    T operator[](const int i) const {
        return *((&x) + i);
    }

    void SetZero() {
        x = 0;
        y = 0;
    }

    // Common aliases: UV (texel coordinates), ST (texture coordinates)
    T& u() {
        return x;
    }
    T& v() {
        return y;
    }
    T& s() {
        return x;
    }
    T& t() {
        return y;
    }

    const T& u() const {
        return x;
    }
    const T& v() const {
        return y;
    }
    const T& s() const {
        return x;
    }
    const T& t() const {
        return y;
    }

    // swizzlers - create a subvector of specific components
    const Vec2 yx() const {
        return Vec2(y, x);
    }
    const Vec2 vu() const {
        return Vec2(y, x);
    }
    const Vec2 ts() const {
        return Vec2(y, x);
    }
};

template <typename T, typename V>
Vec2<T> operator*(const V& f, const Vec2<T>& vec) {
    return Vec2<T>(f * vec.x, f * vec.y);
}

typedef Vec2<float> Vec2f;

template <typename T>
class Vec3 {
public:
    T x;
    T y;
    T z;

    T* AsArray() {
        return &x;
    }

    Vec3() = default;
    Vec3(const T a[3]) : x(a[0]), y(a[1]), z(a[2]) {}
    Vec3(const T& _x, const T& _y, const T& _z) : x(_x), y(_y), z(_z) {}

    template <typename T2>
    Vec3<T2> Cast() const {
        return MakeVec<T2>((T2)x, (T2)y, (T2)z);
    }

    // Only implemented for T=int and T=float
    static Vec3 FromRGB(unsigned int rgb);
    unsigned int ToRGB() const; // alpha bits set to zero

    static Vec3 AssignToAll(const T& f) {
        return MakeVec(f, f, f);
    }

    void Write(T a[3]) {
        a[0] = x;
        a[1] = y;
        a[2] = z;
    }

    Vec3<decltype(T{} + T{})> operator+(const Vec3& other) const {
        return MakeVec(x + other.x, y + other.y, z + other.z);
    }
    void operator+=(const Vec3& other) {
        x += other.x;
        y += other.y;
        z += other.z;
    }
    Vec3<decltype(T{} - T{})> operator-(const Vec3& other) const {
        return MakeVec(x - other.x, y - other.y, z - other.z);
    }
    void operator-=(const Vec3& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
    }
    template <typename Q = T, class = typename std::enable_if<std::is_signed<Q>::value>::type>
    Vec3<decltype(-T{})> operator-() const {
        return MakeVec(-x, -y, -z);
    }
    Vec3<decltype(T{} * T{})> operator*(const Vec3& other) const {
        return MakeVec(x * other.x, y * other.y, z * other.z);
    }
    template <typename V>
    Vec3<decltype(T{} * V{})> operator*(const V& f) const {
        return MakeVec(x * f, y * f, z * f);
    }
    template <typename V>
    void operator*=(const V& f) {
        x *= f;
        y *= f;
        z *= f;
    }
    template <typename V>
    Vec3<decltype(T{} / V{})> operator/(const V& f) const {
        return MakeVec(x / f, y / f, z / f);
    }
    template <typename V>
    void operator/=(const V& f) {
        *this = *this / f;
    }

    T Length2() const {
        return x * x + y * y + z * z;
    }

    // Only implemented for T=float
    float Length() const;
    void SetLength(const float l);
    Vec3 WithLength(const float l) const;
    float Distance2To(Vec3& other);
    Vec3 Normalized() const;
    float Normalize(); // returns the previous length, which is often useful

    T& operator[](int i) // allow vector[2] = 3   (vector.z=3)
    {
        return *((&x) + i);
    }
    T operator[](const int i) const {
        return *((&x) + i);
    }

    void SetZero() {
        x = 0;
        y = 0;
        z = 0;
    }

    // Common aliases: UVW (texel coordinates), RGB (colors), STQ (texture coordinates)
    T& u() {
        return x;
    }
    T& v() {
        return y;
    }
    T& w() {
        return z;
    }

    T& r() {
        return x;
    }
    T& g() {
        return y;
    }
    T& b() {
        return z;
    }

    T& s() {
        return x;
    }
    T& t() {
        return y;
    }
    T& q() {
        return z;
    }

    const T& u() const {
        return x;
    }
    const T& v() const {
        return y;
    }
    const T& w() const {
        return z;
    }

    const T& r() const {
        return x;
    }
    const T& g() const {
        return y;
    }
    const T& b() const {
        return z;
    }

    const T& s() const {
        return x;
    }
    const T& t() const {
        return y;
    }
    const T& q() const {
        return z;
    }

// swizzlers - create a subvector of specific components
// e.g. Vec2 uv() { return Vec2(x,y); }
// _DEFINE_SWIZZLER2 defines a single such function, DEFINE_SWIZZLER2 defines all of them for all
// component names (x<->r) and permutations (xy<->yx)
#define _DEFINE_SWIZZLER2(a, b, name)                                                              \
    const Vec2<T> name() const {                                                                   \
        return Vec2<T>(a, b);                                                                      \
    }
#define DEFINE_SWIZZLER2(a, b, a2, b2, a3, b3, a4, b4)                                             \
    _DEFINE_SWIZZLER2(a, b, a##b);                                                                 \
    _DEFINE_SWIZZLER2(a, b, a2##b2);                                                               \
    _DEFINE_SWIZZLER2(a, b, a3##b3);                                                               \
    _DEFINE_SWIZZLER2(a, b, a4##b4);                                                               \
    _DEFINE_SWIZZLER2(b, a, b##a);                                                                 \
    _DEFINE_SWIZZLER2(b, a, b2##a2);                                                               \
    _DEFINE_SWIZZLER2(b, a, b3##a3);                                                               \
    _DEFINE_SWIZZLER2(b, a, b4##a4)

    DEFINE_SWIZZLER2(x, y, r, g, u, v, s, t);
    DEFINE_SWIZZLER2(x, z, r, b, u, w, s, q);
    DEFINE_SWIZZLER2(y, z, g, b, v, w, t, q);
#undef DEFINE_SWIZZLER2
#undef _DEFINE_SWIZZLER2
};

template <typename T, typename V>
Vec3<T> operator*(const V& f, const Vec3<T>& vec) {
    return Vec3<T>(f * vec.x, f * vec.y, f * vec.z);
}

template <>
inline float Vec3<float>::Length() const {
    return std::sqrt(x * x + y * y + z * z);
}

template <>
inline Vec3<float> Vec3<float>::Normalized() const {
    return *this / Length();
}

typedef Vec3<float> Vec3f;

template <typename T>
class Vec4 {
public:
    T x;
    T y;
    T z;
    T w;

    T* AsArray() {
        return &x;
    }

    Vec4() = default;
    Vec4(const T a[4]) : x(a[0]), y(a[1]), z(a[2]), w(a[3]) {}
    Vec4(const T& _x, const T& _y, const T& _z, const T& _w) : x(_x), y(_y), z(_z), w(_w) {}

    template <typename T2>
    Vec4<T2> Cast() const {
        return Vec4<T2>((T2)x, (T2)y, (T2)z, (T2)w);
    }

    // Only implemented for T=int and T=float
    static Vec4 FromRGBA(unsigned int rgba);
    unsigned int ToRGBA() const;

    static Vec4 AssignToAll(const T& f) {
        return Vec4<T>(f, f, f, f);
    }

    void Write(T a[4]) {
        a[0] = x;
        a[1] = y;
        a[2] = z;
        a[3] = w;
    }

    Vec4<decltype(T{} + T{})> operator+(const Vec4& other) const {
        return MakeVec(x + other.x, y + other.y, z + other.z, w + other.w);
    }
    void operator+=(const Vec4& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        w += other.w;
    }
    Vec4<decltype(T{} - T{})> operator-(const Vec4& other) const {
        return MakeVec(x - other.x, y - other.y, z - other.z, w - other.w);
    }
    void operator-=(const Vec4& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        w -= other.w;
    }
    template <typename Q = T, class = typename std::enable_if<std::is_signed<Q>::value>::type>
    Vec4<decltype(-T{})> operator-() const {
        return MakeVec(-x, -y, -z, -w);
    }
    Vec4<decltype(T{} * T{})> operator*(const Vec4& other) const {
        return MakeVec(x * other.x, y * other.y, z * other.z, w * other.w);
    }
    template <typename V>
    Vec4<decltype(T{} * V{})> operator*(const V& f) const {
        return MakeVec(x * f, y * f, z * f, w * f);
    }
    template <typename V>
    void operator*=(const V& f) {
        x *= f;
        y *= f;
        z *= f;
        w *= f;
    }
    template <typename V>
    Vec4<decltype(T{} / V{})> operator/(const V& f) const {
        return MakeVec(x / f, y / f, z / f, w / f);
    }
    template <typename V>
    void operator/=(const V& f) {
        *this = *this / f;
    }

    T Length2() const {
        return x * x + y * y + z * z + w * w;
    }

    // Only implemented for T=float
    float Length() const;
    void SetLength(const float l);
    Vec4 WithLength(const float l) const;
    float Distance2To(Vec4& other);
    Vec4 Normalized() const;
    float Normalize(); // returns the previous length, which is often useful

    T& operator[](int i) // allow vector[2] = 3   (vector.z=3)
    {
        return *((&x) + i);
    }
    T operator[](const int i) const {
        return *((&x) + i);
    }

    void SetZero() {
        x = 0;
        y = 0;
        z = 0;
        w = 0;
    }

    // Common alias: RGBA (colors)
    T& r() {
        return x;
    }
    T& g() {
        return y;
    }
    T& b() {
        return z;
    }
    T& a() {
        return w;
    }

    const T& r() const {
        return x;
    }
    const T& g() const {
        return y;
    }
    const T& b() const {
        return z;
    }
    const T& a() const {
        return w;
    }

// Swizzlers - Create a subvector of specific components
// e.g. Vec2 uv() { return Vec2(x,y); }

// _DEFINE_SWIZZLER2 defines a single such function
// DEFINE_SWIZZLER2_COMP1 defines one-component functions for all component names (x<->r)
// DEFINE_SWIZZLER2_COMP2 defines two component functions for all component names (x<->r) and
// permutations (xy<->yx)
#define _DEFINE_SWIZZLER2(a, b, name)                                                              \
    const Vec2<T> name() const {                                                                   \
        return Vec2<T>(a, b);                                                                      \
    }
#define DEFINE_SWIZZLER2_COMP1(a, a2)                                                              \
    _DEFINE_SWIZZLER2(a, a, a##a);                                                                 \
    _DEFINE_SWIZZLER2(a, a, a2##a2)
#define DEFINE_SWIZZLER2_COMP2(a, b, a2, b2)                                                       \
    _DEFINE_SWIZZLER2(a, b, a##b);                                                                 \
    _DEFINE_SWIZZLER2(a, b, a2##b2);                                                               \
    _DEFINE_SWIZZLER2(b, a, b##a);                                                                 \
    _DEFINE_SWIZZLER2(b, a, b2##a2)

    DEFINE_SWIZZLER2_COMP2(x, y, r, g);
    DEFINE_SWIZZLER2_COMP2(x, z, r, b);
    DEFINE_SWIZZLER2_COMP2(x, w, r, a);
    DEFINE_SWIZZLER2_COMP2(y, z, g, b);
    DEFINE_SWIZZLER2_COMP2(y, w, g, a);
    DEFINE_SWIZZLER2_COMP2(z, w, b, a);
    DEFINE_SWIZZLER2_COMP1(x, r);
    DEFINE_SWIZZLER2_COMP1(y, g);
    DEFINE_SWIZZLER2_COMP1(z, b);
    DEFINE_SWIZZLER2_COMP1(w, a);
#undef DEFINE_SWIZZLER2_COMP1
#undef DEFINE_SWIZZLER2_COMP2
#undef _DEFINE_SWIZZLER2

#define _DEFINE_SWIZZLER3(a, b, c, name)                                                           \
    const Vec3<T> name() const {                                                                   \
        return Vec3<T>(a, b, c);                                                                   \
    }
#define DEFINE_SWIZZLER3_COMP1(a, a2)                                                              \
    _DEFINE_SWIZZLER3(a, a, a, a##a##a);                                                           \
    _DEFINE_SWIZZLER3(a, a, a, a2##a2##a2)
#define DEFINE_SWIZZLER3_COMP3(a, b, c, a2, b2, c2)                                                \
    _DEFINE_SWIZZLER3(a, b, c, a##b##c);                                                           \
    _DEFINE_SWIZZLER3(a, c, b, a##c##b);                                                           \
    _DEFINE_SWIZZLER3(b, a, c, b##a##c);                                                           \
    _DEFINE_SWIZZLER3(b, c, a, b##c##a);                                                           \
    _DEFINE_SWIZZLER3(c, a, b, c##a##b);                                                           \
    _DEFINE_SWIZZLER3(c, b, a, c##b##a);                                                           \
    _DEFINE_SWIZZLER3(a, b, c, a2##b2##c2);                                                        \
    _DEFINE_SWIZZLER3(a, c, b, a2##c2##b2);                                                        \
    _DEFINE_SWIZZLER3(b, a, c, b2##a2##c2);                                                        \
    _DEFINE_SWIZZLER3(b, c, a, b2##c2##a2);                                                        \
    _DEFINE_SWIZZLER3(c, a, b, c2##a2##b2);                                                        \
    _DEFINE_SWIZZLER3(c, b, a, c2##b2##a2)

    DEFINE_SWIZZLER3_COMP3(x, y, z, r, g, b);
    DEFINE_SWIZZLER3_COMP3(x, y, w, r, g, a);
    DEFINE_SWIZZLER3_COMP3(x, z, w, r, b, a);
    DEFINE_SWIZZLER3_COMP3(y, z, w, g, b, a);
    DEFINE_SWIZZLER3_COMP1(x, r);
    DEFINE_SWIZZLER3_COMP1(y, g);
    DEFINE_SWIZZLER3_COMP1(z, b);
    DEFINE_SWIZZLER3_COMP1(w, a);
#undef DEFINE_SWIZZLER3_COMP1
#undef DEFINE_SWIZZLER3_COMP3
#undef _DEFINE_SWIZZLER3
};

template <typename T, typename V>
Vec4<decltype(V{} * T{})> operator*(const V& f, const Vec4<T>& vec) {
    return MakeVec(f * vec.x, f * vec.y, f * vec.z, f * vec.w);
}

typedef Vec4<float> Vec4f;

template <typename T>
static inline decltype(T{} * T{} + T{} * T{}) Dot(const Vec2<T>& a, const Vec2<T>& b) {
    return a.x * b.x + a.y * b.y;
}

template <typename T>
static inline decltype(T{} * T{} + T{} * T{}) Dot(const Vec3<T>& a, const Vec3<T>& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

template <typename T>
static inline decltype(T{} * T{} + T{} * T{}) Dot(const Vec4<T>& a, const Vec4<T>& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

template <typename T>
static inline Vec3<decltype(T{} * T{} - T{} * T{})> Cross(const Vec3<T>& a, const Vec3<T>& b) {
    return MakeVec(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

// linear interpolation via float: 0.0=begin, 1.0=end
template <typename X>
static inline decltype(X{} * float{} + X{} * float{}) Lerp(const X& begin, const X& end,
                                                           const float t) {
    return begin * (1.f - t) + end * t;
}

// linear interpolation via int: 0=begin, base=end
template <typename X, int base>
static inline decltype((X{} * int{} + X{} * int{}) / base) LerpInt(const X& begin, const X& end,
                                                                   const int t) {
    return (begin * (base - t) + end * t) / base;
}

// Utility vector factories
template <typename T>
static inline Vec2<T> MakeVec(const T& x, const T& y) {
    return Vec2<T>{x, y};
}

template <typename T>
static inline Vec3<T> MakeVec(const T& x, const T& y, const T& z) {
    return Vec3<T>{x, y, z};
}

template <typename T>
static inline Vec4<T> MakeVec(const T& x, const T& y, const Vec2<T>& zw) {
    return MakeVec(x, y, zw[0], zw[1]);
}

template <typename T>
static inline Vec3<T> MakeVec(const Vec2<T>& xy, const T& z) {
    return MakeVec(xy[0], xy[1], z);
}

template <typename T>
static inline Vec3<T> MakeVec(const T& x, const Vec2<T>& yz) {
    return MakeVec(x, yz[0], yz[1]);
}

template <typename T>
static inline Vec4<T> MakeVec(const T& x, const T& y, const T& z, const T& w) {
    return Vec4<T>{x, y, z, w};
}

template <typename T>
static inline Vec4<T> MakeVec(const Vec2<T>& xy, const T& z, const T& w) {
    return MakeVec(xy[0], xy[1], z, w);
}

template <typename T>
static inline Vec4<T> MakeVec(const T& x, const Vec2<T>& yz, const T& w) {
    return MakeVec(x, yz[0], yz[1], w);
}

// NOTE: This has priority over "Vec2<Vec2<T>> MakeVec(const Vec2<T>& x, const Vec2<T>& y)".
//       Even if someone wanted to use an odd object like Vec2<Vec2<T>>, the compiler would error
//       out soon enough due to misuse of the returned structure.
template <typename T>
static inline Vec4<T> MakeVec(const Vec2<T>& xy, const Vec2<T>& zw) {
    return MakeVec(xy[0], xy[1], zw[0], zw[1]);
}

template <typename T>
static inline Vec4<T> MakeVec(const Vec3<T>& xyz, const T& w) {
    return MakeVec(xyz[0], xyz[1], xyz[2], w);
}

template <typename T>
static inline Vec4<T> MakeVec(const T& x, const Vec3<T>& yzw) {
    return MakeVec(x, yzw[0], yzw[1], yzw[2]);
}

} // namespace

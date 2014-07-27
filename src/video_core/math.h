// Licensed under GPLv2
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

namespace Math {

template<typename T> class Vec2;
template<typename T> class Vec3;
template<typename T> class Vec4;


template<typename T>
class Vec2 {
public:
    struct {
        T x,y;
    };

    T* AsArray() { return &x; }

    Vec2() = default;
    Vec2(const T a[2]) : x(a[0]), y(a[1]) {}
    Vec2(const T& _x, const T& _y) : x(_x), y(_y) {}

    template<typename T2>
    Vec2<T2> Cast() const {
        return Vec2<T2>((T2)x, (T2)y);
    }

    static Vec2 AssignToAll(const T& f)
    {
        return Vec2<T>(f, f);
    }

    void Write(T a[2])
    {
        a[0] = x; a[1] = y;
    }

    Vec2 operator +(const Vec2& other) const
    {
        return Vec2(x+other.x, y+other.y);
    }
    void operator += (const Vec2 &other)
    {
        x+=other.x; y+=other.y;
    }
    Vec2 operator -(const Vec2& other) const
    {
        return Vec2(x-other.x, y-other.y);
    }
    void operator -= (const Vec2& other)
    {
        x-=other.x; y-=other.y;
    }
    Vec2 operator -() const
    {
        return Vec2(-x,-y);
    }
    Vec2 operator * (const Vec2& other) const
    {
        return Vec2(x*other.x, y*other.y);
    }
    template<typename V>
    Vec2 operator * (const V& f) const
    {
        return Vec2(x*f,y*f);
    }
    template<typename V>
    void operator *= (const V& f)
    {
        x*=f; y*=f;
    }
    template<typename V>
    Vec2 operator / (const V& f) const
    {
        return Vec2(x/f,y/f);
    }
    template<typename V>
    void operator /= (const V& f)
    {
        *this = *this / f;
    }

    T Length2() const
    {
        return x*x + y*y;
    }

    // Only implemented for T=float
    float Length() const;
    void SetLength(const float l);
    Vec2 WithLength(const float l) const;
    float Distance2To(Vec2 &other);
    Vec2 Normalized() const;
    float Normalize(); // returns the previous length, which is often useful

    T& operator [] (int i) //allow vector[1] = 3   (vector.y=3)
    {
        return *((&x) + i);
    }
    T operator [] (const int i) const
    {
        return *((&x) + i);
    }

    void SetZero()
    {
        x=0; y=0;
    }

    // Common aliases: UV (texel coordinates), ST (texture coordinates)
    T& u() { return x; }
    T& v() { return y; }
    T& s() { return x; }
    T& t() { return y; }

    const T& u() const { return x; }
    const T& v() const { return y; }
    const T& s() const { return x; }
    const T& t() const { return y; }

    // swizzlers - create a subvector of specific components
    Vec2 yx() const { return Vec2(y, x); }
    Vec2 vu() const { return Vec2(y, x); }
    Vec2 ts() const { return Vec2(y, x); }

    // Inserters to add new elements to effectively create larger vectors containing this Vec2
    Vec3<T> InsertBeforeX(const T& value) {
        return Vec3<T>(value, x, y);
    }
    Vec3<T> InsertBeforeY(const T& value) {
        return Vec3<T>(x, value, y);
    }
    Vec3<T> Append(const T& value) {
        return Vec3<T>(x, y, value);
    }
};

template<typename T, typename V>
Vec2<T> operator * (const V& f, const Vec2<T>& vec)
{
    return Vec2<T>(f*vec.x,f*vec.y);
}

typedef Vec2<float> Vec2f;

template<typename T>
class Vec3
{
public:
    struct
    {
        T x,y,z;
    };

    T* AsArray() { return &x; }

    Vec3() = default;
    Vec3(const T a[3]) : x(a[0]), y(a[1]), z(a[2]) {}
    Vec3(const T& _x, const T& _y, const T& _z) : x(_x), y(_y), z(_z) {}

    template<typename T2>
    Vec3<T2> Cast() const {
        return Vec3<T2>((T2)x, (T2)y, (T2)z);
    }

    // Only implemented for T=int and T=float
    static Vec3 FromRGB(unsigned int rgb);
    unsigned int ToRGB() const; // alpha bits set to zero

    static Vec3 AssignToAll(const T& f)
    {
        return Vec3<T>(f, f, f);
    }

    void Write(T a[3])
    {
        a[0] = x; a[1] = y; a[2] = z;
    }

    Vec3 operator +(const Vec3 &other) const
    {
        return Vec3(x+other.x, y+other.y, z+other.z);
    }
    void operator += (const Vec3 &other)
    {
        x+=other.x; y+=other.y; z+=other.z;
    }
    Vec3 operator -(const Vec3 &other) const
    {
        return Vec3(x-other.x, y-other.y, z-other.z);
    }
    void operator -= (const Vec3 &other)
    {
        x-=other.x; y-=other.y; z-=other.z;
    }
    Vec3 operator -() const
    {
        return Vec3(-x,-y,-z);
    }
    Vec3 operator * (const Vec3 &other) const
    {
        return Vec3(x*other.x, y*other.y, z*other.z);
    }
    template<typename V>
    Vec3 operator * (const V& f) const
    {
        return Vec3(x*f,y*f,z*f);
    }
    template<typename V>
    void operator *= (const V& f)
    {
        x*=f; y*=f; z*=f;
    }
    template<typename V>
    Vec3 operator / (const V& f) const
    {
        return Vec3(x/f,y/f,z/f);
    }
    template<typename V>
    void operator /= (const V& f)
    {
        *this = *this / f;
    }

    T Length2() const
    {
        return x*x + y*y + z*z;
    }

    // Only implemented for T=float
    float Length() const;
    void SetLength(const float l);
    Vec3 WithLength(const float l) const;
    float Distance2To(Vec3 &other);
    Vec3 Normalized() const;
    float Normalize(); // returns the previous length, which is often useful

    T& operator [] (int i) //allow vector[2] = 3   (vector.z=3)
    {
        return *((&x) + i);
    }
    T operator [] (const int i) const
    {
        return *((&x) + i);
    }

    void SetZero()
    {
        x=0; y=0; z=0;
    }

    // Common aliases: UVW (texel coordinates), RGB (colors), STQ (texture coordinates)
    T& u() { return x; }
    T& v() { return y; }
    T& w() { return z; }

    T& r() { return x; }
    T& g() { return y; }
    T& b() { return z; }

    T& s() { return x; }
    T& t() { return y; }
    T& q() { return z; }

    const T& u() const { return x; }
    const T& v() const { return y; }
    const T& w() const { return z; }

    const T& r() const { return x; }
    const T& g() const { return y; }
    const T& b() const { return z; }

    const T& s() const { return x; }
    const T& t() const { return y; }
    const T& q() const { return z; }

    // swizzlers - create a subvector of specific components
    // e.g. Vec2 uv() { return Vec2(x,y); }
    // _DEFINE_SWIZZLER2 defines a single such function, DEFINE_SWIZZLER2 defines all of them for all component names (x<->r) and permutations (xy<->yx)
#define _DEFINE_SWIZZLER2(a, b, name) Vec2<T> name() const { return Vec2<T>(a, b); }
#define DEFINE_SWIZZLER2(a, b, a2, b2, a3, b3, a4, b4) \
    _DEFINE_SWIZZLER2(a, b, a##b); \
    _DEFINE_SWIZZLER2(a, b, a2##b2); \
    _DEFINE_SWIZZLER2(a, b, a3##b3); \
    _DEFINE_SWIZZLER2(a, b, a4##b4); \
    _DEFINE_SWIZZLER2(b, a, b##a); \
    _DEFINE_SWIZZLER2(b, a, b2##a2); \
    _DEFINE_SWIZZLER2(b, a, b3##a3); \
    _DEFINE_SWIZZLER2(b, a, b4##a4);

    DEFINE_SWIZZLER2(x, y, r, g, u, v, s, t);
    DEFINE_SWIZZLER2(x, z, r, b, u, w, s, q);
    DEFINE_SWIZZLER2(y, z, g, b, v, w, t, q);
#undef DEFINE_SWIZZLER2
#undef _DEFINE_SWIZZLER2

    // Inserters to add new elements to effectively create larger vectors containing this Vec2
    Vec4<T> InsertBeforeX(const T& value) {
        return Vec4<T>(value, x, y, z);
    }
    Vec4<T> InsertBeforeY(const T& value) {
        return Vec4<T>(x, value, y, z);
    }
    Vec4<T> InsertBeforeZ(const T& value) {
        return Vec4<T>(x, y, value, z);
    }
    Vec4<T> Append(const T& value) {
        return Vec4<T>(x, y, z, value);
    }
};

template<typename T, typename V>
Vec3<T> operator * (const V& f, const Vec3<T>& vec)
{
    return Vec3<T>(f*vec.x,f*vec.y,f*vec.z);
}

typedef Vec3<float> Vec3f;

template<typename T>
class Vec4
{
public:
    struct
    {
        T x,y,z,w;
    };

    T* AsArray() { return &x; }

    Vec4() = default;
    Vec4(const T a[4]) : x(a[0]), y(a[1]), z(a[2]), w(a[3]) {}
    Vec4(const T& _x, const T& _y, const T& _z, const T& _w) : x(_x), y(_y), z(_z), w(_w) {}

    template<typename T2>
    Vec4<T2> Cast() const {
        return Vec4<T2>((T2)x, (T2)y, (T2)z, (T2)w);
    }

    // Only implemented for T=int and T=float
    static Vec4 FromRGBA(unsigned int rgba);
    unsigned int ToRGBA() const;

    static Vec4 AssignToAll(const T& f) {
        return Vec4<T>(f, f, f, f);
    }

    void Write(T a[4])
    {
        a[0] = x; a[1] = y; a[2] = z; a[3] = w;
    }

    Vec4 operator +(const Vec4& other) const
    {
        return Vec4(x+other.x, y+other.y, z+other.z, w+other.w);
    }
    void operator += (const Vec4& other)
    {
        x+=other.x; y+=other.y; z+=other.z; w+=other.w;
    }
    Vec4 operator -(const Vec4 &other) const
    {
        return Vec4(x-other.x, y-other.y, z-other.z, w-other.w);
    }
    void operator -= (const Vec4 &other)
    {
        x-=other.x; y-=other.y; z-=other.z; w-=other.w;
    }
    Vec4 operator -() const
    {
        return Vec4(-x,-y,-z,-w);
    }
    Vec4 operator * (const Vec4 &other) const
    {
        return Vec4(x*other.x, y*other.y, z*other.z, w*other.w);
    }
    template<typename V>
    Vec4 operator * (const V& f) const
    {
        return Vec4(x*f,y*f,z*f,w*f);
    }
    template<typename V>
    void operator *= (const V& f)
    {
        x*=f; y*=f; z*=f; w*=f;
    }
    template<typename V>
    Vec4 operator / (const V& f) const
    {
        return Vec4(x/f,y/f,z/f,w/f);
    }
    template<typename V>
    void operator /= (const V& f)
    {
        *this = *this / f;
    }

    T Length2() const
    {
        return x*x + y*y + z*z + w*w;
    }

    // Only implemented for T=float
    float Length() const;
    void SetLength(const float l);
    Vec4 WithLength(const float l) const;
    float Distance2To(Vec4 &other);
    Vec4 Normalized() const;
    float Normalize(); // returns the previous length, which is often useful

    T& operator [] (int i) //allow vector[2] = 3   (vector.z=3)
    {
        return *((&x) + i);
    }
    T operator [] (const int i) const
    {
        return *((&x) + i);
    }

    void SetZero()
    {
        x=0; y=0; z=0;
    }

    // Common alias: RGBA (colors)
    T& r() { return x; }
    T& g() { return y; }
    T& b() { return z; }
    T& a() { return w; }

    const T& r() const { return x; }
    const T& g() const { return y; }
    const T& b() const { return z; }
    const T& a() const { return w; }

    // swizzlers - create a subvector of specific components
    // e.g. Vec2 uv() { return Vec2(x,y); }
    // _DEFINE_SWIZZLER2 defines a single such function, DEFINE_SWIZZLER2 defines all of them for all component names (x<->r) and permutations (xy<->yx)
#define _DEFINE_SWIZZLER2(a, b, name) Vec2<T> name() const { return Vec2<T>(a, b); }
#define DEFINE_SWIZZLER2(a, b, a2, b2) \
    _DEFINE_SWIZZLER2(a, b, a##b); \
    _DEFINE_SWIZZLER2(a, b, a2##b2); \
    _DEFINE_SWIZZLER2(b, a, b##a); \
    _DEFINE_SWIZZLER2(b, a, b2##a2);

    DEFINE_SWIZZLER2(x, y, r, g);
    DEFINE_SWIZZLER2(x, z, r, b);
    DEFINE_SWIZZLER2(x, w, r, a);
    DEFINE_SWIZZLER2(y, z, g, b);
    DEFINE_SWIZZLER2(y, w, g, a);
    DEFINE_SWIZZLER2(z, w, b, a);
#undef DEFINE_SWIZZLER2
#undef _DEFINE_SWIZZLER2

#define _DEFINE_SWIZZLER3(a, b, c, name) Vec3<T> name() const { return Vec3<T>(a, b, c); }
#define DEFINE_SWIZZLER3(a, b, c, a2, b2, c2) \
    _DEFINE_SWIZZLER3(a, b, c, a##b##c); \
    _DEFINE_SWIZZLER3(a, c, b, a##c##b); \
    _DEFINE_SWIZZLER3(b, a, c, b##a##c); \
    _DEFINE_SWIZZLER3(b, c, a, b##c##a); \
    _DEFINE_SWIZZLER3(c, a, b, c##a##b); \
    _DEFINE_SWIZZLER3(c, b, a, c##b##a); \
    _DEFINE_SWIZZLER3(a, b, c, a2##b2##c2); \
    _DEFINE_SWIZZLER3(a, c, b, a2##c2##b2); \
    _DEFINE_SWIZZLER3(b, a, c, b2##a2##c2); \
    _DEFINE_SWIZZLER3(b, c, a, b2##c2##a2); \
    _DEFINE_SWIZZLER3(c, a, b, c2##a2##b2); \
    _DEFINE_SWIZZLER3(c, b, a, c2##b2##a2);

    DEFINE_SWIZZLER3(x, y, z, r, g, b);
    DEFINE_SWIZZLER3(x, y, w, r, g, a);
    DEFINE_SWIZZLER3(x, z, w, r, b, a);
    DEFINE_SWIZZLER3(y, z, w, g, b, a);
#undef DEFINE_SWIZZLER3
#undef _DEFINE_SWIZZLER3
};


template<typename T, typename V>
Vec4<T> operator * (const V& f, const Vec4<T>& vec)
{
    return Vec4<T>(f*vec.x,f*vec.y,f*vec.z,f*vec.w);
}

typedef Vec4<float> Vec4f;


template<typename T>
static inline T Dot(const Vec2<T>& a, const Vec2<T>& b)
{
    return a.x*b.x + a.y*b.y;
}

template<typename T>
static inline T Dot(const Vec3<T>& a, const Vec3<T>& b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

template<typename T>
static inline T Dot(const Vec4<T>& a, const Vec4<T>& b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
}

template<typename T>
static inline Vec3<T> Cross(const Vec3<T>& a, const Vec3<T>& b)
{
    return Vec3<T>(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x);
}

// linear interpolation via float: 0.0=begin, 1.0=end
template<typename X>
static inline X Lerp(const X& begin, const X& end, const float t)
{
    return begin*(1.f-t) + end*t;
}

// linear interpolation via int: 0=begin, base=end
template<typename X, int base>
static inline X LerpInt(const X& begin, const X& end, const int t)
{
    return (begin*(base-t) + end*t) / base;
}

// Utility vector factories
template<typename T>
static inline Vec2<T> MakeVec2(const T& x, const T& y)
{
    return Vec2<T>{x, y};
}

template<typename T>
static inline Vec3<T> MakeVec3(const T& x, const T& y, const T& z)
{
    return Vec3<T>{x, y, z};
}

template<typename T>
static inline Vec4<T> MakeVec4(const T& x, const T& y, const T& z, const T& w)
{
    return Vec4<T>{x, y, z, w};
}

} // namespace

// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <cstdlib>
#include <type_traits>

namespace MathUtil
{

template<typename T>
inline T Clamp(const T val, const T& min, const T& max)
{
    return std::max(min, std::min(max, val));
}

template<class T>
struct Rectangle
{
    T left;
    T top;
    T right;
    T bottom;

    Rectangle() {}

    Rectangle(T left, T top, T right, T bottom) : left(left), top(top), right(right), bottom(bottom) {}

    T GetWidth() const { return std::abs(static_cast<typename std::make_signed<T>::type>(right - left)); }
    T GetHeight() const { return std::abs(static_cast<typename std::make_signed<T>::type>(bottom - top)); }
};

}  // namespace MathUtil

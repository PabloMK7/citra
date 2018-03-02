// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

// This hack is needed to support json.hpp on platforms where the C++17 stdlib
// lacks std::string_view. See https://github.com/nlohmann/json/issues/735.
// clang-format off
#if !__has_include(<string_view>) && __has_include(<experimental/string_view>)
# include <experimental/string_view>
# define string_view experimental::string_view
# include <json.hpp>
# undef string_view
#else
# include <json.hpp>
#endif
// clang-format on

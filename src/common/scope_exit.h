// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace detail {
    template <typename Func>
    struct ScopeExitHelper {
        explicit ScopeExitHelper(Func&& func) : func(std::move(func)) {}
        ~ScopeExitHelper() { func(); }

        Func func;
    };

    template <typename Func>
    ScopeExitHelper<Func> ScopeExit(Func&& func) { return ScopeExitHelper<Func>(std::move(func)); }
}

/**
 * This macro allows you to conveniently specify a block of code that will run on scope exit. Handy
 * for doing ad-hoc clean-up tasks in a function with multiple returns.
 *
 * Example usage:
 * \code
 * const int saved_val = g_foo;
 * g_foo = 55;
 * SCOPE_EXIT({ g_foo = saved_val; });
 *
 * if (Bar()) {
 *     return 0;
 * } else {
 *     return 20;
 * }
 * \endcode
 */
#define SCOPE_EXIT(body) auto scope_exit_helper_##__LINE__ = detail::ScopeExit([&]() body)

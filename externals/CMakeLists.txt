# Definitions for all external bundled libraries

# Catch
add_library(catch-single-include INTERFACE)
target_include_directories(catch-single-include INTERFACE catch/single_include)

# Crypto++
add_subdirectory(cryptopp)

# Dynarmic
# Dynarmic will skip defining xbyak if it's already defined, we then define it below
add_library(xbyak INTERFACE)
option(DYNARMIC_TESTS OFF)
set(DYNARMIC_NO_BUNDLED_FMT ON)
add_subdirectory(dynarmic)

# libfmt
add_subdirectory(fmt)

# getopt
if (MSVC)
    add_subdirectory(getopt)
endif()

# Glad
add_subdirectory(glad)

# inih
add_subdirectory(inih)

# MicroProfile
add_library(microprofile INTERFACE)
target_include_directories(microprofile INTERFACE ./microprofile)

# Nihstro
add_library(nihstro-headers INTERFACE)
target_include_directories(nihstro-headers INTERFACE ./nihstro/include)

# SoundTouch
add_subdirectory(soundtouch)
# The SoundTouch target doesn't export the necessary include paths as properties by default
target_include_directories(SoundTouch INTERFACE ./soundtouch/include)

# Xbyak
if (ARCHITECTURE_x86_64)
    # Defined before "dynarmic" above
    # add_library(xbyak INTERFACE)
    target_include_directories(xbyak INTERFACE ./xbyak/xbyak)
    target_compile_definitions(xbyak INTERFACE XBYAK_NO_OP_NAMES)
endif()

# ENet
add_subdirectory(enet)
target_include_directories(enet INTERFACE ./enet/include)

if (ENABLE_WEB_SERVICE)
    # CPR
    option(BUILD_TESTING OFF)
    option(BUILD_CPR_TESTS OFF)
    add_subdirectory(cpr)
    target_include_directories(cpr INTERFACE ./cpr/include)

    # JSON
    add_library(json-headers INTERFACE)
    target_include_directories(json-headers INTERFACE ./json)
endif()

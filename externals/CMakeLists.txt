# Definitions for all external bundled libraries

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)
include(DownloadExternals)

# Catch
add_library(catch-single-include INTERFACE)
target_include_directories(catch-single-include INTERFACE catch/single_include)

# Crypto++
add_subdirectory(cryptopp)

# Dynarmic
if (ARCHITECTURE_x86_64)
    # Dynarmic will skip defining xbyak if it's already defined, we then define it below
    add_library(xbyak INTERFACE)
    option(DYNARMIC_TESTS OFF)
    set(DYNARMIC_NO_BUNDLED_FMT ON)
    add_subdirectory(dynarmic)
endif()

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
    # msys installed curl is configured to use openssl, but that isn't portable
    # since it relies on having the bundled certs install in the home folder for SSL
    # by default on mingw, download the precompiled curl thats linked against windows native ssl
    if (MINGW AND CITRA_USE_BUNDLED_CURL)
        download_bundled_external("curl/" "curl-7_55_1" CURL_PREFIX)
        set(CURL_PREFIX "${CMAKE_BINARY_DIR}/externals/curl-7_55_1")
        set(CURL_FOUND YES)
        set(CURL_INCLUDE_DIR "${CURL_PREFIX}/include" CACHE PATH "Path to curl headers")
        set(CURL_LIBRARY "${CURL_PREFIX}/lib/libcurldll.a" CACHE PATH "Path to curl library")
        set(CURL_DLL_DIR "${CURL_PREFIX}/lib/" CACHE PATH "Path to curl.dll")
        set(USE_SYSTEM_CURL ON CACHE BOOL "")
    endif()
    # CPR
    set(BUILD_TESTING OFF CACHE BOOL "")
    set(BUILD_CPR_TESTS OFF CACHE BOOL "")
    add_subdirectory(cpr)
    target_include_directories(cpr INTERFACE ./cpr/include)

    # JSON
    add_library(json-headers INTERFACE)
    target_include_directories(json-headers INTERFACE ./json)
endif()

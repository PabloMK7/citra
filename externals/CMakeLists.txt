# Definitions for all external bundled libraries

# Suppress warnings from external libraries
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/W0)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)
include(DownloadExternals)
include(ExternalProject)

# Boost
set(BOOST_ROOT "${CMAKE_SOURCE_DIR}/externals/boost")
set(Boost_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/externals/boost")
set(Boost_NO_SYSTEM_PATHS ON)
add_library(boost INTERFACE)
target_include_directories(boost SYSTEM INTERFACE ${Boost_INCLUDE_DIR})

# Boost::serialization
file(GLOB boost_serialization_SRC "${CMAKE_SOURCE_DIR}/externals/boost/libs/serialization/src/*.cpp")
add_library(boost_serialization STATIC ${boost_serialization_SRC})
target_link_libraries(boost_serialization PUBLIC boost)

# Boost::iostreams
add_library(
    boost_iostreams
    STATIC
    ${CMAKE_SOURCE_DIR}/externals/boost/libs/iostreams/src/file_descriptor.cpp
    ${CMAKE_SOURCE_DIR}/externals/boost/libs/iostreams/src/mapped_file.cpp
)
target_link_libraries(boost_iostreams PUBLIC boost)

# Add additional boost libs here; remember to ALIAS them in the root CMakeLists!

# Catch2
set(CATCH_INSTALL_DOCS OFF)
set(CATCH_INSTALL_EXTRAS OFF)
add_subdirectory(catch2)

# Crypto++
set(CRYPTOPP_BUILD_DOCUMENTATION OFF)
set(CRYPTOPP_BUILD_TESTING OFF)
set(CRYPTOPP_INSTALL OFF)
set(CRYPTOPP_SOURCES "${CMAKE_SOURCE_DIR}/externals/cryptopp")
add_subdirectory(cryptopp-cmake)

# HACK: Mismatch between compilation of CryptoPP and headers used in Citra can cause runtime issues.
# Pull out the compile definitions from CryptoPP and apply them to Citra as well to fix this.
# See: https://github.com/weidai11/cryptopp/issues/1191
get_source_file_property(CRYPTOPP_COMPILE_DEFINITIONS ${CRYPTOPP_SOURCES}/cryptlib.cpp TARGET_DIRECTORY cryptopp COMPILE_DEFINITIONS)
set(CRYPTOPP_COMPILE_DEFINITIONS ${CRYPTOPP_COMPILE_DEFINITIONS} PARENT_SCOPE)

# HACK: The logic to set up the base include directory for CryptoPP does not work with Android SDK CMake 3.22.1.
# Until there is a fixed version available, this code will detect and add in the proper include if it does not exist.
if(ANDROID)
    message(STATUS "Applying CryptoPP include fix.")
    get_target_property(CRYPTOPP_INCLUDES cryptopp INTERFACE_INCLUDE_DIRECTORIES)
    foreach(CRYPTOPP_INCLUDE ${CRYPTOPP_INCLUDES})
        if("${CRYPTOPP_INCLUDE}" MATCHES "\\$<BUILD_INTERFACE:(.*)/cryptopp>")
            message(STATUS "Fixed include path: ${CMAKE_MATCH_1}")
            target_include_directories(cryptopp PUBLIC $<BUILD_INTERFACE:${CMAKE_MATCH_1}>)
            break()
        endif()
    endforeach()
endif()

# dds-ktx
add_library(dds-ktx INTERFACE)
target_include_directories(dds-ktx INTERFACE ./dds-ktx)

# fmt and Xbyak need to be added before dynarmic
# libfmt
option(FMT_INSTALL "" ON)
add_subdirectory(fmt EXCLUDE_FROM_ALL)

# Xbyak
if ("x86_64" IN_LIST ARCHITECTURE)
    add_subdirectory(xbyak EXCLUDE_FROM_ALL)
endif()

# Dynarmic
if ("x86_64" IN_LIST ARCHITECTURE OR "arm64" IN_LIST ARCHITECTURE)
    set(DYNARMIC_TESTS OFF)
    set(DYNARMIC_FRONTENDS "A32")
    add_subdirectory(dynarmic EXCLUDE_FROM_ALL)
endif()

# getopt
if (MSVC)
    add_subdirectory(getopt)
endif()

# Glad
add_subdirectory(glad)

# glslang
set(SKIP_GLSLANG_INSTALL ON)
set(ENABLE_GLSLANG_BINARIES OFF)
set(ENABLE_SPVREMAPPER OFF)
set(ENABLE_CTEST OFF)
add_subdirectory(glslang)

# inih
add_subdirectory(inih)

# MicroProfile
add_library(microprofile INTERFACE)
target_include_directories(microprofile INTERFACE ./microprofile)

# Nihstro
add_library(nihstro-headers INTERFACE)
target_include_directories(nihstro-headers INTERFACE ./nihstro/include)
if (MSVC)
    target_compile_options(nihstro-headers INTERFACE /W0)
endif()

# Open Source Archives
add_subdirectory(open_source_archives)

# SoundTouch
set(INTEGER_SAMPLES ON CACHE BOOL "")
set(SOUNDSTRETCH OFF CACHE BOOL "")
set(SOUNDTOUCH_DLL OFF CACHE BOOL "")
add_subdirectory(soundtouch EXCLUDE_FROM_ALL)

# sirit
add_subdirectory(sirit EXCLUDE_FROM_ALL)

# Teakra
add_subdirectory(teakra EXCLUDE_FROM_ALL)

# SDL2
if (ENABLE_SDL2 AND NOT USE_SYSTEM_SDL2)
    add_subdirectory(sdl2)
endif()

# Zstandard
set(ZSTD_LEGACY_SUPPORT OFF)
set(ZSTD_BUILD_PROGRAMS OFF)
set(ZSTD_BUILD_SHARED OFF)
add_subdirectory(zstd/build/cmake EXCLUDE_FROM_ALL)
target_include_directories(libzstd_static INTERFACE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/externals/zstd/lib>)

# ENet
add_subdirectory(enet)
target_include_directories(enet INTERFACE ./enet/include)

# Cubeb
if (ENABLE_CUBEB)
    set(BUILD_TESTS OFF CACHE BOOL "")
    add_subdirectory(cubeb EXCLUDE_FROM_ALL)
endif()

# DiscordRPC
if (USE_DISCORD_PRESENCE)
    add_subdirectory(discord-rpc EXCLUDE_FROM_ALL)
    target_include_directories(discord-rpc INTERFACE ./discord-rpc/include)
endif()

# JSON
add_library(json-headers INTERFACE)
target_include_directories(json-headers INTERFACE ./json)

if (ENABLE_WEB_SERVICE)
    find_package(OpenSSL 1.1)
    if (OPENSSL_FOUND)
        set(OPENSSL_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
    else()
        # LibreSSL
        set(LIBRESSL_SKIP_INSTALL ON CACHE BOOL "")
        set(OPENSSLDIR "/etc/ssl/")
        add_subdirectory(libressl EXCLUDE_FROM_ALL)
        target_include_directories(ssl INTERFACE ./libressl/include)
        target_compile_definitions(ssl PRIVATE -DHAVE_INET_NTOP)
        get_directory_property(OPENSSL_LIBRARIES
            DIRECTORY libressl
            DEFINITION OPENSSL_LIBS)
    endif()

    if(ANDROID)
        add_subdirectory(android-ifaddrs)
    endif()

    # httplib
    add_library(httplib INTERFACE)
    target_include_directories(httplib INTERFACE ./httplib)
    target_compile_options(httplib INTERFACE -DCPPHTTPLIB_OPENSSL_SUPPORT)
    target_link_libraries(httplib INTERFACE ${OPENSSL_LIBRARIES})

    # cpp-jwt
    add_library(cpp-jwt INTERFACE)
    target_include_directories(cpp-jwt INTERFACE ./cpp-jwt/include)
    target_compile_definitions(cpp-jwt INTERFACE CPP_JWT_USE_VENDORED_NLOHMANN_JSON)
endif()

# lodepng
add_subdirectory(lodepng)

# (xperia64): Only use libyuv on Android b/c of build issues on Windows and mandatory JPEG
if(ANDROID)
    # libyuv
    add_subdirectory(libyuv)
    target_include_directories(yuv INTERFACE ./libyuv/include)
endif()

# OpenAL Soft
if (ENABLE_OPENAL)
    set(ALSOFT_EMBED_HRTF_DATA OFF CACHE BOOL "")
    set(ALSOFT_EXAMPLES OFF CACHE BOOL "")
    set(ALSOFT_INSTALL OFF CACHE BOOL "")
    set(ALSOFT_INSTALL_CONFIG OFF CACHE BOOL "")
    set(ALSOFT_INSTALL_HRTF_DATA OFF CACHE BOOL "")
    set(ALSOFT_INSTALL_AMBDEC_PRESETS OFF CACHE BOOL "")
    set(ALSOFT_UTILS OFF CACHE BOOL "")
    set(LIBTYPE "STATIC" CACHE STRING "")
    add_subdirectory(openal-soft EXCLUDE_FROM_ALL)
endif()

# VMA
add_library(vma INTERFACE)
target_include_directories(vma INTERFACE ./vma/include)

# vulkan-headers
add_library(vulkan-headers INTERFACE)
target_include_directories(vulkan-headers INTERFACE ./vulkan-headers/include)

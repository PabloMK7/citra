# Definitions for all external bundled libraries

# Suppress warnings from external libraries
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/W0)
else()
    add_compile_options(-w)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)
include(DownloadExternals)
include(ExternalProject)

# Boost
if (NOT USE_SYSTEM_BOOST)
    message(STATUS "Including vendored Boost library")
    set(BOOST_ROOT "${CMAKE_SOURCE_DIR}/externals/boost" CACHE STRING "")
    set(Boost_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/externals/boost" CACHE STRING "")
    set(Boost_NO_SYSTEM_PATHS ON CACHE BOOL "")
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
endif()

# Catch2
set(CATCH_INSTALL_DOCS OFF CACHE BOOL "")
set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "")
add_subdirectory(catch2)

# Crypto++
set(CRYPTOPP_BUILD_DOCUMENTATION OFF CACHE BOOL "")
set(CRYPTOPP_BUILD_TESTING OFF CACHE BOOL "")
set(CRYPTOPP_INSTALL OFF CACHE BOOL "")
set(CRYPTOPP_SOURCES "${CMAKE_SOURCE_DIR}/externals/cryptopp" CACHE STRING "")
add_subdirectory(cryptopp-cmake)

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
    set(DYNARMIC_TESTS OFF CACHE BOOL "")
    set(DYNARMIC_FRONTENDS "A32" CACHE STRING "")
    add_subdirectory(dynarmic EXCLUDE_FROM_ALL)
endif()

# getopt
if (MSVC)
    add_subdirectory(getopt)
endif()

# Glad
add_subdirectory(glad)

# glslang
set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "")
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "")
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "")
set(ENABLE_CTEST OFF CACHE BOOL "")
set(ENABLE_HLSL OFF CACHE BOOL "")
set(BUILD_EXTERNAL OFF CACHE BOOL "")
add_subdirectory(glslang)

# inih
add_subdirectory(inih)

# MicroProfile
add_library(microprofile INTERFACE)
target_include_directories(microprofile SYSTEM INTERFACE ./microprofile)

# Nihstro
add_library(nihstro-headers INTERFACE)
target_include_directories(nihstro-headers SYSTEM INTERFACE ./nihstro/include)
if (MSVC)
    # TODO: For some reason MSVC still applies this warning even with /W0 for externals.
    target_compile_options(nihstro-headers INTERFACE /wd4715)
endif()

# Open Source Archives
add_subdirectory(open_source_archives)

# Dynamic library headers
add_subdirectory(library-headers EXCLUDE_FROM_ALL)

# SoundTouch
if(NOT USE_SYSTEM_SOUNDTOUCH)
    set(INTEGER_SAMPLES ON CACHE BOOL "")
    set(SOUNDSTRETCH OFF CACHE BOOL "")
    set(SOUNDTOUCH_DLL OFF CACHE BOOL "")
    add_subdirectory(soundtouch EXCLUDE_FROM_ALL)
    target_compile_definitions(SoundTouch PUBLIC SOUNDTOUCH_INTEGER_SAMPLES)
endif()

# sirit
add_subdirectory(sirit EXCLUDE_FROM_ALL)

# Teakra
add_subdirectory(teakra EXCLUDE_FROM_ALL)

# SDL2
if (ENABLE_SDL2 AND NOT USE_SYSTEM_SDL2)
    add_subdirectory(sdl2)
endif()

# libusb
if (ENABLE_LIBUSB AND NOT USE_SYSTEM_LIBUSB)
    add_subdirectory(libusb)
    set(LIBUSB_INCLUDE_DIR "" PARENT_SCOPE)
    set(LIBUSB_LIBRARIES usb PARENT_SCOPE)
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
    set(BUILD_TOOLS OFF CACHE BOOL "")
    set(BUNDLE_SPEEX ON CACHE BOOL "")
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

# OpenSSL
if (USE_SYSTEM_OPENSSL)
    find_package(OpenSSL 1.1)
    if (OPENSSL_FOUND)
        set(OPENSSL_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
    endif()
endif()

if (NOT OPENSSL_FOUND)
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

# httplib
add_library(httplib INTERFACE)
target_include_directories(httplib INTERFACE ./httplib)
target_compile_options(httplib INTERFACE -DCPPHTTPLIB_OPENSSL_SUPPORT)
target_link_libraries(httplib INTERFACE ${OPENSSL_LIBRARIES})

if(ANDROID)
    add_subdirectory(android-ifaddrs)
    target_link_libraries(httplib INTERFACE ifaddrs)
endif()

# cpp-jwt
if (ENABLE_WEB_SERVICE)
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
target_include_directories(vma SYSTEM INTERFACE ./vma/include)

# vulkan-headers
add_library(vulkan-headers INTERFACE)
target_include_directories(vulkan-headers SYSTEM INTERFACE ./vulkan-headers/include)
if (APPLE)
    target_include_directories(vulkan-headers SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/MoltenVK)
endif()

# adrenotools
if (ANDROID)
    add_subdirectory(libadrenotools)
endif()

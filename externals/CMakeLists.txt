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

# fmt and Xbyak need to be added before dynarmic
# libfmt
option(FMT_INSTALL "" ON)
add_subdirectory(fmt)

# Xbyak
if ("x86_64" IN_LIST ARCHITECTURE)
    add_subdirectory(xbyak)
endif()

# Dynarmic
if ("x86_64" IN_LIST ARCHITECTURE OR "arm64" IN_LIST ARCHITECTURE)
    set(DYNARMIC_TESTS OFF)
    set(DYNARMIC_FRONTENDS "A32")
    add_subdirectory(dynarmic)
endif()

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

# Open Source Archives
add_subdirectory(open_source_archives)

# SoundTouch
add_subdirectory(soundtouch)
# The SoundTouch target doesn't export the necessary include paths as properties by default
target_include_directories(SoundTouch INTERFACE ./soundtouch/include)

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
    # JSON
    add_library(json-headers INTERFACE)
    target_include_directories(json-headers INTERFACE ./json)

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

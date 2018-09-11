# Definitions for all external bundled libraries

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)
include(DownloadExternals)
include(ExternalProject)

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
add_library(fmt::fmt ALIAS fmt)

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

# Cubeb
if (ENABLE_CUBEB)
    set(BUILD_TESTS OFF CACHE BOOL "")
    add_subdirectory(cubeb EXCLUDE_FROM_ALL)
endif()

# DiscordRPC
if (USE_DISCORD_PRESENCE)
    add_subdirectory(discord-rpc)
    target_include_directories(discord-rpc INTERFACE ./discord-rpc/include)
endif()

if (ENABLE_WEB_SERVICE)
    # LibreSSL
    set(LIBRESSL_SKIP_INSTALL ON CACHE BOOL "")
    add_definitions(-DHAVE_INET_NTOP)
    add_subdirectory(libressl)
    target_include_directories(ssl INTERFACE ./libressl/include)

    # lurlparser
    add_subdirectory(lurlparser)

    # httplib
    add_library(httplib INTERFACE)
    target_include_directories(httplib INTERFACE ./httplib)

    # JSON
    add_library(json-headers INTERFACE)
    target_include_directories(json-headers INTERFACE ./json)
endif()

# ZeroMQ
# libzmq includes its own clang-format target, which conflicts with the
# clang-format in Citra if libzmq is added as a subdirectory. An external
# project gets around this issue. Unfortunately, a lot of different
# configuration options are required for each different platform. An
# attempt was made to use CMake variables where possible, but some
# information necessarily had to be repeated. Hopefully there is not
# often a need to change anything.
if (MINGW)
    if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
        set(LIBZMQ_MAKE mingw32-make)
        set(LIBZMQ_COMPILER "")
        set(LIBZMQ_TOOLCHAIN_FILE "")
    else()
        set(LIBZMQ_MAKE make)
        set(LIBZMQ_COMPILER -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER};-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER})
        set(LIBZMQ_TOOLCHAIN_FILE -DCMAKE_TOOLCHAIN_FILE=${PROJECT_SOURCE_DIR}/CMakeModules/MinGWCross.cmake)
    endif()
    ExternalProject_Add(libzmq-external
        SOURCE_DIR ./libzmq
        CMAKE_ARGS -DWITH_PERF_TOOL=OFF;-DZMQ_BUILD_TESTS=OFF;-DENABLE_CPACK=OFF;-DCMAKE_MAKE_PROGRAM=${LIBZMQ_MAKE};-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE};${LIBZMQ_TOOLCHAIN_FILE};${LIBZMQ_COMPILER}
        BUILD_COMMAND cmake --build ${CMAKE_CURRENT_BINARY_DIR}/libzmq-external-prefix/src/libzmq-external-build --target libzmq-static --config ${CMAKE_BUILD_TYPE}
        GIT_REPOSITORY https://github.com/zeromq/libzmq
        GIT_TAG v4.2.5
        INSTALL_COMMAND "")
else()
    if (MSVC)
        set(LIBZMQ_COMPILER_FLAGS -DCMAKE_C_FLAGS=/GL-;-DCMAKE_CXX_FLAGS=/GL-)
    else()
        set(LIBZMQ_COMPILER_FLAGS "")
    endif()
    ExternalProject_Add(libzmq-external
        SOURCE_DIR ./libzmq
        CMAKE_ARGS -DCMAKE_MACOSX_RPATH=1;-DCMAKE_OSX_ARCHITECTURES=x86_64;-DWITH_PERF_TOOL=OFF;-DZMQ_BUILD_TESTS=OFF;-DENABLE_CPACK=OFF;-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE};${LIBZMQ_COMPILER_FLAGS}
        BUILD_COMMAND cmake --build ${CMAKE_CURRENT_BINARY_DIR}/libzmq-external-prefix/src/libzmq-external-build --target libzmq-static --config ${CMAKE_BUILD_TYPE}
        GIT_REPOSITORY https://github.com/zeromq/libzmq
        GIT_TAG v4.2.5
        INSTALL_COMMAND "")
endif()
set(LIBZMQ_DIR ${CMAKE_CURRENT_BINARY_DIR}/libzmq-external-prefix/src/libzmq-external-build/lib)
# On macOS, we need to build a fat static library containing both x86_64 and x86_64h, since macOS
# targets specify two architectures in their link command line ("-arch x86_64 -arch x86_64h").
if (APPLE)
    ExternalProject_Add(libzmq-external-h
        SOURCE_DIR ./libzmq-h
        CMAKE_ARGS -DCMAKE_MACOSX_RPATH=1;-DCMAKE_OSX_ARCHITECTURES=x86_64h;-DWITH_PERF_TOOL=OFF;-DZMQ_BUILD_TESTS=OFF;-DENABLE_CPACK=OFF;-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        BUILD_COMMAND cmake --build ${CMAKE_CURRENT_BINARY_DIR}/libzmq-external-h-prefix/src/libzmq-external-h-build --target libzmq-static --config ${CMAKE_BUILD_TYPE}
        GIT_REPOSITORY https://github.com/zeromq/libzmq
        GIT_TAG v4.2.5
        INSTALL_COMMAND "")
    set(LIBZMQ_H_DIR ${CMAKE_CURRENT_BINARY_DIR}/libzmq-external-h-prefix/src/libzmq-external-h-build/lib)

    add_library(libzmq-external-imported STATIC IMPORTED GLOBAL)
    add_library(libzmq-external-imported-h STATIC IMPORTED GLOBAL)
    add_dependencies(libzmq-external-imported libzmq-external)
    add_dependencies(libzmq-external-imported-h libzmq-external-h)
else()
    add_library(libzmq STATIC IMPORTED GLOBAL)
    add_dependencies(libzmq libzmq-external)
endif()
# Set up the imported target properties
if (MSVC)
    set_target_properties(libzmq PROPERTIES IMPORTED_LOCATION ${LIBZMQ_DIR}/${CMAKE_BUILD_TYPE}/libzmq-v141-mt-s-4_2_5${CMAKE_STATIC_LIBRARY_SUFFIX})
    set_target_properties(libzmq PROPERTIES IMPORTED_LINK_INTERFACE_LIBRARIES iphlpapi${CMAKE_STATIC_LIBRARY_SUFFIX})
else()
    if (APPLE)
        set_target_properties(libzmq-external-imported PROPERTIES IMPORTED_LOCATION ${LIBZMQ_DIR}/libzmq${CMAKE_STATIC_LIBRARY_SUFFIX})
        set_target_properties(libzmq-external-imported-h PROPERTIES IMPORTED_LOCATION ${LIBZMQ_H_DIR}/libzmq${CMAKE_STATIC_LIBRARY_SUFFIX})
    else()
        set_target_properties(libzmq PROPERTIES IMPORTED_LOCATION ${LIBZMQ_DIR}/libzmq${CMAKE_STATIC_LIBRARY_SUFFIX})
        if(MINGW)
            set_target_properties(libzmq PROPERTIES IMPORTED_LINK_INTERFACE_LIBRARIES "ws2_32${CMAKE_STATIC_LIBRARY_SUFFIX};iphlpapi${CMAKE_STATIC_LIBRARY_SUFFIX}")
        endif()
    endif()
endif()
# On macOS, create the combined target
if (APPLE)
    set(LIBZMQ_COMBINED_OUTPUT ${LIBZMQ_DIR}/libzmq_combined${CMAKE_STATIC_LIBRARY_SUFFIX})
    add_custom_target(libzmq-combined COMMAND lipo -create ${LIBZMQ_DIR}/libzmq${CMAKE_STATIC_LIBRARY_SUFFIX} ${LIBZMQ_H_DIR}/libzmq${CMAKE_STATIC_LIBRARY_SUFFIX} -o ${LIBZMQ_COMBINED_OUTPUT}
                                      BYPRODUCTS ${LIBZMQ_COMBINED_OUTPUT})
    add_dependencies(libzmq-combined libzmq-external-imported libzmq-external-imported-h)
    add_library(libzmq STATIC IMPORTED GLOBAL)
    set_target_properties(libzmq PROPERTIES IMPORTED_LOCATION ${LIBZMQ_COMBINED_OUTPUT})
    add_dependencies(libzmq libzmq-combined)
endif()
# C interface to ZeroMQ
add_library(libzmq-headers INTERFACE)
target_include_directories(libzmq-headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/libzmq/include)
# C++ interface to ZeroMQ
add_library(cppzmq-headers INTERFACE)
target_include_directories(cppzmq-headers INTERFACE ./cppzmq)
add_dependencies(cppzmq-headers libzmq)

# The CMakeLists.txt shipped with cryptopp pollutes our option list and installation list,
# so we made our own one. This is basically a trimmed down version of the shipped CMakeLists.txt
# The differences are:
#  - removed support for legacy CMake versions
#  - removed support for 32-bit
#  - removed rdrand module.asm as a workaround for an issue (see below)
#  - added prefix "CRYPTOPP_" to all option names
#  - disabled testing
#  - disabled installation
#  - disabled documentation
#  - configured to build a static library only

include(TestBigEndian)
include(CheckCXXCompilerFlag)

#============================================================================
# Settable options
#============================================================================

option(CRYPTOPP_DISABLE_ASM "Disable ASM" OFF)
option(CRYPTOPP_DISABLE_SSSE3 "Disable SSSE3" OFF)
option(CRYPTOPP_DISABLE_AESNI "Disable AES-NI" OFF)
option(CRYPTOPP_DISABLE_CXXFLAGS_OPTIMIZATIONS "Disable CXXFLAGS optimizations" OFF)

#============================================================================
# Internal compiler options
#============================================================================

# Only set when cross-compiling, http://www.vtk.org/Wiki/CMake_Cross_Compiling
if (NOT (CMAKE_SYSTEM_VERSION AND CMAKE_SYSTEM_PROCESSOR))
    set(CRYPTOPP_CROSS_COMPILE 1)
else()
    set(CRYPTOPP_CROSS_COMPILE 0)
endif()

# Don't use RPATH's. The resulting binary could fail a security audit.
if (NOT CMAKE_VERSION VERSION_LESS 2.8.12)
  set(CMAKE_MACOSX_RPATH 0)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Intel")
    add_definitions(-wd68 -wd186 -wd279 -wd327 -wd161 -wd3180)
endif()

# Endianness
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if(IS_BIG_ENDIAN)
    add_definitions(-DIS_BIG_ENDIAN)
endif()

if(CRYPTOPP_DISABLE_ASM)
    add_definitions(-DCRYPTOPP_DISABLE_ASM)
endif()
if(CRYPTOPP_DISABLE_SSSE3)
    add_definitions(-DCRYPTOPP_DISABLE_SSSE3)
endif()
if(CRYPTOPP_DISABLE_AESNI)
    add_definitions(-DCRYPTOPP_DISABLE_AESNI)
endif()

# We need the output 'uname -s' for Unix and Linux system detection
if (NOT CRYPTOPP_CROSS_COMPILE)
    set (UNAME_CMD "uname")
    set (UNAME_ARG "-s")
    execute_process(COMMAND ${UNAME_CMD} ${UNAME_ARG}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        RESULT_VARIABLE UNAME_RESULT
        OUTPUT_VARIABLE UNAME_SYSTEM)
        string(REGEX REPLACE "\n$" "" UNAME_SYSTEM "${UNAME_SYSTEM}")
endif()

# We need the output 'uname -m' for Unix and Linux platform detection
if (NOT CRYPTOPP_CROSS_COMPILE)
    set (UNAME_CMD "uname")
    set (UNAME_ARG "-m")
    execute_process(COMMAND ${UNAME_CMD} ${UNAME_ARG}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        RESULT_VARIABLE UNAME_RESULT
        OUTPUT_VARIABLE UNAME_MACHINE)
    string(REGEX REPLACE "\n$" "" UNAME_MACHINE "${UNAME_MACHINE}")
endif()

if(WINDOWS_STORE OR WINDOWS_PHONE)
    if("${CMAKE_SYSTEM_VERSION}" MATCHES "10\\.0.*")
        SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /D\"_WIN32_WINNT=0x0A00\"" )
    endif()
    SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /FI\"winapifamily.h\"" )
endif()

# Enable PIC for all targets except Windows and 32-bit x86.
#   Avoid on 32-bit x86 due to register pressures.
if ((NOT CRYPTOPP_CROSS_COMPILE) AND (NOT (WINDOWS OR WINDOWS_STORE OR WINDOWS_PHONE)))
    # Use Regex; match i386, i486, i586 and i686
    if (NOT (${UNAME_MACHINE} MATCHES "i.86"))
        SET(CMAKE_POSITION_INDEPENDENT_CODE 1)
    endif()
endif()

# -march=native for GCC, Clang and ICC in any version that does support it.
if ((NOT CRYPTOPP_DISABLE_CXXFLAGS_OPTIMIZATIONS) AND (NOT CRYPTOPP_CROSS_COMPILE) AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU|Intel"))
    CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_OPT_ARCH_NATIVE_SUPPORTED)
    if (COMPILER_OPT_ARCH_NATIVE_SUPPORTED AND NOT CMAKE_CXX_FLAGS MATCHES "-march=")
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
    endif()
endif()

# Link is driven through the compiler, but CXXFLAGS are not used. Also see
#   http://public.kitware.com/pipermail/cmake/2003-June/003967.html
if (NOT (WINDOWS OR WINDOWS_STORE OR WINDOWS_PHONE))
    SET(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

#============================================================================
# Sources & headers
#============================================================================

# Library headers
file(GLOB cryptopp_HEADERS cryptopp/*.h)

# Library sources. You can use the GNUmakefile to generate the list: `make sources`.
file(GLOB cryptopp_SOURCES cryptopp/*.cpp)
list(REMOVE_ITEM cryptopp_SOURCES
        # These are removed in the original CMakeLists.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/pch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/simple.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/winpipes.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/cryptlib_bds.cpp
        ${cryptopp_SOURCES_TEST}
        )

if(MINGW OR WIN32)
    list(APPEND cryptopp_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/winpipes.cpp)
endif()

if(MSVC AND NOT CRYPTOPP_DISABLE_ASM)
    if(${CMAKE_GENERATOR} MATCHES ".*ARM")
        message(STATUS "Disabling ASM because ARM is specified as target platform.")
    else()
        # Note that we removed rdrand.asm. This is a workaround for the issue that rdrand.asm cannnot compiled properly
        # on MSVC. Because there is also a rdrand.S file in the submodule, CMake will specify the target path for
        # rdrand.asm as "/crytopp.dir/{Debug|Release}/cryptopp/rdrand.asm.obj". The additional target folder "cryptopp"
        # is specified because the file rdrand.asm is in the source folder "cryptopp". But MSVC assembler can't build
        # target file to an non-existing folder("cryptopp").
        list(APPEND cryptopp_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/x64dll.asm)
        list(APPEND cryptopp_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/x64masm.asm)
        # list(APPEND cryptopp_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/rdrand.asm)
        set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/x64dll.asm PROPERTIES COMPILE_FLAGS "/D_M_X64")
        set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/x64masm.asm PROPERTIES COMPILE_FLAGS "/D_M_X64")
        # set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/rdrand.asm PROPERTIES COMPILE_FLAGS "/D_M_X64")
        enable_language(ASM_MASM)
    endif()
endif()

#============================================================================
# Compile targets
#============================================================================
add_library(cryptopp STATIC ${cryptopp_SOURCES})

#============================================================================
# Third-party libraries
#============================================================================

if(WIN32)
    target_link_libraries(cryptopp ws2_32)
endif()

find_package(Threads)
target_link_libraries(cryptopp ${CMAKE_THREAD_LIBS_INIT})

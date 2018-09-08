# The CMakeLists.txt shipped with cryptopp pollutes our option list and installation list,
# so we made our own one. This is basically a trimmed down version of the shipped CMakeLists.txt
# The differences are:
#  - removed support for legacy CMake versions
#  - removed support for 32-bit
#  - removed -march=native flag
#  - removed rdrand module.asm as a workaround for an issue (see below)
#  - added prefix "CRYPTOPP_" to all option names
#  - disabled testing
#  - disabled installation
#  - disabled documentation
#  - configured to build a static library only
#  - adds include directories to the library target

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

if(MSVC)
  # Disable C4390: empty controlled statement found: is this the intent?
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4390")
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

# Library sources.
# These have been trimmed to include only things Citra uses. This speeds up
# compiles and reduces the amount of compilation breakage.
set(cryptopp_SOURCES
        # The Crypto++ readme says you should put these 3 object files first,
        # to avoid "problems associated with C++ static initialization order",
        # but doesn't actually tell what could go wrong. Better safe than sorry
        # I guess...
        cryptopp/cryptlib.cpp
        cryptopp/cpu.cpp
        cryptopp/integer.cpp

        cryptopp/algparam.cpp
        cryptopp/asn.cpp
        cryptopp/authenc.cpp
        cryptopp/base64.cpp
        cryptopp/basecode.cpp
        cryptopp/ccm.cpp
        cryptopp/crc-simd.cpp
        cryptopp/des.cpp
        cryptopp/dessp.cpp
        cryptopp/dll.cpp
        cryptopp/ec2n.cpp
        cryptopp/ecp.cpp
        cryptopp/filters.cpp
        cryptopp/fips140.cpp
        cryptopp/gcm-simd.cpp
        cryptopp/gf2n.cpp
        cryptopp/gfpcrypt.cpp
        cryptopp/hex.cpp
        cryptopp/hmac.cpp
        cryptopp/hrtimer.cpp
        cryptopp/iterhash.cpp
        cryptopp/md5.cpp
        cryptopp/misc.cpp
        cryptopp/modes.cpp
        cryptopp/mqueue.cpp
        cryptopp/nbtheory.cpp
        cryptopp/neon-simd.cpp
        cryptopp/oaep.cpp
        cryptopp/osrng.cpp
        cryptopp/pubkey.cpp
        cryptopp/queue.cpp
        cryptopp/randpool.cpp
        cryptopp/rdtables.cpp
        cryptopp/rijndael-simd.cpp
        cryptopp/rijndael.cpp
        cryptopp/rng.cpp
        cryptopp/sha-simd.cpp
        cryptopp/sha.cpp
        cryptopp/sse-simd.cpp
        )

if (MINGW OR WIN32)
    list(APPEND cryptopp_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/winpipes.cpp)
endif ()

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

if ((CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU") AND NOT CRYPTOPP_DISABLE_ASM)
    check_cxx_compiler_flag(-msse2 CRYPTOPP_HAS_MSSE2)
    check_cxx_compiler_flag(-mssse3 CRYPTOPP_HAS_MSSSE3)
    check_cxx_compiler_flag(-msse4.1 CRYPTOPP_HAS_MSSE41)
    check_cxx_compiler_flag(-msse4.2 CRYPTOPP_HAS_MSSE42)
    check_cxx_compiler_flag(-maes CRYPTOPP_HAS_MAES)
    check_cxx_compiler_flag(-mpclmul CRYPTOPP_HAS_PCLMUL)
    check_cxx_compiler_flag(-msha CRYPTOPP_HAS_MSHA)
    check_cxx_compiler_flag(-march=armv8-a+crc CRYPTOPP_HAS_ARMV8_CRC32)
    check_cxx_compiler_flag(-march=armv8-a+crypto CRYPTOPP_HAS_ARMV8_CRYPTO)
    if (CRYPTOPP_HAS_MSSE2)
        set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/sse-simd.cpp PROPERTIES COMPILE_FLAGS "-msse2")
    endif()
    if (CRYPTOPP_HAS_MSSSE3 AND CRYPTOPP_HAS_MAES AND CRYPTOPP_HAS_PCLMUL)
        set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/gcm-simd.cpp
                                    PROPERTIES COMPILE_FLAGS "-mssse3 -maes -mpclmul")
    endif()
    if (CRYPTOPP_HAS_MSSE41 AND CRYPTOPP_HAS_MAES)
        set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/rijndael-simd.cpp PROPERTIES COMPILE_FLAGS "-msse4.1 -maes")
    endif()
    if (CRYPTOPP_HAS_MSSE42)
        set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/crc-simd.cpp
                                    PROPERTIES COMPILE_FLAGS "-msse4.2")
    endif()
    if (CRYPTOPP_HAS_MSSE42 AND CRYPTOPP_HAS_MSHA)
        set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/sha-simd.cpp PROPERTIES COMPILE_FLAGS "-msse4.2 -msha")
    endif()
    if (CRYPTOPP_HAS_ARMV8_CRC32)
        set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/crc-simd.cpp
                                    PROPERTIES COMPILE_FLAGS "-march=armv8-a+crc")
    endif()
    if (CRYPTOPP_HAS_ARMV8_CRYPTO)
        set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/rijndael-simd.cpp
                                    ${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/sha-simd.cpp
                                    PROPERTIES COMPILE_FLAGS "-march=armv8-a+crypto")
    endif()
endif()

#============================================================================
# Compile targets
#============================================================================
add_library(cryptopp STATIC ${cryptopp_SOURCES})
target_include_directories(cryptopp INTERFACE .)

#============================================================================
# Third-party libraries
#============================================================================

find_package(Threads)
target_link_libraries(cryptopp PRIVATE ${CMAKE_THREAD_LIBS_INIT})

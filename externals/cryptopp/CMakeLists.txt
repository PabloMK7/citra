# A trimmed down version of the CMakeLists.txt from noloader/cryptopp-cmake
# The differences are:
#  - removed support for legacy CMake versions
#  - removed support for 32-bit
#  - added prefix "CRYPTOPP_OPT_" to all option names
#  - disabled testing
#  - disabled installation
#  - disabled documentation
#  - configured to build a static library only
#  - adds include directories to the library target

cmake_minimum_required(VERSION 3.1)
if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif ()
project(cryptopp VERSION 8.5.0)
if (POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif ()

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cryptopp)

include(TestBigEndian)
include(GNUInstallDirs)
include(CheckCXXCompilerFlag)

set(TEST_PROG_DIR ${SRC_DIR}/TestPrograms)
set(TEST_CXX_FILE ${TEST_PROG_DIR}/test_cxx.cxx)

#============================================================================
# Settable options
#============================================================================

option(CRYPTOPP_OPT_DISABLE_ASM "Disable ASM" OFF)
option(CRYPTOPP_OPT_DISABLE_SSSE3 "Disable SSSE3" OFF)
option(CRYPTOPP_OPT_DISABLE_SSE4 "Disable SSE4" OFF)
option(CRYPTOPP_OPT_DISABLE_AESNI "Disable AES-NI" OFF)
option(CRYPTOPP_OPT_DISABLE_SHA "Disable SHA" OFF)
option(CRYPTOPP_OPT_DISABLE_AVX "Disable AVX" OFF)
option(CRYPTOPP_OPT_DISABLE_AVX2 "Disable AVX2" OFF)

#============================================================================
# Compiler options
#============================================================================

# Only set when cross-compiling, http://www.vtk.org/Wiki/CMake_Cross_Compiling
if (NOT (CMAKE_SYSTEM_VERSION AND CMAKE_SYSTEM_PROCESSOR))
    set(CRYPTOPP_CROSS_COMPILE 1)
else()
    set(CRYPTOPP_CROSS_COMPILE 0)
endif()

set(CRYPTOPP_COMPILE_DEFINITIONS)
set(CRYPTOPP_COMPILE_OPTIONS)

# Don't use RPATH's. The resulting binary could fail a security audit.
set(CMAKE_MACOSX_RPATH 0)

if(CMAKE_CXX_COMPILER_ID MATCHES "Intel")
  list(APPEND CRYPTOPP_COMPILE_OPTIONS -wd68 -wd186 -wd279 -wd327 -wd161 -wd3180)
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

if (CRYPTOPP_OPT_DISABLE_ASM)
  list(APPEND CRYPTOPP_COMPILE_DEFINITIONS CRYPTOPP_DISABLE_ASM)
endif ()
if (CRYPTOPP_OPT_DISABLE_SSSE3)
  list(APPEND CRYPTOPP_COMPILE_DEFINITIONS CRYPTOPP_DISABLE_SSSE3)
endif ()
if (CRYPTOPP_OPT_DISABLE_SSE4)
  list(APPEND CRYPTOPP_COMPILE_DEFINITIONS CRYPTOPP_DISABLE_SSSE4)
endif ()
if (CRYPTOPP_OPT_DISABLE_CLMUL)
  list(APPEND CRYPTOPP_COMPILE_DEFINITIONS CRYPTOPP_DISABLE_CLMUL)
endif ()
if (CRYPTOPP_OPT_DISABLE_AESNI)
  list(APPEND CRYPTOPP_COMPILE_DEFINITIONS CRYPTOPP_DISABLE_AESNI)
endif ()
if (CRYPTOPP_OPT_DISABLE_RDRAND)
  list(APPEND CRYPTOPP_COMPILE_DEFINITIONS CRYPTOPP_DISABLE_RDRAND)
endif ()
if (CRYPTOPP_OPT_DISABLE_RDSEED)
  list(APPEND CRYPTOPP_COMPILE_DEFINITIONS CRYPTOPP_DISABLE_RDSEED)
endif ()
if (CRYPTOPP_OPT_DISABLE_AVX)
  list(APPEND CRYPTOPP_COMPILE_DEFINITIONS CRYPTOPP_DISABLE_AVX)
endif ()
if (CRYPTOPP_OPT_DISABLE_AVX2)
  list(APPEND CRYPTOPP_COMPILE_DEFINITIONS CRYPTOPP_DISABLE_AVX2)
endif ()
if (CRYPTOPP_OPT_DISABLE_SHA)
  list(APPEND CRYPTOPP_COMPILE_DEFINITIONS CRYPTOPP_DISABLE_SHA)
endif ()

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

###############################################################################

# Try to find a Posix compatible grep and sed. Solaris, Digital Unix,
#   Tru64, HP-UX and a few others need tweaking

if (EXISTS /usr/xpg4/bin/grep)
  set(GREP_CMD /usr/xpg4/bin/grep)
elseif (EXISTS /usr/gnu/bin/grep)
  set(GREP_CMD /usr/gnu/bin/grep)
elseif (EXISTS /usr/linux/bin/grep)
  set(GREP_CMD /usr/linux/bin/grep)
else ()
  set(GREP_CMD grep)
endif ()

if (EXISTS /usr/xpg4/bin/sed)
  set(SED_CMD /usr/xpg4/bin/sed)
elseif (EXISTS /usr/gnu/bin/sed)
  set(SED_CMD /usr/gnu/bin/sed)
elseif (EXISTS /usr/linux/bin/sed)
  set(SED_CMD /usr/linux/bin/sed)
else ()
  set(SED_CMD sed)
endif ()

###############################################################################

function(CheckCompileOption opt var)
    CHECK_CXX_COMPILER_FLAG(${opt} ${var})
endfunction(CheckCompileOption)

function(CheckCompileLinkOption opt var prog)

  if (MSVC)

    # TODO: improve this...
    CHECK_CXX_COMPILER_FLAG(${opt} ${var})

  elseif (APPLE)

    message(STATUS "Performing Test ${var}")
    try_compile(COMMAND_SUCCESS ${CMAKE_BINARY_DIR} ${prog} COMPILE_DEFINITIONS ${opt})
    if (COMMAND_SUCCESS)
      set(${var} 1 PARENT_SCOPE)
      message(STATUS "Performing Test ${var} - Success")
    else ()
      set(${var} 0 PARENT_SCOPE)
      message(STATUS "Performing Test ${var} - Failed")
    endif ()

  else ()

    message(STATUS "Performing Test ${var}")
    try_compile(COMMAND_SUCCESS ${CMAKE_BINARY_DIR} ${prog} COMPILE_DEFINITIONS ${opt})
    if (COMMAND_SUCCESS)
        set(${var} 1 PARENT_SCOPE)
        message(STATUS "Performing Test ${var} - Success")
    else ()
      set(${var} 0 PARENT_SCOPE)
      message(STATUS "Performing Test ${var} - Failed")
    endif ()

  endif ()

endfunction(CheckCompileLinkOption)

function(AddCompileOption opt)

    if ("${COMMAND_OUTPUT}" NOT STREQUAL "")
      list(APPEND CRYPTOPP_COMPILE_OPTIONS "${opt}")
    endif ()

endfunction(AddCompileOption)

###############################################################################

function(DumpMachine output pattern)

  if (MSVC)

    # CMake does not provide a generic shell/terminal mechanism
    #  and Microsoft environments don't know what 'sh' is.
    set(${output} 0 PARENT_SCOPE)

  else ()
      if(CMAKE_SYSTEM_PROCESSOR MATCHES ${pattern})
          set(${output} TRUE PARENT_SCOPE)
      endif()
  endif()

endfunction(DumpMachine)

# Thansk to Anonimal for MinGW; see http://github.com/weidai11/cryptopp/issues/466
DumpMachine(CRYPTOPP_AMD64 "(x86_64|AMD64|amd64)")
DumpMachine(CRYPTOPP_I386 "(i.86)")
DumpMachine(CRYPTOPP_MINGW64 "(w64-mingw32)|(mingw64)")
DumpMachine(CRYPTOPP_ARMV8 "(armv8|aarch64)")

###############################################################################

if(MSVC)
  if(CMAKE_SYSTEM_VERSION MATCHES "10\\.0.*")
    list(APPEND CRYPTOPP_COMPILE_DEFINITIONS "_WIN32_WINNT=0x0A00")
  endif()
  list(APPEND CRYPTOPP_COMPILE_OPTIONS /FI winapifamily.h)
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
file(GLOB cryptopp_HEADERS ${SRC_DIR}/*.h)

# Remove headers used to build test suite
list(REMOVE_ITEM cryptopp_HEADERS
    ${SRC_DIR}/bench.h
    ${SRC_DIR}/validate.h
    )

# Library sources.
# These have been trimmed to include only things Citra uses. This speeds up
# compiles and reduces the amount of compilation breakage.
set(cryptopp_SOURCES
        # The Crypto++ readme says you should put these 3 object files first,
        # to avoid "problems associated with C++ static initialization order",
        # but doesn't actually tell what could go wrong. Better safe than sorry
        # I guess...
        ${SRC_DIR}/cryptlib.cpp
        ${SRC_DIR}/cpu.cpp
        ${SRC_DIR}/integer.cpp

        ${SRC_DIR}/algparam.cpp
        ${SRC_DIR}/allocate.cpp
        ${SRC_DIR}/asn.cpp
        ${SRC_DIR}/authenc.cpp
        ${SRC_DIR}/base64.cpp
        ${SRC_DIR}/basecode.cpp
        ${SRC_DIR}/ccm.cpp
        ${SRC_DIR}/crc_simd.cpp
        ${SRC_DIR}/des.cpp
        ${SRC_DIR}/dessp.cpp
        ${SRC_DIR}/dll.cpp
        ${SRC_DIR}/ec2n.cpp
        ${SRC_DIR}/ecp.cpp
        ${SRC_DIR}/filters.cpp
        ${SRC_DIR}/fips140.cpp
        ${SRC_DIR}/gcm_simd.cpp
        ${SRC_DIR}/gf2n_simd.cpp
        ${SRC_DIR}/gf2n.cpp
        ${SRC_DIR}/gfpcrypt.cpp
        ${SRC_DIR}/hex.cpp
        ${SRC_DIR}/hmac.cpp
        ${SRC_DIR}/hrtimer.cpp
        ${SRC_DIR}/iterhash.cpp
        ${SRC_DIR}/md5.cpp
        ${SRC_DIR}/misc.cpp
        ${SRC_DIR}/modes.cpp
        ${SRC_DIR}/mqueue.cpp
        ${SRC_DIR}/nbtheory.cpp
        ${SRC_DIR}/neon_simd.cpp
        ${SRC_DIR}/oaep.cpp
        ${SRC_DIR}/osrng.cpp
        ${SRC_DIR}/ppc_power7.cpp
        ${SRC_DIR}/ppc_power8.cpp
        ${SRC_DIR}/ppc_power9.cpp
        ${SRC_DIR}/ppc_simd.cpp
        ${SRC_DIR}/pubkey.cpp
        ${SRC_DIR}/queue.cpp
        ${SRC_DIR}/randpool.cpp
        ${SRC_DIR}/rdtables.cpp
        ${SRC_DIR}/rijndael_simd.cpp
        ${SRC_DIR}/rijndael.cpp
        ${SRC_DIR}/rng.cpp
        ${SRC_DIR}/sha_simd.cpp
        ${SRC_DIR}/sha.cpp
        ${SRC_DIR}/sse_simd.cpp
        )

if(ANDROID)
    include_directories(${ANDROID_NDK}/sources/android/cpufeatures)
    list(APPEND cryptopp_SOURCES ${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c)
endif()

set(cryptopp_SOURCES_ASM)

if (MSVC AND NOT DISABLE_ASM)
  if (${CMAKE_GENERATOR} MATCHES ".*ARM")
    message(STATUS "Disabling ASM because ARM is specified as target platform.")
  else ()
    enable_language(ASM_MASM)
    list(APPEND cryptopp_SOURCES_ASM
      ${SRC_DIR}/rdrand.asm
      ${SRC_DIR}/rdseed.asm
      )
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
      list(APPEND cryptopp_SOURCES_ASM
        ${SRC_DIR}/x64dll.asm
        ${SRC_DIR}/x64masm.asm
        )
      set_source_files_properties(${cryptopp_SOURCES_ASM} PROPERTIES COMPILE_DEFINITIONS "_M_X64")
    else ()
      set_source_files_properties(${cryptopp_SOURCES_ASM} PROPERTIES COMPILE_DEFINITIONS "_M_X86" COMPILE_FLAGS "/safeseh")
    endif ()
    set_source_files_properties(${cryptopp_SOURCES_ASM} PROPERTIES LANGUAGE ASM_MASM)
  endif ()
endif ()

#============================================================================
# Architecture flags
#============================================================================

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Intel" OR CMAKE_CXX_COMPILER MATCHES "xlC")

  if (CRYPTOPP_AMD64 OR CRYPTOPP_I386)
    CheckCompileLinkOption("-msse2" CRYPTOPP_IA32_SSE2
                           "${TEST_PROG_DIR}/test_x86_sse2.cxx")
    CheckCompileLinkOption("-mssse3" CRYPTOPP_IA32_SSSE3
                           "${TEST_PROG_DIR}/test_x86_ssse3.cxx")
    CheckCompileLinkOption("-msse4.1" CRYPTOPP_IA32_SSE41
                           "${TEST_PROG_DIR}/test_x86_sse41.cxx")
    CheckCompileLinkOption("-msse4.2" CRYPTOPP_IA32_SSE42
                           "${TEST_PROG_DIR}/test_x86_sse42.cxx")
    CheckCompileLinkOption("-mssse3 -mpclmul" CRYPTOPP_IA32_CLMUL
                           "${TEST_PROG_DIR}/test_x86_clmul.cxx")
    CheckCompileLinkOption("-msse4.1 -maes" CRYPTOPP_IA32_AES
                           "${TEST_PROG_DIR}/test_x86_aes.cxx")
    CheckCompileLinkOption("-mavx" CRYPTOPP_IA32_AVX
                           "${TEST_PROG_DIR}/test_x86_avx.cxx")
    CheckCompileLinkOption("-mavx2" CRYPTOPP_IA32_AVX2
                           "${TEST_PROG_DIR}/test_x86_avx2.cxx")
    CheckCompileLinkOption("-msse4.2 -msha" CRYPTOPP_IA32_SHA
                           "${TEST_PROG_DIR}/test_x86_sha.cxx")
    if (EXISTS "${TEST_PROG_DIR}/test_asm_mixed.cxx")
      CheckCompileLinkOption("" CRYPTOPP_MIXED_ASM
                             "${TEST_PROG_DIR}/test_asm_mixed.cxx")
    else ()
      CheckCompileLinkOption("" CRYPTOPP_MIXED_ASM
                             "${TEST_PROG_DIR}/test_mixed_asm.cxx")
    endif ()

    if (NOT CRYPTOPP_MIXED_ASM)
      list(APPEND CRYPTOPP_COMPILE_OPTIONS "-DCRYPTOPP_DISABLE_MIXED_ASM")
    endif ()

    if (NOT CRYPTOPP_IA32_SSE2 AND NOT CRYPTOPP_DISABLE_ASM)
      list(APPEND CRYPTOPP_COMPILE_OPTIONS "-DCRYPTOPP_DISABLE_ASM")
    elseif (CRYPTOPP_IA32_SSE2 AND NOT CRYPTOPP_DISABLE_ASM)
      set_source_files_properties(${SRC_DIR}/sse_simd.cpp PROPERTIES COMPILE_FLAGS "-msse2")
    endif ()
    if (NOT CRYPTOPP_IA32_SSSE3 AND NOT CRYPTOPP_DISABLE_SSSE3)
      list(APPEND CRYPTOPP_COMPILE_OPTIONS "-DCRYPTOPP_DISABLE_SSSE3")
    elseif (CRYPTOPP_IA32_SSSE3 AND NOT CRYPTOPP_DISABLE_SSSE3)
      if (NOT CRYPTOPP_IA32_SSE41 AND NOT CRYPTOPP_DISABLE_SSE4)
        list(APPEND CRYPTOPP_COMPILE_OPTIONS "-DCRYPTOPP_DISABLE_SSE4")
      endif ()
      if (NOT CRYPTOPP_IA32_SSE42 AND NOT CRYPTOPP_DISABLE_SSE4)
        list(APPEND CRYPTOPP_COMPILE_OPTIONS "-DCRYPTOPP_DISABLE_SSE4")
      elseif (CRYPTOPP_IA32_SSE42 AND NOT CRYPTOPP_DISABLE_SSE4)
        set_source_files_properties(${SRC_DIR}/crc_simd.cpp PROPERTIES COMPILE_FLAGS "-msse4.2")
        if (NOT CRYPTOPP_IA32_CLMUL AND NOT CRYPTOPP_DISABLE_CLMUL)
          list(APPEND CRYPTOPP_COMPILE_OPTIONS "-DCRYPTOPP_DISABLE_CLMUL")
        elseif (CRYPTOPP_IA32_CLMUL AND NOT CRYPTOPP_DISABLE_CLMUL)
          set_source_files_properties(${SRC_DIR}/gcm_simd.cpp PROPERTIES COMPILE_FLAGS "-mssse3 -mpclmul")
          set_source_files_properties(${SRC_DIR}/gf2n_simd.cpp PROPERTIES COMPILE_FLAGS "-mpclmul")
        endif ()
        if (NOT CRYPTOPP_IA32_AES AND NOT CRYPTOPP_DISABLE_AES)
          list(APPEND CRYPTOPP_COMPILE_OPTIONS "-DCRYPTOPP_DISABLE_AESNI")
        elseif (CRYPTOPP_IA32_AES AND NOT CRYPTOPP_DISABLE_AES)
          set_source_files_properties(${SRC_DIR}/rijndael_simd.cpp PROPERTIES COMPILE_FLAGS "-msse4.1 -maes")
        endif ()
        if (NOT CRYPTOPP_IA32_AVX2 AND NOT CRYPTOPP_DISABLE_AVX2)
          list(APPEND CRYPTOPP_COMPILE_OPTIONS "-DCRYPTOPP_DISABLE_AVX2")
        endif ()
        if (NOT CRYPTOPP_IA32_SHA AND NOT CRYPTOPP_DISABLE_SHA)
          list(APPEND CRYPTOPP_COMPILE_OPTIONS "-DCRYPTOPP_DISABLE_SHANI")
        elseif (CRYPTOPP_IA32_SHA AND NOT CRYPTOPP_DISABLE_SHA)
          set_source_files_properties(${SRC_DIR}/sha_simd.cpp PROPERTIES COMPILE_FLAGS "-msse4.2 -msha")
        endif ()
      endif ()
    endif ()
  endif ()
endif ()

#============================================================================
# Compile targets
#============================================================================

set(cryptopp_LIBRARY_SOURCES ${cryptopp_SOURCES_ASM})
list(APPEND cryptopp_LIBRARY_SOURCES ${cryptopp_SOURCES})

add_library(cryptopp STATIC ${cryptopp_LIBRARY_SOURCES})
target_compile_definitions(cryptopp PUBLIC ${CRYPTOPP_COMPILE_DEFINITIONS})
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}")
target_include_directories(cryptopp INTERFACE .)

#============================================================================
# Third-party libraries
#============================================================================

find_package(Threads)
target_link_libraries(cryptopp PRIVATE ${CMAKE_THREAD_LIBS_INIT})

if(ANDROID)
    include(AndroidNdkModules)
    android_ndk_import_module_cpufeatures()
    target_link_libraries(cryptopp PRIVATE cpufeatures)
endif()

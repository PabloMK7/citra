# Sources cut down to just what we need for AAC-LC.
set(FAAD2_SOURCE_DIR "faad2/libfaad")
add_library(faad2 STATIC EXCLUDE_FROM_ALL
    "${FAAD2_SOURCE_DIR}/bits.c"
    "${FAAD2_SOURCE_DIR}/cfft.c"
    "${FAAD2_SOURCE_DIR}/common.c"
    "${FAAD2_SOURCE_DIR}/decoder.c"
    "${FAAD2_SOURCE_DIR}/drc.c"
    "${FAAD2_SOURCE_DIR}/error.c"
    "${FAAD2_SOURCE_DIR}/filtbank.c"
    "${FAAD2_SOURCE_DIR}/huffman.c"
    "${FAAD2_SOURCE_DIR}/is.c"
    "${FAAD2_SOURCE_DIR}/mdct.c"
    "${FAAD2_SOURCE_DIR}/mp4.c"
    "${FAAD2_SOURCE_DIR}/ms.c"
    "${FAAD2_SOURCE_DIR}/output.c"
    "${FAAD2_SOURCE_DIR}/pns.c"
    "${FAAD2_SOURCE_DIR}/pulse.c"
    "${FAAD2_SOURCE_DIR}/specrec.c"
    "${FAAD2_SOURCE_DIR}/syntax.c"
    "${FAAD2_SOURCE_DIR}/tns.c"
)
target_include_directories(faad2 PUBLIC faad2/include PRIVATE "${FAAD2_SOURCE_DIR}")

# Configure compile definitions.

# Read version from properties file for configuring constant.
file(READ faad2/properties.json FAAD_PROPERTIES_JSON)
string(JSON FAAD_VERSION GET ${FAAD_PROPERTIES_JSON} PACKAGE_VERSION)
message(STATUS "Building faad2 version ${FAAD_VERSION}")

# Check for functions and headers.
include(CheckFunctionExists)
include(CheckIncludeFiles)
check_function_exists(getpwuid HAVE_GETPWUID)
check_function_exists(lrintf HAVE_LRINTF)
check_function_exists(memcpy HAVE_MEMCPY)
check_function_exists(strchr HAVE_STRCHR)
check_function_exists(strsep HAVE_STRSEP)
check_include_files(dlfcn.h HAVE_DLFCN_H)
check_include_files(errno.h HAVE_ERRNO_H)
check_include_files(float.h HAVE_FLOAT_H)
check_include_files(inttypes.h HAVE_INTTYPES_H)
check_include_files(IOKit/IOKitLib.h HAVE_IOKIT_IOKITLIB_H)
check_include_files(limits.h HAVE_LIMITS_H)
check_include_files(mathf.h HAVE_MATHF_H)
check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(stdio.h HAVE_STDIO_H)
check_include_files(stdlib.h HAVE_STDLIB_H)
check_include_files(strings.h HAVE_STRINGS_H)
check_include_files(string.h HAVE_STRING_H)
check_include_files(sysfs/libsysfs.h HAVE_SYSFS_LIBSYSFS_H)
check_include_files(sys/stat.h HAVE_SYS_STAT_H)
check_include_files(sys/time.h HAVE_SYS_TIME_H)
check_include_files(sys/types.h HAVE_SYS_TYPES_H)
check_include_files(unistd.h HAVE_UNISTD_H)

# faad2 uses a relative include for its config.h which breaks under CMake.
# We can use target_compile_definitions to pass on the configuration instead.
target_compile_definitions(faad2 PRIVATE
    -DFAAD_VERSION=${FAAD_VERSION}
    -DPACKAGE_VERSION=\"${FAAD_VERSION}\"
    -DSTDC_HEADERS
    -DHAVE_GETPWUID=${HAVE_GETPWUID}
    -DHAVE_LRINTF=${HAVE_LRINTF}
    -DHAVE_MEMCPY=${HAVE_MEMCPY}
    -DHAVE_STRCHR=${HAVE_STRCHR}
    -DHAVE_STRSEP=${HAVE_STRSEP}
    -DHAVE_DLFCN_H=${HAVE_DLFCN_H}
    -DHAVE_ERRNO_H=${HAVE_ERRNO_H}
    -DHAVE_FLOAT_H=${HAVE_FLOAT_H}
    -DHAVE_INTTYPES_H=${HAVE_INTTYPES_H}
    -DHAVE_IOKIT_IOKITLIB_H=${HAVE_IOKIT_IOKITLIB_H}
    -DHAVE_LIMITS_H=${HAVE_LIMITS_H}
    -DHAVE_MATHF_H=${HAVE_MATHF_H}
    -DHAVE_STDINT_H=${HAVE_STDINT_H}
    -DHAVE_STDIO_H=${HAVE_STDIO_H}
    -DHAVE_STDLIB_H=${HAVE_STDLIB_H}
    -DHAVE_STRINGS_H=${HAVE_STRINGS_H}
    -DHAVE_STRING_H=${HAVE_STRING_H}
    -DHAVE_SYSFS_LIBSYSFS_H=${HAVE_SYSFS_LIBSYSFS_H}
    -DHAVE_SYS_STAT_H=${HAVE_SYS_STAT_H}
    -DHAVE_SYS_TIME_H=${HAVE_SYS_TIME_H}
    -DHAVE_SYS_TYPES_H=${HAVE_SYS_TYPES_H}
    -DHAVE_UNISTD_H=${HAVE_UNISTD_H}
    # Only compile for AAC-LC decoding.
    -DLC_ONLY_DECODER -DDISABLE_SBR
)

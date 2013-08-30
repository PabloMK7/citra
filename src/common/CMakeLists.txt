set(SRCS    src/config.cpp
            src/crc.cpp
            src/file_utils.cpp
            src/hash.cpp
            src/log.cpp
            src/misc_utils.cpp
            src/timer.cpp
            src/x86_utils.cpp
            src/xml.cpp)

add_library(common STATIC ${SRCS})

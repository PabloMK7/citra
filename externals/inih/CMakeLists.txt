set(SRCS
            inih/ini.c
            inih/cpp/INIReader.cpp
            )
set(HEADERS
            inih/ini.h
            inih/cpp/INIReader.h
            )

create_directory_groups(${SRCS} ${HEADERS})
add_library(inih ${SRCS} ${HEADERS})
target_include_directories(inih INTERFACE .)

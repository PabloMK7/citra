add_library(ifaddrs
    ifaddrs.c
    ifaddrs.h
)

create_target_directory_groups(ifaddrs)

target_include_directories(ifaddrs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

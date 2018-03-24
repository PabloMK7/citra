add_library(lurlparser
        LUrlParser.cpp
        LUrlParser.h
)

create_target_directory_groups(lurlparser)

target_include_directories(lurlparser INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

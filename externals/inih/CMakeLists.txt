add_library(inih
    inih/ini.c
    inih/ini.h
    inih/cpp/INIReader.cpp
    inih/cpp/INIReader.h
)

create_target_directory_groups(inih)
target_include_directories(inih INTERFACE .)

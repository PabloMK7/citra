add_library(lodepng
    lodepng.cpp
    lodepng.h
)

create_target_directory_groups(lodepng)
target_include_directories(lodepng INTERFACE .)

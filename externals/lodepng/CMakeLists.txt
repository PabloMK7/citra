add_library(lodepng
    lodepng/lodepng.cpp
    lodepng/lodepng.h
)

create_target_directory_groups(lodepng)
target_include_directories(lodepng INTERFACE lodepng)

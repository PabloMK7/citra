cmake_minimum_required(VERSION 3.8)

add_library(citra-android STATIC
            src/main/cpp/dummy.cpp
            )

# find Android's log library
find_library(log-lib log)

target_link_libraries(citra-android ${log-lib} core common inih)

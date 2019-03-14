cmake_minimum_required(VERSION 3.8)

add_library(citra-android SHARED
            logging/log.cpp
            logging/logcat_backend.cpp
            logging/logcat_backend.h
            native_interface.cpp
            native_interface.h
            ui/main/main_activity.cpp
            )

# find Android's log library
find_library(log-lib log)

target_link_libraries(citra-android ${log-lib} core common inih)
target_include_directories(citra-android PRIVATE "../../../../../" "./")

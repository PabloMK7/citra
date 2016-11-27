set(SRCS
            glad.cpp
            tests.cpp
            core/file_sys/path_parser.cpp
            )

set(HEADERS
            )

create_directory_groups(${SRCS} ${HEADERS})

include_directories(../../externals/catch/single_include/)

add_executable(tests ${SRCS} ${HEADERS})
target_link_libraries(tests core video_core audio_core common)
target_link_libraries(tests ${PLATFORM_LIBRARIES})

add_test(NAME tests COMMAND $<TARGET_FILE:tests>)

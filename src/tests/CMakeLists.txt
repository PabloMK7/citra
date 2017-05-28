set(SRCS
            glad.cpp
            tests.cpp
            common/param_package.cpp
            core/file_sys/path_parser.cpp
            )

set(HEADERS
            )

create_directory_groups(${SRCS} ${HEADERS})

include_directories(../../externals/catch/single_include/)

add_executable(tests ${SRCS} ${HEADERS})
target_link_libraries(tests PRIVATE common core)
target_link_libraries(tests PRIVATE glad) # To support linker work-around
target_link_libraries(tests PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)

add_test(NAME tests COMMAND $<TARGET_FILE:tests>)

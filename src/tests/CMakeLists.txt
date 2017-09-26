set(SRCS
            common/param_package.cpp
            core/arm/arm_test_common.cpp
            core/arm/dyncom/arm_dyncom_vfp_tests.cpp
            core/file_sys/path_parser.cpp
            core/hle/kernel/hle_ipc.cpp
            core/memory/memory.cpp
            glad.cpp
            tests.cpp
            )

set(HEADERS
            core/arm/arm_test_common.h
            )

create_directory_groups(${SRCS} ${HEADERS})

add_executable(tests ${SRCS} ${HEADERS})
target_link_libraries(tests PRIVATE common core)
target_link_libraries(tests PRIVATE glad) # To support linker work-around
target_link_libraries(tests PRIVATE ${PLATFORM_LIBRARIES} catch-single-include Threads::Threads)

add_test(NAME tests COMMAND tests)

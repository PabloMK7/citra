set(SRCS
            src/glad.c
            )
set(HEADERS
            include/KHR/khrplatform.h
            include/glad/glad.h
            )

create_directory_groups(${SRCS} ${HEADERS})
add_library(glad STATIC ${SRCS} ${HEADERS})
target_include_directories(glad PUBLIC "include/")

if ("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
    target_link_libraries(glad PRIVATE dl)
endif()

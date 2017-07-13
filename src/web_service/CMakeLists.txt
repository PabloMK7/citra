set(SRCS
            telemetry_json.cpp
            web_backend.cpp
            )

set(HEADERS
            telemetry_json.h
            web_backend.h
            )

create_directory_groups(${SRCS} ${HEADERS})

add_library(web_service STATIC ${SRCS} ${HEADERS})
target_link_libraries(web_service PUBLIC common cpr json-headers)

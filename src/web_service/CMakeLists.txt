add_library(web_service STATIC
    announce_room_json.cpp
    announce_room_json.h
    telemetry_json.cpp
    telemetry_json.h
    verify_login.cpp
    verify_login.h
    web_backend.cpp
    web_backend.h
)

create_target_directory_groups(web_service)

target_link_libraries(web_service PUBLIC common cpr json-headers)

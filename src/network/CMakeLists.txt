add_library(network STATIC
    announce_multiplayer_session.cpp
    announce_multiplayer_session.h
    network.cpp
    network.h
    network_settings.cpp
    network_settings.h
    packet.cpp
    packet.h
    precompiled_headers.h
    room.cpp
    room.h
    room_member.cpp
    room_member.h
    verify_user.cpp
    verify_user.h
)

create_target_directory_groups(network)

if (ENABLE_WEB_SERVICE)
    target_compile_definitions(network PRIVATE -DENABLE_WEB_SERVICE -DCPPHTTPLIB_OPENSSL_SUPPORT)
    target_link_libraries(network PRIVATE web_service httplib)
    if (ANDROID)
        target_link_libraries(network PRIVATE ifaddrs)
    endif()
endif()

target_link_libraries(network PRIVATE citra_common enet Boost::serialization)
set_target_properties(network PROPERTIES INTERPROCEDURAL_OPTIMIZATION ${ENABLE_LTO})

if (CITRA_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(network PRIVATE precompiled_headers.h)
endif()

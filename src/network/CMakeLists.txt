add_library(network STATIC
    network.cpp
    network.h
    packet.cpp
    packet.h
    room.cpp
    room.h
    room_member.cpp
    room_member.h
    verify_user.cpp
    verify_user.h
)

create_target_directory_groups(network)

target_link_libraries(network PRIVATE common enet Boost::serialization)

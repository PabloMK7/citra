set(SRCS
            network.cpp
            packet.cpp
            room.cpp
            room_member.cpp
            )

set(HEADERS
            network.h
            packet.h
            room.h
            room_member.h
            )

create_directory_groups(${SRCS} ${HEADERS})

add_library(network STATIC ${SRCS} ${HEADERS})
target_link_libraries(network PRIVATE common enet)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(SRCS
            commands.cpp
            qhexedit.cpp
            qhexedit_p.cpp
            xbytearray.cpp
            )

set(HEADERS
            commands.h
            qhexedit.h
            qhexedit_p.h
            xbytearray.h
            )

create_directory_groups(${SRCS} ${HEADERS})

add_library(qhexedit STATIC ${SRCS} ${HEADERS})
target_link_libraries(qhexedit ${CITRA_QT_LIBS})

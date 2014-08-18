set(CMAKE_AUTOMOC ON)

set(SRCS
            commands.cpp
            qhexedit.cpp
            qhexedit_p.cpp
            xbytearray.cpp)

set(HEADERS
            qhexedit.h
            qhexedit_p.h)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library(qhexedit STATIC ${SRCS} ${HEADERS})

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(SRCS
            commands.cpp
            qhexedit.cpp
            qhexedit_p.cpp
            xbytearray.cpp)

set(HEADERS
            qhexedit.h
            qhexedit_p.h)

add_library(qhexedit STATIC ${SRCS} ${HEADERS})
if(USE_QT5)
    target_link_libraries(qhexedit Qt5::Core Qt5::Widgets)
endif()


set(SRCS
            commands.cpp
            qhexedit.cpp
            qhexedit_p.cpp
            xbytearray.cpp)

qt4_wrap_cpp(MOC_SRCS
                        qhexedit.h
                        qhexedit_p.h)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library(qhexedit STATIC ${SRCS} ${MOC_SRCS})

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(SRCS
            config/controller_config.cpp
            config/controller_config_util.cpp
            debugger/callstack.cpp
            debugger/disassembler.cpp
            debugger/graphics.cpp
            debugger/graphics_cmdlists.cpp
            debugger/ramview.cpp
            debugger/registers.cpp
            bootmanager.cpp
            hotkeys.cpp
            main.cpp
            )

set(HEADERS
            config/controller_config.hxx
            config/controller_config_util.hxx
            debugger/callstack.hxx
            debugger/disassembler.hxx
            debugger/graphics.hxx
            debugger/graphics_cmdlists.hxx
            debugger/ramview.hxx
            debugger/registers.hxx
            bootmanager.hxx
            hotkeys.hxx
            main.hxx
            version.h
            )

set(UIS
            config/controller_config.ui
            debugger/callstack.ui
            debugger/disassembler.ui
            debugger/registers.ui
            hotkeys.ui
            main.ui
            )

create_directory_groups(${SRCS} ${HEADERS} ${UIS})

if (Qt5_FOUND)
    qt5_wrap_ui(UI_HDRS ${UIS})
else()
    qt4_wrap_ui(UI_HDRS ${UIS})
endif()

add_executable(citra-qt ${SRCS} ${HEADERS} ${UI_HDRS})
target_link_libraries(citra-qt core common video_core qhexedit)
target_link_libraries(citra-qt ${OPENGL_gl_LIBRARY} ${CITRA_QT_LIBS})

if (APPLE)
    target_link_libraries(citra-qt iconv ${COREFOUNDATION_LIBRARY})
elseif (WIN32)
    target_link_libraries(citra-qt winmm)
else() # Unix
    target_link_libraries(citra-qt rt)
endif()

#install(TARGETS citra-qt RUNTIME DESTINATION ${bindir})

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(SRCS
            config/controller_config.cpp
            config/controller_config_util.cpp
            config.cpp
            debugger/callstack.cpp
            debugger/disassembler.cpp
            debugger/graphics.cpp
            debugger/graphics_breakpoints.cpp
            debugger/graphics_cmdlists.cpp
            debugger/graphics_framebuffer.cpp
            debugger/ramview.cpp
            debugger/registers.cpp
            util/spinbox.cpp
            bootmanager.cpp
            hotkeys.cpp
            main.cpp
            citra-qt.rc
            )

set(HEADERS
            config/controller_config.hxx
            config/controller_config_util.hxx
            config.h
            debugger/callstack.hxx
            debugger/disassembler.hxx
            debugger/graphics.hxx
            debugger/graphics_breakpoints.hxx
            debugger/graphics_breakpoints_p.hxx
            debugger/graphics_cmdlists.hxx
            debugger/graphics_framebuffer.hxx
            debugger/ramview.hxx
            debugger/registers.hxx
            util/spinbox.hxx
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

if (UNIX)
    target_link_libraries(citra-qt -pthread)
endif()

if (APPLE)
    target_link_libraries(citra-qt iconv ${COREFOUNDATION_LIBRARY})
elseif (WIN32)
    target_link_libraries(citra-qt winmm wsock32 ws2_32)
else() # Unix
    target_link_libraries(citra-qt rt)
endif()

#install(TARGETS citra-qt RUNTIME DESTINATION ${bindir})

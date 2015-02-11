set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(SRCS
            config/controller_config.cpp
            config/controller_config_util.cpp
            config.cpp
            debugger/callstack.cpp
            debugger/disassembler.cpp
            debugger/graphics.cpp
            debugger/graphics_breakpoint_observer.cpp
            debugger/graphics_breakpoints.cpp
            debugger/graphics_cmdlists.cpp
            debugger/graphics_framebuffer.cpp
            debugger/graphics_vertex_shader.cpp
            debugger/ramview.cpp
            debugger/registers.cpp
            util/spinbox.cpp
            bootmanager.cpp
            hotkeys.cpp
            main.cpp
            citra-qt.rc
            )

set(HEADERS
            config/controller_config.h
            config/controller_config_util.h
            config.h
            debugger/callstack.h
            debugger/disassembler.h
            debugger/graphics.h
            debugger/graphics_breakpoint_observer.h
            debugger/graphics_breakpoints.h
            debugger/graphics_breakpoints_p.h
            debugger/graphics_cmdlists.h
            debugger/graphics_framebuffer.h
            debugger/graphics_vertex_shader.h
            debugger/ramview.h
            debugger/registers.h
            util/spinbox.h
            bootmanager.h
            hotkeys.h
            main.h
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
target_link_libraries(citra-qt ${PLATFORM_LIBRARIES})

#install(TARGETS citra-qt RUNTIME DESTINATION ${bindir})

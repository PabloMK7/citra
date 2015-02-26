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
            debugger/profiler.cpp
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
            debugger/profiler.h
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
            debugger/profiler.ui
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

if (APPLE)
    add_executable(citra-qt MACOSX_BUNDLE ${SRCS} ${HEADERS} ${UI_HDRS})
else()
    add_executable(citra-qt ${SRCS} ${HEADERS} ${UI_HDRS})
endif()
target_link_libraries(citra-qt core common video_core qhexedit)
target_link_libraries(citra-qt ${OPENGL_gl_LIBRARY} ${CITRA_QT_LIBS})
target_link_libraries(citra-qt ${PLATFORM_LIBRARIES})

#install(TARGETS citra-qt RUNTIME DESTINATION ${bindir})

# setup a post build to copy the Qt Dlls to the right place on Windows MSVC
# I only have Qt 5 so if someone wants to add this for Qt 4 as well that would be great
if (Qt5_FOUND)
    set(Qt5_DLL_DIR "${Qt5_DIR}/../../../bin")
    file(GLOB Qt5_DEBUG_DLLS
        "${Qt5_DLL_DIR}/icudt*.dll"
        "${Qt5_DLL_DIR}/icuin*.dll"
        "${Qt5_DLL_DIR}/icuuc*.dll"
        "${Qt5_DLL_DIR}/Qt5Cored.*"
        "${Qt5_DLL_DIR}/Qt5Guid.*"
        "${Qt5_DLL_DIR}/Qt5OpenGLd.*"
        "${Qt5_DLL_DIR}/Qt5Widgetsd.*"
    )
    foreach(Dll ${Qt5_DEBUG_DLLS})
        # the if not exist skips this copy if the build directory doesn't exist so it doesn't error out
        add_custom_command(TARGET citra-qt POST_BUILD
                           COMMAND if not exist "${CMAKE_BINARY_DIR}/bin/Debug/" goto :cmEnd
                           COMMAND ${CMAKE_COMMAND} -E
                             copy ${Dll} "${CMAKE_BINARY_DIR}/bin/Debug/")
    endforeach()
    # sooo here's a fun struggle. I can't set a custom command for a single build target,
    # so instead I have to add all these to every configuration in VS anyway.
    file(GLOB Qt5_RELEASE_DLLS
        "${Qt5_DLL_DIR}/icudt*.dll"
        "${Qt5_DLL_DIR}/icuin*.dll"
        "${Qt5_DLL_DIR}/icuuc*.dll"
        "${Qt5_DLL_DIR}/Qt5Core.*"
        "${Qt5_DLL_DIR}/Qt5Gui.*"
        "${Qt5_DLL_DIR}/Qt5OpenGL.*"
        "${Qt5_DLL_DIR}/Qt5Widgets.*"
    )
    foreach(Dll ${Qt5_RELEASE_DLLS})
        add_custom_command(TARGET citra-qt POST_BUILD
                           COMMAND if not exist "${CMAKE_BINARY_DIR}/bin/Release/" goto :cmEnd
                           COMMAND ${CMAKE_COMMAND} -E
                             copy ${Dll} "${CMAKE_BINARY_DIR}/bin/Release/")
        add_custom_command(TARGET citra-qt POST_BUILD
                           COMMAND if not exist "${CMAKE_BINARY_DIR}/bin/RelWithDebInfo/" goto :cmEnd
                           COMMAND ${CMAKE_COMMAND} -E
                             copy ${Dll} "${CMAKE_BINARY_DIR}/bin/RelWithDebInfo/")
    endforeach()
    unset(Qt5_DLL_DIR)
endif()

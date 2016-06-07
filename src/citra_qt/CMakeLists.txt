set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(SRCS
            config.cpp
            debugger/callstack.cpp
            debugger/disassembler.cpp
            debugger/graphics.cpp
            debugger/graphics_breakpoint_observer.cpp
            debugger/graphics_breakpoints.cpp
            debugger/graphics_cmdlists.cpp
            debugger/graphics_surface.cpp
            debugger/graphics_tracing.cpp
            debugger/graphics_vertex_shader.cpp
            debugger/profiler.cpp
            debugger/ramview.cpp
            debugger/registers.cpp
            util/spinbox.cpp
            util/util.cpp
            bootmanager.cpp
            configure_audio.cpp
            configure_debug.cpp
            configure_dialog.cpp
            configure_general.cpp
            game_list.cpp
            hotkeys.cpp
            main.cpp
            ui_settings.cpp
            citra-qt.rc
            Info.plist
            )

set(HEADERS
            config.h
            debugger/callstack.h
            debugger/disassembler.h
            debugger/graphics.h
            debugger/graphics_breakpoint_observer.h
            debugger/graphics_breakpoints.h
            debugger/graphics_breakpoints_p.h
            debugger/graphics_cmdlists.h
            debugger/graphics_surface.h
            debugger/graphics_tracing.h
            debugger/graphics_vertex_shader.h
            debugger/profiler.h
            debugger/ramview.h
            debugger/registers.h
            util/spinbox.h
            util/util.h
            bootmanager.h
            configure_audio.h
            configure_debug.h
            configure_dialog.h
            configure_general.h
            game_list.h
            game_list_p.h
            hotkeys.h
            main.h
            ui_settings.h
            version.h
            )

set(UIS
            debugger/callstack.ui
            debugger/disassembler.ui
            debugger/profiler.ui
            debugger/registers.ui
            configure.ui
            configure_audio.ui
            configure_debug.ui
            configure_general.ui
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
    set(MACOSX_ICON "../../dist/citra.icns")
    set_source_files_properties(${MACOSX_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    add_executable(citra-qt MACOSX_BUNDLE ${SRCS} ${HEADERS} ${UI_HDRS} ${MACOSX_ICON})
    set_target_properties(citra-qt PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
else()
    add_executable(citra-qt ${SRCS} ${HEADERS} ${UI_HDRS})
endif()
target_link_libraries(citra-qt core video_core audio_core common qhexedit)
target_link_libraries(citra-qt ${OPENGL_gl_LIBRARY} ${CITRA_QT_LIBS})
target_link_libraries(citra-qt ${PLATFORM_LIBRARIES} Threads::Threads)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD|OpenBSD|NetBSD")
    install(TARGETS citra-qt RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

if (Qt5_FOUND AND MSVC)
    include(WindowsCopyFiles)

    set(Qt5_DLL_DIR "${Qt5_DIR}/../../../bin")
    set(Qt5_PLATFORMS_DIR "${Qt5_DIR}/../../../plugins/platforms/")
    set(DLL_DEST "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/")
    set(PLATFORMS ${DLL_DEST}platforms/)

    windows_copy_files(citra-qt ${Qt5_DLL_DIR} ${DLL_DEST}
        icudt*.dll
        icuin*.dll
        icuuc*.dll
        Qt5Core$<$<CONFIG:Debug>:d>.*
        Qt5Gui$<$<CONFIG:Debug>:d>.*
        Qt5OpenGL$<$<CONFIG:Debug>:d>.*
        Qt5Widgets$<$<CONFIG:Debug>:d>.*
    )
    windows_copy_files(citra-qt ${Qt5_PLATFORMS_DIR} ${PLATFORMS} qwindows$<$<CONFIG:Debug>:d>.*)

    unset(Qt5_DLL_DIR)
    unset(Qt5_PLATFORMS_DIR)
    unset(DLL_DEST)
    unset(PLATFORMS)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)

set(SRCS
            configuration/config.cpp
            configuration/configure_audio.cpp
            configuration/configure_debug.cpp
            configuration/configure_dialog.cpp
            configuration/configure_general.cpp
            configuration/configure_graphics.cpp
            configuration/configure_input.cpp
            configuration/configure_system.cpp
            configuration/configure_web.cpp
            debugger/graphics/graphics.cpp
            debugger/graphics/graphics_breakpoint_observer.cpp
            debugger/graphics/graphics_breakpoints.cpp
            debugger/graphics/graphics_cmdlists.cpp
            debugger/graphics/graphics_surface.cpp
            debugger/graphics/graphics_tracing.cpp
            debugger/graphics/graphics_vertex_shader.cpp
            debugger/profiler.cpp
            debugger/registers.cpp
            debugger/wait_tree.cpp
            util/spinbox.cpp
            util/util.cpp
            bootmanager.cpp
            game_list.cpp
            hotkeys.cpp
            main.cpp
            ui_settings.cpp
            citra-qt.rc
            Info.plist
            )

set(HEADERS
            configuration/config.h
            configuration/configure_audio.h
            configuration/configure_debug.h
            configuration/configure_dialog.h
            configuration/configure_general.h
            configuration/configure_graphics.h
            configuration/configure_input.h
            configuration/configure_system.h
            configuration/configure_web.h
            debugger/graphics/graphics.h
            debugger/graphics/graphics_breakpoint_observer.h
            debugger/graphics/graphics_breakpoints.h
            debugger/graphics/graphics_breakpoints_p.h
            debugger/graphics/graphics_cmdlists.h
            debugger/graphics/graphics_surface.h
            debugger/graphics/graphics_tracing.h
            debugger/graphics/graphics_vertex_shader.h
            debugger/profiler.h
            debugger/registers.h
            debugger/wait_tree.h
            util/spinbox.h
            util/util.h
            bootmanager.h
            game_list.h
            game_list_p.h
            hotkeys.h
            main.h
            ui_settings.h
            )

set(UIS
            configuration/configure.ui
            configuration/configure_audio.ui
            configuration/configure_debug.ui
            configuration/configure_general.ui
            configuration/configure_graphics.ui
            configuration/configure_input.ui
            configuration/configure_system.ui
            configuration/configure_web.ui
            debugger/registers.ui
            hotkeys.ui
            main.ui
            )

file(GLOB_RECURSE ICONS ${CMAKE_SOURCE_DIR}/dist/icons/*)
file(GLOB_RECURSE THEMES ${CMAKE_SOURCE_DIR}/dist/qt_themes/*)

create_directory_groups(${SRCS} ${HEADERS} ${UIS})

if (Qt5_FOUND)
    qt5_wrap_ui(UI_HDRS ${UIS})
else()
    qt4_wrap_ui(UI_HDRS ${UIS})
endif()

if (APPLE)
    set(MACOSX_ICON "../../dist/citra.icns")
    set_source_files_properties(${MACOSX_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    add_executable(citra-qt MACOSX_BUNDLE ${SRCS} ${HEADERS} ${UI_HDRS} ${ICONS} ${THEMES} ${MACOSX_ICON})
    set_target_properties(citra-qt PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
else()
    add_executable(citra-qt ${SRCS} ${HEADERS} ${UI_HDRS} ${ICONS} ${THEMES})
endif()
target_link_libraries(citra-qt PRIVATE audio_core common core input_common network video_core)
target_link_libraries(citra-qt PRIVATE Boost::boost glad nihstro-headers Qt5::OpenGL Qt5::Widgets)
target_link_libraries(citra-qt PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)

if(UNIX AND NOT APPLE)
    install(TARGETS citra-qt RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

if (MSVC)
    include(CopyCitraQt5Deps)
    include(CopyCitraSDLDeps)
    copy_citra_Qt5_deps(citra-qt)
    copy_citra_SDL_deps(citra-qt)
endif()

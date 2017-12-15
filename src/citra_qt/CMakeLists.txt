set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)

add_executable(citra-qt
    Info.plist
    aboutdialog.cpp
    aboutdialog.h
    bootmanager.cpp
    bootmanager.h
    citra-qt.rc
    configuration/config.cpp
    configuration/config.h
    configuration/configure_audio.cpp
    configuration/configure_audio.h
    configuration/configure_debug.cpp
    configuration/configure_debug.h
    configuration/configure_dialog.cpp
    configuration/configure_dialog.h
    configuration/configure_general.cpp
    configuration/configure_general.h
    configuration/configure_graphics.cpp
    configuration/configure_graphics.h
    configuration/configure_input.cpp
    configuration/configure_input.h
    configuration/configure_system.cpp
    configuration/configure_system.h
    configuration/configure_web.cpp
    configuration/configure_web.h
    debugger/graphics/graphics.cpp
    debugger/graphics/graphics.h
    debugger/graphics/graphics_breakpoint_observer.cpp
    debugger/graphics/graphics_breakpoint_observer.h
    debugger/graphics/graphics_breakpoints.cpp
    debugger/graphics/graphics_breakpoints.h
    debugger/graphics/graphics_breakpoints_p.h
    debugger/graphics/graphics_cmdlists.cpp
    debugger/graphics/graphics_cmdlists.h
    debugger/graphics/graphics_surface.cpp
    debugger/graphics/graphics_surface.h
    debugger/graphics/graphics_tracing.cpp
    debugger/graphics/graphics_tracing.h
    debugger/graphics/graphics_vertex_shader.cpp
    debugger/graphics/graphics_vertex_shader.h
    debugger/profiler.cpp
    debugger/profiler.h
    debugger/registers.cpp
    debugger/registers.h
    debugger/wait_tree.cpp
    debugger/wait_tree.h
    game_list.cpp
    game_list.h
    game_list_p.h
    hotkeys.cpp
    hotkeys.h
    main.cpp
    main.h
    ui_settings.cpp
    ui_settings.h
    updater/updater.cpp
    updater/updater.h
    updater/updater_p.h
    util/spinbox.cpp
    util/spinbox.h
    util/util.cpp
    util/util.h
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
    aboutdialog.ui
    hotkeys.ui
    main.ui
)

file(GLOB_RECURSE ICONS ${CMAKE_SOURCE_DIR}/dist/icons/*)
file(GLOB_RECURSE THEMES ${CMAKE_SOURCE_DIR}/dist/qt_themes/*)

qt5_wrap_ui(UI_HDRS ${UIS})

target_sources(citra-qt
    PRIVATE
        ${ICONS}
        ${THEMES}
        ${UI_HDRS}
        ${UIS}
)

if (APPLE)
    set(MACOSX_ICON "../../dist/citra.icns")
    set_source_files_properties(${MACOSX_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(citra-qt PRIVATE ${MACOSX_ICON})
    set_target_properties(citra-qt PROPERTIES MACOSX_BUNDLE TRUE)
    set_target_properties(citra-qt PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
endif()

create_target_directory_groups(citra-qt)

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

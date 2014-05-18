set(SRCS
            bootmanager.cpp
            debugger/callstack.cpp
            debugger/disassembler.cpp
            debugger/graphics.cpp
            debugger/graphics_cmdlists.cpp
            debugger/ramview.cpp
            debugger/registers.cpp
            hotkeys.cpp
            main.cpp
            config/controller_config.cpp
            config/controller_config_util.cpp)
set (HEADERS
            bootmanager.hxx
            debugger/callstack.hxx
            debugger/disassembler.hxx
            debugger/ramview.hxx
            debugger/registers.hxx
            hotkeys.hxx
            main.hxx
            ui_callstack.h
            ui_controller_config.h
            ui_disassembler.h
            ui_hotkeys.h
            ui_main.h
            ui_registers.h
            version.h
            config/controller_config.hxx
            config/controller_config_util.hxx)

qt4_wrap_ui(UI_HDRS
                    debugger/callstack.ui
                    debugger/disassembler.ui
                    debugger/registers.ui
                    hotkeys.ui
                    main.ui
                    config/controller_config.ui)

qt4_wrap_cpp(MOC_SRCS
                        bootmanager.hxx
                        debugger/callstack.hxx
                        debugger/disassembler.hxx
                        debugger/graphics.hxx
                        debugger/graphics_cmdlists.hxx
                        debugger/registers.hxx
                        debugger/ramview.hxx
                        hotkeys.hxx
                        main.hxx
                        config/controller_config.hxx
                        config/controller_config_util.hxx)

# add uic results to include directories
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(./)

add_executable(citra-qt ${SRCS} ${HEADERS} ${MOC_SRCS} ${UI_HDRS})
if (APPLE)
    target_link_libraries(citra-qt core common video_core qhexedit iconv ${COREFOUNDATION_LIBRARY} ${QT_LIBRARIES} ${GLEW_LIBRARY} ${OPENGL_LIBRARIES})
else()
    target_link_libraries(citra-qt core common video_core qhexedit ${QT_LIBRARIES} ${OPENGL_LIBRARIES} ${SDL2_LIBRARY} rt GLEW ${GLFW_LIBRARIES})
endif()

#install(TARGETS citra-qt RUNTIME DESTINATION ${bindir})

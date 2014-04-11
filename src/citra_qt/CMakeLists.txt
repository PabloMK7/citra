set(SRCS
            bootmanager.cpp
            callstack.cpp
            disasm.cpp
            cpu_regs.cpp
            hotkeys.cpp
            main.cpp
            ramview.cpp
            config/controller_config.cpp
            config/controller_config_util.cpp)

qt4_wrap_ui(UI_HDRS
                    callstack.ui
                    disasm.ui
                    cpu_regs.ui
                    hotkeys.ui
                    main.ui
                    config/controller_config.ui)

qt4_wrap_cpp(MOC_SRCS
                        bootmanager.hxx
                        callstack.hxx
                        disasm.hxx
                        cpu_regs.hxx
                        hotkeys.hxx
                        main.hxx
                        ramview.hxx
                        config/controller_config.hxx
                        config/controller_config_util.hxx)

# add uic results to include directories
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(./)

add_executable(citra-qt ${SRCS} ${MOC_SRCS} ${UI_HDRS})
target_link_libraries(citra-qt core common qhexedit ${QT_LIBRARIES} ${OPENGL_LIBRARIES} ${SDL2_LIBRARY} rt GLEW ${GLFW_LIBRARIES})

#install(TARGETS citra-qt RUNTIME DESTINATION ${bindir})

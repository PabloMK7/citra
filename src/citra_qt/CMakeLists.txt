set(SRCS
            src/bootmanager.cpp
            src/callstack.cpp
            src/disasm.cpp
            src/cpu_regs.cpp
            src/hotkeys.cpp
            src/main.cpp
            src/ramview.cpp
            src/config/controller_config.cpp
            src/config/controller_config_util.cpp)

qt4_wrap_ui(UI_HDRS
                    src/callstack.ui
                    src/disasm.ui
                    src/cpu_regs.ui
                    src/hotkeys.ui
                    src/main.ui
                    src/config/controller_config.ui)

qt4_wrap_cpp(MOC_SRCS
                        src/bootmanager.hxx
                        src/callstack.hxx
                        src/disasm.hxx
                        src/cpu_regs.hxx
                        src/hotkeys.hxx
                        src/main.hxx
                        src/ramview.hxx
                        src/config/controller_config.hxx
                        src/config/controller_config_util.hxx)

# add uic results to include directories
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(./src/)

add_executable(citra-qt ${SRCS} ${MOC_SRCS} ${UI_HDRS})
target_link_libraries(citra-qt core common qhexedit ${QT_LIBRARIES} ${OPENGL_LIBRARIES} ${SDL2_LIBRARY} rt GLEW ${GLFW_LIBRARIES})

#install(TARGETS citra-qt RUNTIME DESTINATION ${bindir})

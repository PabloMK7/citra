set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

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

set(HEADERS
            bootmanager.hxx
            debugger/callstack.hxx
            debugger/disassembler.hxx
            debugger/ramview.hxx
            debugger/registers.hxx
            hotkeys.hxx
            main.hxx
            version.h
            config/controller_config.hxx
            config/controller_config_util.hxx)

set(UIS
            debugger/callstack.ui
            debugger/disassembler.ui
            debugger/registers.ui
            hotkeys.ui
            main.ui
            config/controller_config.ui)

if(USE_QT5)
    qt5_wrap_ui(UI_HDRS ${UIS})
else()
    qt4_wrap_ui(UI_HDRS ${UIS})
endif()

add_executable(citra-qt ${SRCS} ${HEADERS} ${UI_HDRS})
if(APPLE)
	set(ICONV_LIBRARY iconv)
else()
	set(RT_LIBRARY rt)
endif()

target_link_libraries(citra-qt core common video_core qhexedit ${ICONV_LIBRARY} ${COREFOUNDATION_LIBRARY} ${QT_LIBRARIES} ${OPENGL_LIBRARIES} ${RT_LIBRARY} ${GLEW_LIBRARY} ${PNG_LIBRARIES})
if(USE_QT5)
    target_link_libraries(citra-qt Qt5::Gui Qt5::Widgets Qt5::OpenGL)
endif()

#install(TARGETS citra-qt RUNTIME DESTINATION ${bindir})

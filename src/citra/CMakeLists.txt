set(SRCS
            emu_window/emu_window_sdl2.cpp
            citra.cpp
            config.cpp
            citra.rc
            )
set(HEADERS
            emu_window/emu_window_sdl2.h
            config.h
            default_ini.h
            resource.h
            )

create_directory_groups(${SRCS} ${HEADERS})

include_directories(${SDL2_INCLUDE_DIR})

add_executable(citra ${SRCS} ${HEADERS})
target_link_libraries(citra core video_core audio_core common)
target_link_libraries(citra ${SDL2_LIBRARY} ${OPENGL_gl_LIBRARY} inih glad)
if (MSVC)
    target_link_libraries(citra getopt)
endif()
target_link_libraries(citra ${PLATFORM_LIBRARIES} Threads::Threads)

if(UNIX AND NOT APPLE)
    install(TARGETS citra RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

if (MSVC)
    include(WindowsCopyFiles)

    set(DLL_DEST "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/")

    windows_copy_files(citra ${SDL2_DLL_DIR} ${DLL_DEST} SDL2.dll)

    unset(DLL_DEST)
endif()

add_library(citra-android SHARED
    android_common/android_common.cpp
    android_common/android_common.h
    applets/mii_selector.cpp
    applets/mii_selector.h
    applets/swkbd.cpp
    applets/swkbd.h
    input_manager.cpp
    input_manager.h
    camera/ndk_camera.cpp
    camera/ndk_camera.h
    camera/still_image_camera.cpp
    camera/still_image_camera.h
    cheats/cheat.cpp
    cheats/cheat.h
    cheats/cheat_engine.cpp
    config.cpp
    config.h
    default_ini.h
    emu_window/emu_window.cpp
    emu_window/emu_window.h
    game_info.cpp
    game_info.h
    game_settings.cpp
    game_settings.h
    id_cache.cpp
    id_cache.h
    lodepng_image_interface.cpp
    lodepng_image_interface.h
    mic.cpp
    mic.h
    native.cpp
    native.h
    ndk_motion.cpp
    ndk_motion.h
)

target_link_libraries(citra-android PRIVATE audio_core common core input_common network)
target_link_libraries(citra-android PRIVATE android camera2ndk EGL glad inih jnigraphics lodepng log mediandk yuv)

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} citra-android)
